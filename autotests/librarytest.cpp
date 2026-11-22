// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "database.h"
#include "databasefolder.h"
#include "email.h"
#include "extractor.h"
#include "extractorpool.h"
#include "indexer.h"
#include "jsonformat.h"
#include "library.h"
#include "mailbox.h"
#include "maintenance.h"
#include "ocr.h"
#include "office.h"
#include "paths.h"
#include "rules.h"
#include "scripthost.h"
#include "scripts.h"
#include "store.h"

#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPdfWriter>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Indeks;
using namespace Qt::StringLiterals;

namespace
{
void writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
    file.write(content);
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void writePdf(const QString &path, const QString &text)
{
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A5));
    QPainter painter(&writer);
    QFont font = painter.font();
    font.setPointSize(14);
    painter.setFont(font);
    painter.drawText(QRect(0, 0, writer.width(), writer.height()), Qt::TextWordWrap, text);
}

void writeTextImage(const QString &path, const QString &text)
{
    QImage image(1400, 300, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    QFont font(u"DejaVu Sans"_s);
    font.setPixelSize(90);
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(image.rect(), Qt::AlignCenter, text);
    painter.end();
    QVERIFY(image.save(path));
}

QVariantList ids(std::initializer_list<qint64> list)
{
    QVariantList result;
    for (qint64 id : list) {
        result.append(id);
    }
    return result;
}

const QByteArray mail = "From: Ada Lovelace <ada@example.org>\r\n"
                        "To: Charles Babbage <charles@example.org>\r\n"
                        "Subject: =?UTF-8?Q?Analytical_engine_caf=C3=A9?=\r\n"
                        "Date: Mon, 02 Mar 2026 10:15:00 +0000\r\n"
                        "MIME-Version: 1.0\r\n"
                        "Content-Type: multipart/mixed; boundary=\"XX\"\r\n"
                        "\r\n"
                        "--XX\r\n"
                        "Content-Type: text/plain; charset=utf-8\r\n"
                        "\r\n"
                        "The punchcards arrived yesterday.\r\n"
                        "--XX\r\n"
                        "Content-Type: text/plain; name=\"notes.txt\"\r\n"
                        "Content-Disposition: attachment; filename=\"notes.txt\"\r\n"
                        "Content-Transfer-Encoding: base64\r\n"
                        "\r\n"
                        "Zmx1eCBjYXBhY2l0b3I=\r\n"
                        "--XX--\r\n";
}

class LibraryTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_data;
    QTemporaryDir m_docs;
    std::unique_ptr<Library> m_library;

    qint64 idOf(const QString &fileName) const
    {
        return Store::documentByUrl(QUrl::fromLocalFile(m_docs.filePath(fileName))).id;
    }
    void settle()
    {
        m_library->indexer()->waitUntilIdle();
        QCoreApplication::processEvents();
    }
    /** Has @p start begin a run of a script and returns how the run went. */
    QVariantMap reportOf(const std::function<void()> &start, bool trial = false)
    {
        QSignalSpy spy(m_library.get(), trial ? &Library::scriptTried : &Library::scriptFinished);
        start();
        if (spy.isEmpty() && !spy.wait(60000)) {
            return {};
        }
        return spy.constFirst().constFirst().toMap();
    }
    void scriptsCometoRest()
    {
        QTRY_VERIFY_WITH_TIMEOUT(!m_library->scriptsRunning(), 60000);
        QCoreApplication::processEvents();
    }
    /**
     * Reads what the tests of single file formats wrote into "formats". They run after the
     * location the other tests share is gone, so their folder is a location of its own.
     */
    void indexFormats()
    {
        QVERIFY(m_library->addSource(QUrl::fromLocalFile(m_docs.filePath(u"formats"_s))) > 0);
        settle();
    }
    QStringList tagsOf(qint64 id) const
    {
        return m_library->documentInfo(id).value(u"tags"_s).toStringList();
    }
    /** The tests of scripts bring their own documents and leave nothing of them behind. */
    void writeScriptDocuments()
    {
        writeFile(m_docs.filePath(u"mango-list.txt"_s), "Mangoes, limes and a papaya.\n");
        writeFile(m_docs.filePath(u"mango-notes.md"_s), "# Orchard\n\nThe mango trees need water.\n");
        writeFile(m_docs.filePath(u"plain.txt"_s), "Nothing to see.\n");
        m_library->rescanAll();
        settle();
    }
    void removeScriptDocuments(const QStringList &tags)
    {
        scriptsCometoRest();
        // Without their tags, notes and the like, so that nothing keeps them once the files are gone.
        for (const QString &name : {u"mango-list.txt"_s, u"mango-notes.md"_s, u"plain.txt"_s, u"mango-cake.txt"_s}) {
            const qint64 id = idOf(name);
            if (id == 0) {
                continue;
            }
            m_library->setComment(id, QString());
            m_library->setFlagged(ids({id}), false);
            const QVariantList notes = m_library->notes(id);
            for (const QVariant &note : notes) {
                m_library->removeNote(note.toMap().value(u"id"_s).toLongLong());
            }
            QFile::remove(m_docs.filePath(name));
        }
        const QVariantList allTags = m_library->tags();
        for (const QVariant &tag : allTags) {
            if (tags.contains(tag.toMap().value(u"name"_s).toString())) {
                m_library->deleteTag(tag.toMap().value(u"id"_s).toLongLong());
            }
        }
        const QVariantList groups = m_library->groups();
        for (const QVariant &group : groups) {
            if (group.toMap().value(u"name"_s).toString() == "Kitchen"_L1) {
                m_library->deleteGroup(group.toMap().value(u"id"_s).toLongLong());
            }
        }
        m_library->rescanAll();
        settle();
        QCOMPARE(m_library->count(u"mango"_s), 0);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_data.isValid() && m_docs.isValid());
        qputenv("INDEKS_DATA_DIR", m_data.path().toUtf8());

        writeFile(m_docs.filePath(u"shopping.txt"_s), "Bananas, oat milk and a pineapple.\n");
        writeFile(m_docs.filePath(u"notes.md"_s), "# Meeting\n\nDiscussed the *quarterly* budget.\n");
        writeFile(m_docs.filePath(u"page.html"_s), "<html><head><title>T</title><style>p{color:red}</style></head><body><p>Hello <b>zeppelin</b> world</p></body></html>");
        writeFile(m_docs.filePath(u"letter.rtf"_s), "{\\rtf1\\ansi{\\fonttbl{\\f0 Arial;}}\\f0 Dear \\b Ms. Rutherford\\b0 ,\\par caf\\'e9 \\u8364? 5}");
        writeFile(m_docs.filePath(u"message.eml"_s), mail);
        writePdf(m_docs.filePath(u"report.pdf"_s), u"Annual report about hydroelectric turbines"_s);
        QDir(m_docs.path()).mkdir(u"sub"_s);
        writeFile(m_docs.filePath(u"sub/deep.txt"_s), "A file in a subfolder mentions xylophone.\n");
        writeFile(m_docs.filePath(u"backup.txt~"_s), "ignored backup\n");

        m_library = std::make_unique<Library>();
        IndexOptions options;
        options.ocrEnabled = Ocr::isAvailable();
        m_library->setOptions(options);
        m_library->start(false);
    }

    void cleanupTestCase()
    {
        m_library.reset();
        Database::closeConnection();
    }

    void extractsRtf()
    {
        const QString text = rtfToPlainText(readFile(m_docs.filePath(u"letter.rtf"_s)));
        QCOMPARE(text, u"Dear Ms. Rutherford,\ncafé € 5"_s);
    }

    void readsEmail()
    {
        const QList<Email> emails = readEmails(m_docs.filePath(u"message.eml"_s));
        QCOMPARE(emails.size(), 1);
        QCOMPARE(emails.first().subject, u"Analytical engine café"_s);
        QVERIFY(emails.first().from.contains("ada@example.org"_L1));
        QCOMPARE(emails.first().textBody, u"The punchcards arrived yesterday."_s);
        QCOMPARE(emails.first().attachments.size(), 1);
        QCOMPARE(emails.first().attachments.first().name, u"notes.txt"_s);
        QCOMPARE(readEmailAttachment(m_docs.filePath(u"message.eml"_s), 0, 0), QByteArray("flux capacitor"));
    }

    void indexesFolder()
    {
        const qint64 source = m_library->addSource(QUrl::fromLocalFile(m_docs.path()));
        QVERIFY(source > 0);
        settle();

        QCOMPARE(m_library->count(QString()), 7); // the backup file is skipped
        QCOMPARE(m_library->count(u"pineapple"_s), 1);
        QCOMPARE(m_library->count(u"pine"_s), 1); // prefix
        QCOMPARE(m_library->count(u"hopp"_s), 1); // substring of the file name
        QCOMPARE(m_library->count(u"zeppelin"_s), 1);
        QCOMPARE(m_library->count(u"color"_s), 0); // style sheets are not content
        QCOMPARE(m_library->count(u"Rutherford"_s), 1);
        QCOMPARE(m_library->count(u"punchcards"_s), 1);
        QCOMPARE(m_library->count(u"\"Analytical engine\""_s), 1);
        QCOMPARE(m_library->count(u"turbines"_s), 1);
        QCOMPARE(m_library->count(u"xylophone"_s), 1);
        QCOMPARE(m_library->count(u"kind:pdf"_s), 1);
        QCOMPARE(m_library->count(u"kind:email"_s), 1);
        QCOMPARE(m_library->count(u"kind:rich"_s), 2);
        QCOMPARE(m_library->count(u"ext:md"_s), 1);
        QCOMPARE(m_library->count(u"pineapple OR zeppelin"_s), 2);
        QCOMPARE(m_library->count(u"-pineapple"_s), 6);
        QCOMPARE(m_library->count(u"is:inbox"_s), 0); // indexed locations do not flood the inbox
        QCOMPARE(m_library->count(u"added:today"_s), 7);

        const QVariantMap info = m_library->documentInfo(idOf(u"report.pdf"_s));
        QCOMPARE(info.value(u"kind"_s).toInt(), int(Kind::Pdf));
        QCOMPARE(info.value(u"pages"_s).toInt(), 1);
        QCOMPARE(info.value(u"contentPath"_s).toString(), m_docs.filePath(u"report.pdf"_s));

        const QVariantList sources = m_library->sources();
        QCOMPARE(sources.size(), 1);
        QCOMPARE(sources.first().toMap().value(u"count"_s).toInt(), 7);
        QVERIFY(sources.first().toMap().value(u"available"_s).toBool());
    }

    void tagsAndGroups()
    {
        const qint64 shopping = idOf(u"shopping.txt"_s);
        const qint64 notes = idOf(u"notes.md"_s);
        m_library->addTag(ids({shopping, notes}), u"Household"_s);
        m_library->addTag(ids({shopping}), u"two words"_s);
        QCOMPARE(m_library->count(u"tag:household"_s), 2); // tags ignore case
        QCOMPARE(m_library->count(u"tag:\"two words\""_s), 1);
        QCOMPARE(m_library->count(u"is:untagged"_s), 5);
        QCOMPARE(m_library->tags().size(), 2);
        m_library->removeTag(ids({notes}), u"household"_s);
        QCOMPARE(m_library->count(u"tag:Household"_s), 1);

        const qint64 parent = m_library->createGroup(u"Projects"_s);
        const qint64 child = m_library->createGroup(u"Garden"_s, parent);
        QVERIFY(!m_library->moveGroup(parent, child)); // no cycles
        m_library->addToGroup(ids({notes}), child);
        QCOMPARE(m_library->count(u"groupid:%1"_s.arg(child)), 1);
        QCOMPARE(m_library->count(u"group:garden"_s), 1);
        const QVariantList groups = m_library->groups();
        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups.at(1).toMap().value(u"path"_s).toString(), u"Projects / Garden"_s);
        QCOMPARE(groups.at(1).toMap().value(u"depth"_s).toInt(), 1);
        m_library->deleteGroup(parent);
        QVERIFY(m_library->groups().isEmpty());
        QVERIFY(Store::document(notes).isValid()); // deleting a group keeps its documents
    }

    void movedFileKeepsItsTags()
    {
        const qint64 before = idOf(u"shopping.txt"_s);
        QVERIFY(QFile::rename(m_docs.filePath(u"shopping.txt"_s), m_docs.filePath(u"sub/groceries.txt"_s)));
        m_library->rescanAll();
        settle();
        QCOMPARE(idOf(u"sub/groceries.txt"_s), before);
        QCOMPARE(idOf(u"shopping.txt"_s), 0);
        QCOMPARE(m_library->count(u"tag:household"_s), 1);
        QCOMPARE(m_library->count(QString()), 7);
        QCOMPARE(m_library->count(u"groceries"_s), 1);
        QCOMPARE(m_library->count(u"name:shopping"_s), 0);
    }

    void deletedFiles()
    {
        // A document whose file is gone is not dropped: it goes into the trash, where a
        // file that was deleted by accident is noticed. With nothing attached to it ...
        const qint64 deep = idOf(u"sub/deep.txt"_s);
        const qint64 groceries = idOf(u"sub/groceries.txt"_s);
        const QByteArray deepText = readFile(m_docs.filePath(u"sub/deep.txt"_s));
        QVERIFY(deep > 0 && groceries > 0);
        QVERIFY(QFile::remove(m_docs.filePath(u"sub/deep.txt"_s)));
        // ... and with a tag, which goes along.
        QVERIFY(QFile::remove(m_docs.filePath(u"sub/groceries.txt"_s)));
        QSignalSpy messages(m_library.get(), &Library::message);
        m_library->rescanAll();
        settle();
        QCOMPARE(m_library->count(u"xylophone"_s), 0);
        QCOMPARE(m_library->count(u"pineapple"_s), 0);
        QCOMPARE(m_library->count(QString()), 5);
        QCOMPARE(m_library->count(u"is:trashed"_s), 2);
        QCOMPARE(m_library->count(u"is:trashed is:missing"_s), 2);
        QCOMPARE(m_library->count(u"is:trashed xylophone"_s), 1); // what it said is still known
        QCOMPARE(m_library->count(u"is:trashed tag:household"_s), 1);
        QVERIFY(std::any_of(messages.cbegin(), messages.cend(), [](const QList<QVariant> &arguments) {
            return arguments.constFirst().toString().contains(u"in the trash"_s);
        }));
        // Another scan changes nothing about them.
        m_library->rescanAll();
        settle();
        QCOMPARE(m_library->count(u"is:trashed"_s), 2);

        // The file comes back, restored from a backup say, and the document with it.
        writeFile(m_docs.filePath(u"sub/deep.txt"_s), deepText);
        m_library->rescanAll();
        settle();
        QCOMPARE(idOf(u"sub/deep.txt"_s), deep);
        QVERIFY(!Store::document(deep).trashed);
        QVERIFY(!Store::document(deep).missing);
        QCOMPARE(m_library->count(u"xylophone"_s), 1);
        QVERIFY(!Store::document(deep).inbox); // it is not new

        // Gone again. There is nothing to restore by hand, and the document says so ...
        QVERIFY(QFile::remove(m_docs.filePath(u"sub/deep.txt"_s)));
        m_library->rescanAll();
        settle();
        messages.clear();
        m_library->restoreFromTrash(ids({deep}));
        QVERIFY(Store::document(deep).trashed);
        QCOMPARE(int(messages.size()), 1);
        // ... while one with a tag comes back as a document whose file is missing, and stays.
        m_library->restoreFromTrash(ids({groceries}));
        QVERIFY(!Store::document(groceries).trashed);
        QCOMPARE(m_library->count(u"is:missing"_s), 1);
        QCOMPARE(m_library->count(u"pineapple"_s), 1); // still searchable
        m_library->rescanAll();
        settle();
        QVERIFY(!Store::document(groceries).trashed);

        // Emptying the trash forgets the other for good.
        m_library->emptyTrash();
        QVERIFY(!Store::document(deep).isValid());
        QCOMPARE(m_library->count(u"is:trashed"_s), 0);
        QCOMPARE(m_library->count(QString()), 6);
    }

    void changedFileIsReindexed()
    {
        writeFile(m_docs.filePath(u"notes.md"_s), "# Meeting\n\nNow about the kaleidoscope.\n");
        m_library->rescanAll();
        settle();
        QCOMPARE(m_library->count(u"kaleidoscope"_s), 1);
        QCOMPARE(m_library->count(u"quarterly"_s), 0);
    }

    void editingNeverTouchesTheOriginal()
    {
        const QString original = m_docs.filePath(u"notes.md"_s);
        const QByteArray before = readFile(original);
        const qint64 id = idOf(u"notes.md"_s);

        QVERIFY(m_library->saveText(id, u"Rewritten with a marmalade recipe."_s));
        QCOMPARE(readFile(original), before);
        const QVariantMap info = m_library->documentInfo(id);
        QVERIFY(info.value(u"hasLocalCopy"_s).toBool());
        const QString copy = info.value(u"contentPath"_s).toString();
        QVERIFY(copy.startsWith(Paths::copiesDir()));
        QCOMPARE(m_library->readText(id), u"Rewritten with a marmalade recipe."_s);
        QCOMPARE(m_library->count(u"marmalade"_s), 1);

        // A rescan keeps indexing the working copy.
        m_library->rescanAll();
        settle();
        QCOMPARE(m_library->count(u"marmalade"_s), 1);

        m_library->discardLocalCopy(id);
        settle();
        QVERIFY(!QFile::exists(copy));
        QCOMPARE(m_library->count(u"marmalade"_s), 0);
        QCOMPARE(m_library->count(u"kaleidoscope"_s), 1);
    }

    void drawingCreatesACopy()
    {
        const qint64 id = idOf(u"report.pdf"_s);
        QVERIFY(!m_library->documentInfo(id).value(u"hasLocalCopy"_s).toBool());
        m_library->setInk(id, 0, u"[{\"color\":\"#ff0000\",\"width\":0.004,\"points\":[0.1,0.1,0.2,0.2]}]"_s);
        QVERIFY(m_library->documentInfo(id).value(u"hasLocalCopy"_s).toBool());
        QVERIFY(m_library->hasInk(id));
        QVERIFY(m_library->ink(id, 0).contains("ff0000"_L1));
        QCOMPARE(m_library->count(u"is:annotated"_s), 1);
        m_library->setInk(id, 0, u"[]"_s);
        QVERIFY(!m_library->hasInk(id));
    }

    void notesAreSearchable()
    {
        const qint64 id = idOf(u"page.html"_s);
        const qint64 note = m_library->addNote(id, u"Remember the gyroscope"_s);
        QVERIFY(note > 0);
        m_library->setComment(id, u"filed under ornithology"_s);
        QCOMPARE(m_library->count(u"gyroscope"_s), 1);
        QCOMPARE(m_library->count(u"note:ornithology"_s), 1);
        QCOMPARE(m_library->count(u"content:gyroscope"_s), 0);
        QCOMPARE(m_library->notes(id).size(), 1);
        m_library->removeNote(note);
        QCOMPARE(m_library->count(u"gyroscope"_s), 0);
        // Notes survive re-indexing of the file.
        m_library->contentSaved(id);
        QCOMPARE(m_library->count(u"note:ornithology"_s), 1);
    }

    void notesStickToPlacesOnPages()
    {
        const qint64 id = idOf(u"report.pdf"_s);
        QVERIFY(id > 0);
        // One for the document as a whole, two stuck to places on pages.
        const qint64 general = m_library->addNote(id, u"About hydro power"_s);
        const qint64 second = m_library->addNote(id, u"Check this figure"_s, 1, 0.25, 0.8);
        const qint64 first = m_library->addNote(id, u"Title is misleading"_s, 0, 0.5, 0.1);
        QVERIFY(general > 0 && first > 0 && second > 0);

        // Listed in the order of the pages; the general one, which has no page, first.
        QVariantList notes = m_library->notes(id);
        QCOMPARE(notes.size(), 3);
        QCOMPARE(notes.at(0).toMap().value(u"id"_s).toLongLong(), general);
        QCOMPARE(notes.at(0).toMap().value(u"page"_s).toInt(), -1);
        QVERIFY(notes.at(0).toMap().value(u"x"_s).toDouble() < 0);
        QCOMPARE(notes.at(1).toMap().value(u"id"_s).toLongLong(), first);
        QCOMPARE(notes.at(2).toMap().value(u"page"_s).toInt(), 1);
        QCOMPARE(notes.at(2).toMap().value(u"x"_s).toDouble(), 0.25);
        QCOMPARE(notes.at(2).toMap().value(u"y"_s).toDouble(), 0.8);

        // A page without a place on it is no place to stick a note to.
        const qint64 loose = m_library->addNote(id, u"Somewhere on page three"_s, 2);
        QCOMPARE(m_library->notes(id).at(1).toMap().value(u"page"_s).toInt(), -1);
        m_library->removeNote(loose);

        // Moved, it stays on its page, and never leaves the page.
        QSignalSpy changed(m_library.get(), &Library::documentChanged);
        m_library->moveNote(second, 0.6, 1.7);
        QVERIFY(!changed.isEmpty());
        notes = m_library->notes(id);
        QCOMPARE(notes.at(2).toMap().value(u"page"_s).toInt(), 1);
        QCOMPARE(notes.at(2).toMap().value(u"x"_s).toDouble(), 0.6);
        QCOMPARE(notes.at(2).toMap().value(u"y"_s).toDouble(), 1.0);

        // What is written on them is found like any note.
        QCOMPARE(m_library->count(u"note:misleading"_s), 1);
        for (qint64 note : {general, first, second}) {
            m_library->removeNote(note);
        }
        QVERIFY(m_library->notes(id).isEmpty());
        QCOMPARE(m_library->count(u"note:misleading"_s), 0);
    }

    void createsDocumentsInItsOwnFolder()
    {
        QSignalSpy created(m_library.get(), &Library::documentCreated);
        const qint64 id = m_library->createDocument(u"Idea"_s, u"markdown"_s, u"A trampoline for cats"_s);
        QVERIFY(id > 0);
        QCOMPARE(created.size(), 1);
        const QVariantMap info = m_library->documentInfo(id);
        QCOMPARE(info.value(u"name"_s).toString(), u"Idea.md"_s);
        QVERIFY(info.value(u"owned"_s).toBool());
        QVERIFY(info.value(u"inbox"_s).toBool());
        QVERIFY(info.value(u"contentPath"_s).toString().startsWith(Paths::filesDir()));
        QCOMPARE(m_library->count(u"is:inbox trampoline"_s), 1);

        // A second document of the same name does not overwrite the first.
        const qint64 second = m_library->createDocument(u"Idea"_s, u"markdown"_s);
        QCOMPARE(m_library->documentInfo(second).value(u"name"_s).toString(), u"Idea 2.md"_s);

        QVERIFY(m_library->renameDocument(id, u"Cat trampoline.md"_s));
        QCOMPARE(m_library->count(u"name:\"Cat trampoline\""_s), 1);
        QVERIFY(!m_library->renameDocument(idOf(u"page.html"_s), u"other.html"_s)); // not ours to rename
        QVERIFY(QFile::exists(m_docs.filePath(u"page.html"_s)));

        const qint64 group = m_library->createGroup(u"Ideas"_s);
        m_library->addToGroup(ids({id}), group);
        QVERIFY(!m_library->documentInfo(id).value(u"inbox"_s).toBool()); // filing empties the inbox
    }

    void takesFoldersNotSingleFiles()
    {
        QTemporaryDir elsewhere;
        const QUrl loose = QUrl::fromLocalFile(elsewhere.filePath(u"loose.txt"_s));
        writeFile(loose.toLocalFile(), "A loose leaf about a submarine.\n");
        writeFile(elsewhere.filePath(u"second.txt"_s), "Another one.\n");

        // A file by itself is turned down, with a word on what to do instead.
        QSignalSpy told(m_library.get(), &Library::message);
        QCOMPARE(m_library->addFolders({loose}), 0);
        settle();
        QCOMPARE(told.size(), 1);
        QVERIFY(told.constFirst().constFirst().toString().contains(u"loose.txt"_s));
        QVERIFY(!Store::documentByUrl(loose).isValid());
        QCOMPARE(m_library->count(u"submarine"_s), 0);

        // Several are turned down in one go, and what is not there is said to be not there.
        told.clear();
        QCOMPARE(m_library->addFolders({loose, QUrl::fromLocalFile(elsewhere.filePath(u"second.txt"_s)), QUrl::fromLocalFile(elsewhere.filePath(u"nothing"_s))}), 0);
        settle();
        QCOMPARE(told.size(), 2);
        QCOMPARE(m_library->count(u"submarine"_s), 0);

        // The folder it is in is what Indeks takes: it becomes a location, the file with it.
        told.clear();
        const qint64 location = m_library->addFolders({loose, QUrl::fromLocalFile(elsewhere.path())});
        QVERIFY(location > 0);
        settle();
        QCOMPARE(told.size(), 1); // about the file that was named beside the folder
        QCOMPARE(Store::documentByUrl(loose).sourceId, location);
        QCOMPARE(m_library->count(u"submarine"_s), 1);
        // Naming it again adds no second location.
        QCOMPARE(m_library->addFolders({QUrl::fromLocalFile(elsewhere.path())}), location);
        settle();

        m_library->removeSource(location);
        settle();
        QVERIFY(!Store::documentByUrl(loose).isValid());
        QVERIFY(QFile::exists(loose.toLocalFile())); // the file itself is left alone
    }

    void rulesRunOnImport()
    {
        const qint64 group = m_library->createGroup(u"Receipts"_s);
        const qint64 rule = m_library->saveRule({
            {u"name"_s, u"File receipts"_s},
            {u"query"_s, u"receipt kind:text"_s},
            {u"actions"_s,
             QVariantList{
                 QVariantMap{{u"type"_s, u"tag"_s}, {u"name"_s, u"money"_s}},
                 QVariantMap{{u"type"_s, u"tag"_s}, {u"name"_s, u"2026"_s}},
                 QVariantMap{{u"type"_s, u"group"_s}, {u"id"_s, group}},
                 QVariantMap{{u"type"_s, u"flag"_s}},
             }},
        });
        QVERIFY(rule > 0);
        writeFile(m_docs.filePath(u"receipt-bakery.txt"_s), "Receipt for three croissants.\n");
        writeFile(m_docs.filePath(u"unrelated.txt"_s), "Nothing to see.\n");
        m_library->rescanAll();
        settle();

        const qint64 id = idOf(u"receipt-bakery.txt"_s);
        const QVariantMap info = m_library->documentInfo(id);
        QCOMPARE(info.value(u"tags"_s).toStringList(), QStringList({u"2026"_s, u"money"_s}));
        QVERIFY(info.value(u"flagged"_s).toBool());
        QCOMPARE(m_library->count(u"groupid:%1"_s.arg(group)), 1);
        QVERIFY(m_library->documentInfo(idOf(u"unrelated.txt"_s)).value(u"tags"_s).toStringList().isEmpty());

        // Run by hand it also reaches documents that were there before.
        m_library->saveRule({{u"id"_s, rule},
                             {u"name"_s, u"File receipts"_s},
                             {u"query"_s, u"croissants OR turbines"_s},
                             {u"filters"_s, QVariantList{QVariantMap{{u"type"_s, u"words"_s}, {u"query"_s, u"croissants OR turbines"_s}}}},
                             {u"actions"_s, QVariantList{QVariantMap{{u"type"_s, u"tag"_s}, {u"name"_s, u"seen"_s}}}}});
        QCOMPARE(m_library->applyRule(rule), 2);
        QCOMPARE(m_library->count(u"tag:seen"_s), 2);
        // What the condition was put together from comes back as it was given.
        QCOMPARE(m_library->rule(rule).value(u"filters"_s).toList().constFirst().toMap().value(u"type"_s).toString(), u"words"_s);

        // Actions take back as well as give, in the order they stand in.
        m_library->saveRule({{u"id"_s, rule},
                             {u"name"_s, u"File receipts"_s},
                             {u"query"_s, u"croissants"_s},
                             {u"actions"_s,
                              QVariantList{
                                  QVariantMap{{u"type"_s, u"untag"_s}, {u"name"_s, u"seen"_s}},
                                  QVariantMap{{u"type"_s, u"unflag"_s}},
                                  QVariantMap{{u"type"_s, u"ungroup"_s}, {u"id"_s, group}},
                                  QVariantMap{{u"type"_s, u"nonsense"_s}},
                              }}});
        QCOMPARE(m_library->rule(rule).value(u"actions"_s).toList().size(), 3);
        QCOMPARE(m_library->applyRule(rule), 1);
        QCOMPARE(m_library->count(u"tag:seen"_s), 1);
        QVERIFY(!m_library->documentInfo(id).value(u"flagged"_s).toBool());
        QCOMPARE(m_library->count(u"groupid:%1"_s.arg(group)), 0);
        // A rule that does nothing is none.
        QCOMPARE(m_library->saveRule({{u"name"_s, u"Idle"_s}, {u"query"_s, u"croissants"_s}}), 0);
        m_library->deleteRule(rule);
        QVERIFY(m_library->rules().isEmpty());
    }

    void rulesCanBeKeptToTheInbox()
    {
        // A rule that sorts what comes in: its condition asks for the inbox, so it leaves
        // alone what was filed, by it or by hand, and lists only what still waits.
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        writeFile(folder.filePath(u"old-invoice.txt"_s), "An invoice for a harpsichord, long paid.\n");
        const qint64 source = m_library->addSource(QUrl::fromLocalFile(folder.path()));
        settle();
        const QUrl oldUrl = QUrl::fromLocalFile(folder.filePath(u"old-invoice.txt"_s));
        QVERIFY(!Store::documentByUrl(oldUrl).inbox); // there when the location was added

        const QString condition = u"harpsichord is:inbox"_s;
        const qint64 rule = m_library->saveRule({
            {u"name"_s, u"Sort invoices"_s},
            {u"query"_s, condition},
            {u"filters"_s,
             QVariantList{
                 QVariantMap{{u"type"_s, u"words"_s}, {u"query"_s, u"harpsichord"_s}},
                 QVariantMap{{u"type"_s, u"state"_s}, {u"query"_s, u"is:inbox"_s}},
             }},
            {u"actions"_s,
             QVariantList{
                 QVariantMap{{u"type"_s, u"tag"_s}, {u"name"_s, u"sorted"_s}},
                 QVariantMap{{u"type"_s, u"inbox"_s}},
             }},
            {u"onImport"_s, true},
        });
        QVERIFY(rule > 0);
        QCOMPARE(m_library->count(condition), 0);

        // A file that turns up in the location goes to the inbox, and the rule files it.
        writeFile(folder.filePath(u"new-invoice.txt"_s), "An invoice for tuning the harpsichord.\n");
        writeFile(folder.filePath(u"postcard.txt"_s), "Greetings from the seaside.\n");
        m_library->rescanSource(source);
        settle();
        const Document fresh = Store::documentByUrl(QUrl::fromLocalFile(folder.filePath(u"new-invoice.txt"_s)));
        QVERIFY(fresh.isValid());
        QCOMPARE(tagsOf(fresh.id), QStringList{u"sorted"_s});
        QVERIFY(!fresh.inbox);
        // What the rule is not for stays in the inbox.
        QVERIFY(Store::documentByUrl(QUrl::fromLocalFile(folder.filePath(u"postcard.txt"_s))).inbox);
        // What was there before is none of its business, also when it is run by hand.
        QVERIFY(tagsOf(Store::documentByUrl(oldUrl).id).isEmpty());
        QCOMPARE(m_library->count(condition), 0); // nothing left for the rule to show
        QCOMPARE(m_library->applyRule(rule), 0);
        QVERIFY(tagsOf(Store::documentByUrl(oldUrl).id).isEmpty());

        // Put back into the inbox by hand, a document is the rule's again.
        const qint64 old = Store::documentByUrl(oldUrl).id;
        m_library->setInbox({old}, true);
        QCOMPARE(m_library->count(condition), 1);
        QCOMPARE(m_library->applyRule(rule), 1);
        QCOMPARE(tagsOf(old), QStringList{u"sorted"_s});
        QVERIFY(!Store::document(old).inbox);

        m_library->deleteRule(rule);
        const QVariantList allTags = m_library->tags();
        for (const QVariant &tag : allTags) {
            if (tag.toMap().value(u"name"_s).toString() == "sorted"_L1) {
                m_library->deleteTag(tag.toMap().value(u"id"_s).toLongLong());
            }
        }
        m_library->removeSource(source);
        settle();
    }

    void scriptsWorkOnDocumentsThroughIndeks()
    {
        QCOMPARE(Scripts::description(u"# -*- coding: utf-8 -*-\n\n\"\"\"\nFiles receipts.\n\nMore about it.\n\"\"\"\nimport os\n"_s), u"Files receipts."_s);
        QCOMPARE(Scripts::description(u"import os\n\"\"\"Not at the top.\"\"\"\n"_s), QString());
        QCOMPARE(Scripts::fileNameFor(u"  ../Tidy  up.py "_s), u"-Tidy up.py"_s);
        QVERIFY(!Scripts::isFileName(u"../evil.py"_s));
        if (!m_library->scriptsProblem().isEmpty()) {
            QSKIP(qPrintable(m_library->scriptsProblem()));
        }

        writeScriptDocuments();
        const QString source = uR"PY("""Sorts what mentions fruit."""
import os
import socket


def process(document):
    print("looking at", document.name)
    if document.name == "mango-notes.md":
        raise ValueError("no notes today")
    if "papaya" in document.text:
        document.add_tag("fruit")
        document.add_to_group("Kitchen", "Lists")
        document.pin()
        document.comment = "%s, %d bytes, %s, added %d" % (document.kind, document.size, document.extension, document.added.year)
        document.add_note("Buy more.")
        document.add_note("Buy more.")
    if document.name == "plain.txt":
        facts = []
        try:
            open(document.path, "a").close()
            facts.append("wrote the file")
        except OSError:
            facts.append("cannot write")
        facts.append("network: " + " ".join(name for index, name in socket.if_nameindex()))
        facts.append("sees my files" if os.path.exists(os.path.expanduser("~/.config")) else "sees none of my files")
        facts.append("reads the document" if "Nothing to see" in open(document.path).read() else "cannot read the document")
        document.comment = ", ".join(facts)
)PY"_s;
        QSignalSpy changed(m_library.get(), &Library::scriptsChanged);
        QCOMPARE(m_library->saveScript(QString(), u"Fruit"_s, source), u"Fruit.py"_s);
        QVERIFY(!changed.isEmpty());
        // Scripts are files in the folder of the database, to travel with it.
        QCOMPARE(readFile(m_data.filePath(u"scripts/Fruit.py"_s)), source.toUtf8());
        QCOMPARE(m_library->scripts().size(), 1);
        const QVariantMap listed = m_library->scripts().constFirst().toMap();
        QCOMPARE(listed.value(u"name"_s).toString(), u"Fruit"_s);
        QCOMPARE(listed.value(u"description"_s).toString(), u"Sorts what mentions fruit."_s);
        QVERIFY(listed.value(u"allowed"_s).toBool());
        QCOMPARE(m_library->script(u"Fruit.py"_s).value(u"source"_s).toString(), source);
        // Another script cannot take its name.
        QCOMPARE(m_library->saveScript(QString(), u"Fruit"_s, u"pass\n"_s), QString());

        const qint64 shopping = idOf(u"mango-list.txt"_s);
        const qint64 notes = idOf(u"mango-notes.md"_s);
        const qint64 unrelated = idOf(u"plain.txt"_s);
        QVERIFY(shopping > 0 && notes > 0 && unrelated > 0);
        const QByteArray before = readFile(m_docs.filePath(u"plain.txt"_s));
        const QVariantMap report = reportOf([&] {
            m_library->runScript(u"Fruit.py"_s, ids({shopping, notes, unrelated}));
        });
        QCOMPARE(report.value(u"problem"_s).toString(), QString());
        QCOMPARE(report.value(u"documents"_s).toInt(), 3);
        QCOMPARE(report.value(u"changed"_s).toInt(), 2);
        QCOMPARE(report.value(u"failed"_s).toInt(), 1);
        scriptsCometoRest();

        // What the script asked for was done, by Indeks.
        const QVariantMap info = m_library->documentInfo(shopping);
        QVERIFY(info.value(u"tags"_s).toStringList().contains(u"fruit"_s));
        QVERIFY(info.value(u"flagged"_s).toBool());
        QCOMPARE(info.value(u"comment"_s).toString(), u"text, %1 bytes, txt, added %2"_s.arg(info.value(u"size"_s).toLongLong()).arg(QDate::currentDate().year()));
        QCOMPARE(m_library->notes(shopping).size(), 1); // the same note is not attached twice
        QCOMPARE(m_library->count(u"note:Buy"_s), 1);
        bool grouped = false;
        const QVariantList groups = m_library->groups();
        for (const QVariant &group : groups) {
            if (group.toMap().value(u"path"_s).toString() == "Kitchen / Lists"_L1) {
                grouped = m_library->count(u"groupid:%1"_s.arg(group.toMap().value(u"id"_s).toLongLong())) == 1;
            }
        }
        QVERIFY(grouped);

        // What it printed and where it failed is told, document by document.
        const QVariantList entries = report.value(u"entries"_s).toList();
        QCOMPARE(entries.size(), 3);
        QCOMPARE(entries.at(0).toMap().value(u"output"_s).toString(), u"looking at mango-list.txt\n"_s);
        const QString error = entries.at(1).toMap().value(u"error"_s).toString();
        QVERIFY2(error.contains("ValueError: no notes today"_L1) && error.contains("Fruit.py"_L1) && error.contains("line 9"_L1), qPrintable(error));
        QVERIFY(!tagsOf(notes).contains(u"fruit"_s));

        // The sandbox: the document can be read and nothing else can be done.
        QCOMPARE(m_library->documentInfo(unrelated).value(u"comment"_s).toString(), u"cannot write, network: lo, sees none of my files, reads the document"_s);
        QCOMPARE(readFile(m_docs.filePath(u"plain.txt"_s)), before);

        // Trying out what is in the editor shows what would be done and does none of it.
        const QVariantMap trial = reportOf(
            [&] {
                m_library->tryScript(u"Fruit"_s, u"def process(document):\n    document.add_tag('maybe')\n    document.remove_from_group('Kitchen', 'Lists')\n"_s, ids({shopping}));
            },
            true);
        QVERIFY(trial.value(u"trial"_s).toBool());
        const QVariantList wanted = trial.value(u"entries"_s).toList().value(0).toMap().value(u"actions"_s).toList();
        QCOMPARE(wanted.size(), 2);
        QCOMPARE(wanted.at(0).toMap().value(u"name"_s).toString(), u"maybe"_s);
        QCOMPARE(wanted.at(1).toMap().value(u"path"_s).toStringList(), QStringList({u"Kitchen"_s, u"Lists"_s}));
        scriptsCometoRest();
        QCOMPARE(m_library->count(u"tag:maybe"_s), 0);
        QCOMPARE(m_library->count(u"is:unfiled papaya"_s), 0);

        QVERIFY(QFile::remove(m_data.filePath(u"scripts/Fruit.py"_s)));
        removeScriptDocuments({u"fruit"_s});
    }

    void scriptsThatDoNotWorkAreStopped()
    {
        if (!m_library->scriptsProblem().isEmpty()) {
            QSKIP(qPrintable(m_library->scriptsProblem()));
        }
        writeScriptDocuments();
        const QVariantList documents = ids({idOf(u"mango-list.txt"_s), idOf(u"plain.txt"_s)});

        // One that never comes back.
        ScriptHost::setTimeLimit(1500);
        QCOMPARE(m_library->saveScript(QString(), u"Endless"_s, u"def process(document):\n    document.add_tag('never')\n    while True:\n        pass\n"_s), u"Endless.py"_s);
        QElapsedTimer timer;
        timer.start();
        QVariantMap report = reportOf([&] {
            m_library->runScript(u"Endless.py"_s, documents);
        });
        ScriptHost::setTimeLimit(30000);
        QVERIFY2(report.value(u"problem"_s).toString().contains("longer than"_L1), qPrintable(report.value(u"problem"_s).toString()));
        QCOMPARE(report.value(u"documents"_s).toInt(), 0);
        QVERIFY(timer.elapsed() < 15000);
        scriptsCometoRest();
        QCOMPARE(m_library->count(u"tag:never"_s), 0);

        // One that is called off by the user.
        QSignalSpy finished(m_library.get(), &Library::scriptFinished);
        m_library->runScript(u"Endless.py"_s, documents);
        QTest::qWait(500);
        m_library->stopScripts();
        QVERIFY(finished.wait(10000));
        QVERIFY(finished.constFirst().constFirst().toMap().value(u"stopped"_s).toBool());
        scriptsCometoRest();

        // One that is not Python, and one that has nothing to call.
        m_library->saveScript(u"Endless.py"_s, u"Endless"_s, u"def process(document)\n    pass\n"_s);
        report = reportOf([&] {
            m_library->runScript(u"Endless.py"_s, documents);
        });
        QVERIFY2(report.value(u"problem"_s).toString().contains("SyntaxError"_L1), qPrintable(report.value(u"problem"_s).toString()));
        m_library->saveScript(u"Endless.py"_s, u"Endless"_s, u"def other(document):\n    pass\n"_s);
        report = reportOf([&] {
            m_library->runScript(u"Endless.py"_s, documents);
        });
        QVERIFY2(report.value(u"problem"_s).toString().contains("process(document)"_L1), qPrintable(report.value(u"problem"_s).toString()));

        // One that takes the interpreter down with it.
        m_library->saveScript(u"Endless.py"_s, u"Endless"_s, u"import os\n\ndef process(document):\n    os._exit(3)\n"_s);
        report = reportOf([&] {
            m_library->runScript(u"Endless.py"_s, documents);
        });
        QVERIFY2(report.value(u"problem"_s).toString().contains("ended before"_L1), qPrintable(report.value(u"problem"_s).toString()));
        QVERIFY(QFile::remove(m_data.filePath(u"scripts/Endless.py"_s)));
        removeScriptDocuments({});
    }

    void rulesRunScripts()
    {
        if (!m_library->scriptsProblem().isEmpty()) {
            QSKIP(qPrintable(m_library->scriptsProblem()));
        }
        writeScriptDocuments();
        QCOMPARE(m_library->saveScript(QString(),
                                       u"Check"_s,
                                       u"def process(document):\n    if document.has_tag('first'):\n        document.add_tag('script saw first')\n    document.flag()\n"_s),
                 u"Check.py"_s);
        const QVariantList actions = {
            QVariantMap{{u"type"_s, u"tag"_s}, {u"name"_s, u"first"_s}},
            QVariantMap{{u"type"_s, u"script"_s}, {u"name"_s, u"Check.py"_s}},
            QVariantMap{{u"type"_s, u"unflag"_s}},
            QVariantMap{{u"type"_s, u"tag"_s}, {u"name"_s, u"last"_s}},
            QVariantMap{{u"type"_s, u"script"_s}, {u"name"_s, u"../no/such.py"_s}},
        };
        const qint64 rule = m_library->saveRule({{u"name"_s, u"Scripted"_s}, {u"query"_s, u"mango OR papaya"_s}, {u"actions"_s, actions}});
        QVERIFY(rule > 0);
        QCOMPARE(m_library->rule(rule).value(u"actions"_s).toList().size(), 4); // a script is a file in the folder for scripts, nowhere else
        QCOMPARE(m_library->scripts().constFirst().toMap().value(u"rules"_s).toStringList(), QStringList{u"Scripted"_s});

        // By hand. The script runs in its place among the other actions: it sees the tag
        // set before it, and its flag is taken off again by what comes after it.
        QSignalSpy messages(m_library.get(), &Library::message);
        QCOMPARE(m_library->applyRule(rule), -1);
        QVERIFY(messages.wait(60000));
        QVERIFY2(messages.constFirst().constFirst().toString().contains("2 documents"_L1), qPrintable(messages.constFirst().constFirst().toString()));
        scriptsCometoRest();
        for (const QString &name : {u"mango-list.txt"_s, u"mango-notes.md"_s}) {
            const QVariantMap info = m_library->documentInfo(idOf(name));
            const QStringList tags = info.value(u"tags"_s).toStringList();
            QVERIFY2(tags.contains(u"first"_s) && tags.contains(u"script saw first"_s) && tags.contains(u"last"_s), qPrintable(tags.join(u',')));
            QVERIFY(!info.value(u"flagged"_s).toBool());
        }

        // And for what comes in.
        writeFile(m_docs.filePath(u"mango-cake.txt"_s), "A mango upside-down cake.\n");
        m_library->rescanAll();
        settle();
        QTRY_VERIFY_WITH_TIMEOUT(tagsOf(idOf(u"mango-cake.txt"_s)).contains(u"last"_s), 60000);
        QVERIFY(tagsOf(idOf(u"mango-cake.txt"_s)).contains(u"script saw first"_s));
        scriptsCometoRest();

        // Renamed, the rule still finds it.
        QCOMPARE(m_library->saveScript(u"Check.py"_s, u"Checked"_s, m_library->script(u"Check.py"_s).value(u"source"_s).toString()), u"Checked.py"_s);
        QVERIFY(!QFileInfo::exists(m_data.filePath(u"scripts/Check.py"_s)));
        QCOMPARE(m_library->rule(rule).value(u"actions"_s).toList().at(1).toMap().value(u"name"_s).toString(), u"Checked.py"_s);

        // A script that is gone is said to be, and the rest of the rule is done all the same.
        QVERIFY(QFile::remove(m_data.filePath(u"scripts/Checked.py"_s)));
        m_library->removeTag(ids({idOf(u"mango-list.txt"_s)}), u"last"_s);
        const QVariantMap report = reportOf([&] {
            m_library->applyRule(rule);
        });
        QVERIFY2(report.value(u"problem"_s).toString().contains("not there any more"_L1), qPrintable(report.value(u"problem"_s).toString()));
        QCOMPARE(report.value(u"rule"_s).toString(), u"Scripted"_s);
        scriptsCometoRest();
        QVERIFY(tagsOf(idOf(u"mango-list.txt"_s)).contains(u"last"_s));

        m_library->deleteRule(rule);
        removeScriptDocuments({u"first"_s, u"last"_s, u"script saw first"_s});
    }

    void scriptsFromElsewhereHaveToBeAllowed()
    {
        if (!m_library->scriptsProblem().isEmpty()) {
            QSKIP(qPrintable(m_library->scriptsProblem()));
        }
        // What an earlier run of this test allowed is not to count. In test mode this is a
        // file of Qt's own test folder, not the user's settings.
        QVERIFY(QStandardPaths::isTestModeEnabled());
        QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/indeks-allowed-scripts"_L1);

        // A database that is not this computer's own, as on a share others can write to.
        QTemporaryDir share;
        QVERIFY(share.isValid());
        QCOMPARE(m_library->switchDatabase(share.path(), true), QString());
        const qint64 id = m_library->createDocument(u"Harbour log"_s, u"text"_s, u"The ketch left at dawn."_s);
        QVERIFY(id > 0);
        settle();

        // A script someone else put there does not run here by itself.
        QVERIFY(QDir().mkpath(share.filePath(u"scripts"_s)));
        writeFile(share.filePath(u"scripts/Foreign.py"_s), "def process(document):\n    document.add_tag('foreign')\n");
        QTRY_COMPARE(m_library->scripts().size(), 1);
        QVERIFY(!m_library->scripts().constFirst().toMap().value(u"allowed"_s).toBool());
        QVariantMap report = reportOf([&] {
            m_library->runScript(u"Foreign.py"_s, ids({id}));
        });
        QVERIFY2(report.value(u"problem"_s).toString().contains("not been allowed"_L1), qPrintable(report.value(u"problem"_s).toString()));
        scriptsCometoRest();
        QCOMPARE(m_library->count(u"tag:foreign"_s), 0);

        // Once it was read and allowed, it does.
        m_library->allowScript(u"Foreign.py"_s);
        QVERIFY(m_library->script(u"Foreign.py"_s).value(u"allowed"_s).toBool());
        report = reportOf([&] {
            m_library->runScript(u"Foreign.py"_s, ids({id}));
        });
        QCOMPARE(report.value(u"problem"_s).toString(), QString());
        scriptsCometoRest();
        QCOMPARE(m_library->count(u"tag:foreign"_s), 1);

        // That goes for the text as it was then. Changed elsewhere, it is a stranger again;
        // changed here, it is not.
        writeFile(share.filePath(u"scripts/Foreign.py"_s), "def process(document):\n    document.add_tag('changed elsewhere')\n");
        QVERIFY(!m_library->script(u"Foreign.py"_s).value(u"allowed"_s).toBool());
        QCOMPARE(m_library->saveScript(u"Foreign.py"_s, u"Foreign"_s, u"def process(document):\n    document.add_tag('changed here')\n"_s), u"Foreign.py"_s);
        QVERIFY(m_library->script(u"Foreign.py"_s).value(u"allowed"_s).toBool());

        QCOMPARE(m_library->switchDatabase(QString()), QString());
        QVERIFY(m_library->scripts().isEmpty());
    }

    void mailboxesAreTakenApart()
    {
        const QByteArray first = "From ada@example.org Mon Mar  2 10:15:00 2026\n"
                                 "From: Ada Lovelace <ada@example.org>\n"
                                 "To: Charles Babbage <charles@example.org>\n"
                                 "Subject: Difference engine\n"
                                 "Date: Mon, 02 Mar 2026 10:15:00 +0000\n"
                                 "\n"
                                 "The cogwheels arrived.\n"
                                 "\n"
                                 ">From what I hear they are of brass.\n"
                                 "\n"
                                 "From here on it gets technical.\n"
                                 "\n";
        const QByteArray second = "From charles@example.org Tue Mar  3 08:00:00 2026\n"
                                  "From: Charles Babbage <charles@example.org>\n"
                                  "Subject: Re: Difference engine\n"
                                  "Date: Tue, 03 Mar 2026 08:00:00 +0000\n"
                                  "\n"
                                  "Splendid. Bring the quadrant too.\n"
                                  "\n";
        const QByteArray third = "From grace@example.org Wed Mar  4 09:30:00 2026\n"
                                 "From: Grace Hopper <grace@example.org>\n"
                                 "Date: Wed, 04 Mar 2026 09:30:00 +0000\n"
                                 "\n"
                                 "A moth was found in the relay.\n"
                                 "\n";
        const QString path = m_docs.filePath(u"archive.mbox"_s);
        writeFile(path, first + second + third);

        // Where the messages stand is found in one pass, and each can be read by itself.
        const QList<Mailbox::Entry> entries = Mailbox::scan(path);
        QCOMPARE(entries.size(), 3);
        QCOMPARE(entries.at(0).offset, qint64(first.indexOf('\n') + 1));
        QCOMPARE(entries.at(1).offset, qint64(first.size() + second.indexOf('\n') + 1));
        const QByteArray message = Mailbox::readMessage(path, entries.at(0).offset, entries.at(0).length);
        QVERIFY(message.startsWith("From: Ada Lovelace"));
        QVERIFY(message.contains("\nFrom what I hear")); // the mark it was given is gone
        QVERIFY(message.contains("\nFrom here on it gets technical")); // text, not another message
        QVERIFY(!message.contains("Splendid"));

        const int before = m_library->count(QString());
        m_library->rescanAll();
        settle();
        const qint64 mailbox = idOf(u"archive.mbox"_s);
        QVERIFY(mailbox > 0);

        // In the list of documents the mailbox is one; its messages are found inside it.
        QCOMPARE(m_library->count(QString()), before + 1);
        const QString inside = u"partof:%1"_s.arg(mailbox);
        QCOMPARE(m_library->count(inside), 3);
        QCOMPARE(m_library->count(u"kind:email added:7d"_s) - m_library->count(u"kind:email added:7d -name:archive"_s), 1);
        const QVariantMap box = m_library->documentInfo(mailbox);
        QVERIFY(box.value(u"container"_s).toBool());
        QCOMPARE(box.value(u"parts"_s).toInt(), 3);

        // Each message is a document: found by its own words, named by its subject, dated as sent.
        QCOMPARE(m_library->count(u"quadrant"_s), 1);
        QCOMPARE(m_library->count(u"cogwheels"_s), 1);
        const QList<qint64> parts = Store::parts(mailbox);
        QCOMPARE(parts.size(), 3);
        qint64 reply = 0;
        qint64 unnamed = 0;
        for (qint64 id : parts) {
            const Document doc = Store::document(id);
            QCOMPARE(doc.parentId, mailbox);
            QCOMPARE(doc.kind, Kind::Email);
            if (doc.name == "Re: Difference engine"_L1) {
                reply = id;
                QCOMPARE(doc.modified.toUTC().date(), QDate(2026, 3, 3));
            } else if (doc.name == "(No subject)"_L1) {
                unnamed = id;
            }
        }
        QVERIFY(reply > 0 && unnamed > 0);
        const QVariantMap info = m_library->documentInfo(reply);
        QCOMPARE(info.value(u"parentId"_s).toLongLong(), mailbox);
        QVERIFY(info.value(u"location"_s).toString().contains("archive.mbox"_L1));

        // Looked at, it is taken out of the mailbox into a file of its own.
        QSignalSpy ready(m_library.get(), &Library::contentReady);
        m_library->requestContent(reply);
        QVERIFY(!ready.isEmpty() || ready.wait(5000));
        const QString taken = ready.constFirst().at(1).toString();
        QVERIFY(readFile(taken).startsWith("From: Charles Babbage"));
        QCOMPARE(m_library->email(reply, 0).value(u"subject"_s).toString(), u"Re: Difference engine"_s);
        QCOMPARE(readFile(path), first + second + third); // and the mailbox is as it was

        // What is attached to a message stays with it when the mailbox changes around it:
        // here the first message is deleted and one is added at the end.
        m_library->addTag(ids({reply}), u"answered"_s);
        m_library->addTag(ids({unnamed}), u"relay"_s);
        QCOMPARE(m_library->count(u"tag:answered"_s), 1);
        const QByteArray fourth = "From ada@example.org Thu Mar  5 07:00:00 2026\n"
                                  "From: Ada Lovelace <ada@example.org>\n"
                                  "Subject: Punch cards\n"
                                  "\n"
                                  "A new box of jacquard cards.\n";
        QTest::qWait(1100); // the file's time is kept in whole seconds
        writeFile(path, second + fourth);
        m_library->rescanAll();
        settle();
        QCOMPARE(m_library->count(u"cogwheels"_s), 0);
        QCOMPARE(m_library->count(u"jacquard"_s), 1);
        QCOMPARE(m_library->count(u"tag:answered"_s), 1);
        QCOMPARE(Store::document(reply).partOffset, qint64(second.indexOf('\n') + 1));
        m_library->requestContent(reply);
        QVERIFY(readFile(m_library->contentPath(reply)).startsWith("From: Charles Babbage"));
        // The message with a tag that is gone from the file is kept, as one that is missing.
        QVERIFY(Store::document(unnamed).missing);
        QCOMPARE(m_library->count(inside), 3);

        // In the trash, the mailbox takes its messages along; and back.
        m_library->moveToTrash(ids({mailbox}));
        QCOMPARE(m_library->count(u"jacquard"_s), 0);
        m_library->restoreFromTrash(ids({mailbox}));
        QCOMPARE(m_library->count(u"jacquard"_s), 1);

        // Gone from disk, the mailbox goes into the trash and its messages with it ...
        m_library->removeTag(ids({reply}), u"answered"_s);
        m_library->removeTag(ids({unnamed}), u"relay"_s);
        QVERIFY(QFile::remove(path));
        m_library->rescanAll();
        settle();
        QCOMPARE(m_library->count(QString()), before);
        QCOMPARE(m_library->count(inside), 0);
        QVERIFY(Store::document(mailbox).trashed);
        QVERIFY(Store::document(reply).trashed);
        // ... and they are forgotten together when it is emptied.
        m_library->emptyTrash();
        QVERIFY(!Store::document(mailbox).isValid());
        QVERIFY(!Store::document(reply).isValid());
        QCOMPARE(m_library->count(u"is:trashed"_s), 0);
        for (const QVariant &tag : m_library->tags()) {
            const QString name = tag.toMap().value(u"name"_s).toString();
            if (name == "answered"_L1 || name == "relay"_L1) {
                m_library->deleteTag(tag.toMap().value(u"id"_s).toLongLong());
            }
        }
    }

    void savedSearches()
    {
        const qint64 id = m_library->saveSearch(0, u"Recent PDFs"_s, u"kind:pdf added:7d"_s);
        QVERIFY(id > 0);
        QCOMPARE(m_library->savedSearches().size(), 1);
        QCOMPARE(m_library->saveSearch(id, u"PDFs"_s, u"kind:pdf"_s), id);
        QCOMPARE(m_library->savedSearches().first().toMap().value(u"name"_s).toString(), u"PDFs"_s);
        m_library->deleteSearch(id);
        QVERIFY(m_library->savedSearches().isEmpty());
    }

    void trashedDocumentsStayAway()
    {
        const qint64 id = idOf(u"unrelated.txt"_s);
        const int before = m_library->count(QString());
        m_library->moveToTrash(ids({id}));
        QCOMPARE(m_library->count(QString()), before - 1);
        QCOMPARE(m_library->count(u"is:trashed"_s), 1);
        m_library->restoreFromTrash(ids({id}));
        QCOMPARE(m_library->count(QString()), before);

        m_library->moveToTrash(ids({id}));
        m_library->emptyTrash();
        QCOMPARE(m_library->count(u"is:trashed"_s), 0);
        QVERIFY(QFile::exists(m_docs.filePath(u"unrelated.txt"_s))); // never deleted
        m_library->rescanAll();
        settle();
        QCOMPARE(m_library->count(QString()), before - 1); // and the scan does not bring it back
    }

    void emailDetails()
    {
        const qint64 id = idOf(u"message.eml"_s);
        const QVariantMap email = m_library->email(id);
        QVERIFY(email.value(u"valid"_s).toBool());
        QCOMPARE(email.value(u"subject"_s).toString(), u"Analytical engine café"_s);
        QCOMPARE(email.value(u"attachments"_s).toList().size(), 1);

        const QUrl extracted = m_library->extractAttachment(id, 0, 0);
        QVERIFY(extracted.toLocalFile().startsWith(Paths::cacheDir()));
        QCOMPARE(readFile(extracted.toLocalFile()), QByteArray("flux capacitor"));

        const qint64 imported = m_library->importAttachment(id, 0, 0);
        QVERIFY(imported > 0);
        QCOMPARE(m_library->count(u"is:inbox capacitor"_s), 1);
    }

    void recognisesTextInImages()
    {
        if (!Ocr::isAvailable() || Ocr::installedLanguages().isEmpty()) {
            QSKIP("tesseract is not installed");
        }
        writeTextImage(m_docs.filePath(u"scan.png"_s), u"Lighthouse keeper"_s);
        m_library->rescanAll();
        settle();
        const Document doc = Store::document(idOf(u"scan.png"_s));
        QCOMPARE(doc.kind, Kind::Image);
        QCOMPARE(doc.ocr, OcrState::Done);
        QCOMPARE(m_library->count(u"lighthouse"_s), 1);
        QCOMPARE(m_library->count(u"is:ocr"_s), 1);

        // Unchanged files keep their recognised text when they are indexed again.
        m_library->contentSaved(doc.id);
        QCOMPARE(m_library->count(u"lighthouse"_s), 1);
    }

    void recognisesSeveralPicturesAtOnce()
    {
        if (!Ocr::isAvailable()) {
            QSKIP("tesseract is not installed");
        }
        // Too small to hold text: not even worth asking tesseract.
        QVERIFY(Ocr::isTooSmallForText(QSize(32, 32)));
        QVERIFY(Ocr::isTooSmallForText(QSize(400, 8)));
        QVERIFY(!Ocr::isTooSmallForText(QSize(400, 60)));

        const QStringList words = {u"ALBATROSS"_s, u"BUTTERCUP"_s, u"CARAVAN"_s, u"DOLPHIN"_s, u"EVERGREEN"_s, u"FOXGLOVE"_s};
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        for (const QString &word : words) {
            writeTextImage(folder.filePath(word.toLower() + ".png"_L1), word);
        }
        QImage icon(24, 24, QImage::Format_RGB32);
        icon.fill(Qt::white);
        QVERIFY(icon.save(folder.filePath(u"icon.png"_s)));

        // Side by side, and every result ends up with its own picture.
        const QList<Ocr::Recognized> recognized = Ocr::recognizeAll(
            int(words.size()),
            [&](int index) {
                return Ocr::readImageFile(folder.filePath(words.at(index).toLower() + ".png"_L1));
            },
            u"eng"_s,
            4);
        QCOMPARE(recognized.size(), words.size());
        for (qsizetype i = 0; i < words.size(); ++i) {
            QVERIFY(recognized.at(i).ok);
            QVERIFY2(recognized.at(i).text.contains(words.at(i), Qt::CaseInsensitive), qPrintable(words.at(i) + u" / "_s + recognized.at(i).text));
        }

        // The same through the library: a folder of pictures becomes searchable.
        const qint64 source = m_library->addSource(QUrl::fromLocalFile(folder.path()));
        QVERIFY(QTest::qWaitFor([&] { return m_library->count(u"sourceid:%1 is:ocr"_s.arg(source)) == words.size(); }, 60000));
        settle();
        for (const QString &word : words) {
            QCOMPARE(m_library->count(u"sourceid:%1 %2"_s.arg(source).arg(word)), 1);
        }
        // The icon was never queued.
        QCOMPARE(int(Store::documentByUrl(QUrl::fromLocalFile(folder.filePath(u"icon.png"_s))).ocr), int(OcrState::NotNeeded));
        m_library->removeSource(source);
    }

    void filesThatBreakTheReader()
    {
        if (ExtractorPool::helperPath().isEmpty()) {
            QSKIP("the helper program is not built");
        }
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QStringList good = {u"alpha.txt"_s, u"bravo.txt"_s, u"charlie.txt"_s, u"delta.txt"_s};
        for (const QString &name : good) {
            writeFile(folder.filePath(name), "A note about marmalade, in " + name.toUtf8() + '\n');
        }
        // The helper is told to crash on the one and never to come back from the other.
        writeFile(folder.filePath(u"crash-the-reader.txt"_s), "This file takes the reader down with it: marmalade.\n");
        writeFile(folder.filePath(u"hang-the-reader.txt"_s), "This file keeps the reader for good: marmalade.\n");

        QSignalSpy messages(m_library.get(), &Library::message);
        const qint64 source = m_library->addSource(QUrl::fromLocalFile(folder.path()));
        const QString scope = u"sourceid:%1"_s.arg(source);
        QVERIFY(QTest::qWaitFor([&] { return m_library->count(scope) == 6; }, 30000));
        settle();

        // Neither took Indeks or the other files with it: those are all read …
        QCOMPARE(m_library->count(scope + u" marmalade"_s), int(good.size()));
        // … and the two are there too, by name, and say why their text is not.
        for (const QString &name : {u"crash-the-reader.txt"_s, u"hang-the-reader.txt"_s}) {
            const Document doc = Store::documentByUrl(QUrl::fromLocalFile(folder.filePath(name)));
            QVERIFY2(doc.isValid(), qPrintable(name));
            QCOMPARE(doc.kind, Kind::PlainText);
            QVERIFY(!doc.hash.isEmpty());
            QCOMPARE(m_library->count(scope + u" name:"_s + name), 1);
            const QVariantList metadata = m_library->documentInfo(doc.id).value(u"metadata"_s).toList();
            const bool explained = std::any_of(metadata.cbegin(), metadata.cend(), [](const QVariant &entry) {
                return entry.toMap().value(u"value"_s).toString().contains(u"Could not be read"_s);
            });
            QVERIFY2(explained, qPrintable(name));
        }
        const bool told = std::any_of(messages.cbegin(), messages.cend(), [](const QList<QVariant> &arguments) {
            return arguments.constFirst().toString().contains(u"could not be read"_s);
        });
        QVERIFY(told);

        // They are not tried again at every scan, only when they change.
        const int messagesBefore = int(messages.size());
        m_library->rescanSource(source);
        settle();
        QCOMPARE(int(messages.size()), messagesBefore);
        m_library->removeSource(source);
    }

    void workingWithADocumentTakesItOutOfTheInbox()
    {
        const qint64 id = m_library->createDocument(u"Errands"_s, u"txt"_s, u"Collect the parcel of marzipan.\n"_s);
        QVERIFY(id > 0);
        const auto waits = [&] {
            return Store::document(id).inbox;
        };
        const auto putBack = [&] {
            m_library->setInbox({id}, true);
            return waits();
        };
        QVERIFY(waits()); // what is made in Indeks starts out in the inbox

        m_library->addTag({id}, u"errands"_s);
        QVERIFY(!waits());
        QVERIFY(putBack());
        m_library->setFlagged({id}, true);
        QVERIFY(!waits());
        QVERIFY(putBack());
        QVERIFY(m_library->addNote(id, u"Ask for the receipt."_s) > 0);
        QVERIFY(!waits());
        QVERIFY(putBack());
        const qint64 group = m_library->createGroup(u"Errands of the week"_s);
        m_library->addToGroup({id}, group);
        QVERIFY(!waits());

        // Taking things off again is no reason to leave, and neither is a comment.
        QVERIFY(putBack());
        m_library->setFlagged({id}, false);
        m_library->removeTag({id}, u"errands"_s);
        m_library->removeFromGroup({id}, group);
        m_library->setComment(id, u"Before noon."_s);
        QVERIFY(waits());

        // A rule does what it says and no more: tagging by rule leaves the inbox alone.
        Rules::apply({id}, {QVariantMap{{u"type"_s, u"tag"_s}, {u"name"_s, u"sorted by rule"_s}}});
        QCOMPARE(m_library->count(u"tag:\"sorted by rule\""_s), 1);
        QVERIFY(waits());

        m_library->deleteGroup(group);
        m_library->deleteTag(Rules::tagId(u"errands"_s, false));
        m_library->deleteTag(Rules::tagId(u"sorted by rule"_s, false));
        m_library->moveToTrash({id});
        m_library->emptyTrash();
    }

    void everyViewHasItsOwnColumns()
    {
        QSignalSpy changes(m_library.get(), &Library::viewColumnsChanged);
        // A view nobody chose columns for has none of its own.
        QVERIFY(!m_library->hasViewColumns(u"place:inbox"_s));
        QVERIFY(m_library->viewColumns(u"place:inbox"_s).isEmpty());

        m_library->setViewColumns(u"place:inbox"_s, {u"kind"_s, u"added"_s});
        const qint64 group = m_library->createGroup(u"Columns of their own"_s);
        const QString groupView = u"group:%1"_s.arg(group);
        m_library->setViewColumns(groupView, {u"tags"_s, u"size"_s, u"comment"_s});
        QCOMPARE(int(changes.size()), 2);
        QVERIFY(m_library->hasViewColumns(u"place:inbox"_s));
        QCOMPARE(m_library->viewColumns(u"place:inbox"_s), QStringList({u"kind"_s, u"added"_s}));
        QCOMPARE(m_library->viewColumns(groupView), QStringList({u"tags"_s, u"size"_s, u"comment"_s}));
        QVERIFY(!m_library->hasViewColumns(u"place:all"_s));

        // No columns at all is a choice too, and not the same as no choice.
        m_library->setViewColumns(u"place:trash"_s, {});
        QVERIFY(m_library->hasViewColumns(u"place:trash"_s));
        QVERIFY(m_library->viewColumns(u"place:trash"_s).isEmpty());

        // Back to the standard, for one view ...
        m_library->clearViewColumns(u"place:trash"_s);
        QVERIFY(!m_library->hasViewColumns(u"place:trash"_s));
        QVERIFY(m_library->hasViewColumns(u"place:inbox"_s));

        // A group that is deleted takes its columns along: one that is made later, and may
        // be given the same number, starts out with the standard ones.
        m_library->deleteGroup(group);
        QVERIFY(!m_library->hasViewColumns(groupView));
        QVERIFY(m_library->hasViewColumns(u"place:inbox"_s));

        // ... and for all of them.
        m_library->clearAllViewColumns();
        QVERIFY(!m_library->hasViewColumns(u"place:inbox"_s));
    }

    void keepsAListOfThingsToDo()
    {
        const qint64 document = m_library->createDocument(u"Tax return"_s, u"txt"_s, u"Figures for the tax office.\n"_s);
        QVERIFY(document > 0);
        const QDate today = QDate::currentDate();
        const auto day = [&](int fromToday) {
            return today.addDays(fromToday).toString(Qt::ISODate);
        };
        QSignalSpy changes(m_library.get(), &Library::todosChanged);

        // A line of text is all it takes; a day and a document are extras.
        const qint64 plain = m_library->addTodo(u"  Water   the plants "_s);
        const qint64 late = m_library->addTodo(u"Hand in the tax return"_s, document, day(-2));
        const qint64 near = m_library->addTodo(u"Pay the plumber"_s, 0, day(2));
        const qint64 far = m_library->addTodo(u"Buy seeds"_s, 0, day(30));
        QVERIFY(plain > 0 && late > 0 && near > 0 && far > 0);
        QCOMPARE(m_library->addTodo(u"   "_s), 0); // nothing to do is no to-do
        QCOMPARE(int(changes.size()), 4);

        // What is due soonest comes first, what has no day last.
        const auto order = [&] {
            QList<qint64> ids;
            for (const QVariant &entry : m_library->todos()) {
                ids.append(entry.toMap().value(u"id"_s).toLongLong());
            }
            return ids;
        };
        QCOMPARE(order(), QList<qint64>({late, near, far, plain}));

        QCOMPARE(m_library->todo(plain).value(u"text"_s).toString(), u"Water the plants"_s);
        QCOMPARE(m_library->todo(plain).value(u"state"_s).toString(), QString());
        QCOMPARE(m_library->todo(plain).value(u"documentId"_s).toLongLong(), 0);
        // Past its day, and about a document.
        const QVariantMap overdue = m_library->todo(late);
        QCOMPARE(overdue.value(u"state"_s).toString(), u"overdue"_s);
        QCOMPARE(overdue.value(u"due"_s).toString(), day(-2));
        QCOMPARE(overdue.value(u"documentId"_s).toLongLong(), document);
        QCOMPARE(overdue.value(u"documentName"_s).toString(), Store::document(document).name);
        // Due within days, and due some time.
        QCOMPARE(m_library->todo(near).value(u"state"_s).toString(), u"soon"_s);
        QCOMPARE(m_library->todo(near).value(u"dueText"_s).isNull(), false);
        QCOMPARE(m_library->todo(far).value(u"state"_s).toString(), QString());
        QCOMPARE(m_library->todoCounts().value(u"open"_s).toInt(), 4);
        QCOMPARE(m_library->todoCounts().value(u"overdue"_s).toInt(), 1);
        QCOMPARE(m_library->todoCounts().value(u"soon"_s).toInt(), 1);
        // Today counts as soon, not as late.
        m_library->updateTodo(far, u"Buy seeds for the garden"_s, 0, day(0));
        QCOMPARE(m_library->todo(far).value(u"state"_s).toString(), u"soon"_s);
        QCOMPARE(m_library->todo(far).value(u"text"_s).toString(), u"Buy seeds for the garden"_s);
        // How early a to-do stands out is the user's to say: a month ahead ...
        m_library->setTodoWarningDays(30);
        QCOMPARE(m_library->todo(near).value(u"state"_s).toString(), u"soon"_s);
        m_library->updateTodo(far, u"Buy seeds for the garden"_s, 0, day(30));
        QCOMPARE(m_library->todo(far).value(u"state"_s).toString(), u"soon"_s);
        QCOMPARE(m_library->todoCounts().value(u"soon"_s).toInt(), 2);
        // ... or only on the day itself. Past its day is past its day either way.
        m_library->setTodoWarningDays(0);
        QCOMPARE(m_library->todo(near).value(u"state"_s).toString(), QString());
        QCOMPARE(m_library->todo(late).value(u"state"_s).toString(), u"overdue"_s);
        m_library->updateTodo(far, u"Buy seeds for the garden"_s, 0, day(0));
        QCOMPARE(m_library->todo(far).value(u"state"_s).toString(), u"soon"_s);
        QCOMPARE(m_library->todoCounts().value(u"soon"_s).toInt(), 1);
        m_library->setTodoWarningDays(3);
        // A day that is none is no day.
        m_library->updateTodo(far, u"Buy seeds for the garden"_s, 0, u"some time"_s);
        QCOMPARE(m_library->todo(far).value(u"due"_s).toString(), QString());

        // Done goes to the end and stands out no more.
        m_library->setTodoDone(late, true);
        QCOMPARE(m_library->todo(late).value(u"state"_s).toString(), QString());
        QCOMPARE(order().constLast(), late);
        QCOMPARE(m_library->todoCounts().value(u"done"_s).toInt(), 1);
        m_library->setTodoDone(late, false);
        QCOMPARE(order().constFirst(), late);

        // A to-do outlives the document it is about, as a line of text.
        m_library->moveToTrash({document});
        QVERIFY(m_library->todo(late).value(u"documentTrashed"_s).toBool());
        m_library->emptyTrash();
        QVERIFY(!Store::document(document).isValid());
        QCOMPARE(m_library->todo(late).value(u"documentId"_s).toLongLong(), 0);
        QCOMPARE(m_library->todo(late).value(u"text"_s).toString(), u"Hand in the tax return"_s);

        // Taking what is done off the list leaves the rest.
        m_library->setTodoDone(late, true);
        m_library->setTodoDone(near, true);
        QCOMPARE(m_library->removeDoneTodos(), 2);
        QCOMPARE(order(), QList<qint64>({plain, far}));
        m_library->removeTodo(plain);
        m_library->removeTodo(far);
        QVERIFY(m_library->todos().isEmpty());
    }

    void newFilesInLocationsGoToTheInbox()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        writeFile(folder.filePath(u"ledger.txt"_s), "What was owed in the year of the comet.\n");
        writeFile(folder.filePath(u"minutes.txt"_s), "Minutes of the kite society.\n");
        writeFile(folder.filePath(u".secret.txt"_s), "A hidden recipe for quince jelly.\n");

        const qint64 source = m_library->addSource(QUrl::fromLocalFile(folder.path()));
        const QString scope = u"sourceid:%1"_s.arg(source);
        const QString waiting = scope + u" is:inbox"_s;
        settle();
        // What was there when the location was added is no news.
        QCOMPARE(m_library->count(scope), 2);
        QCOMPARE(m_library->count(waiting), 0);
        QVERIFY(m_library->sourceUsesInbox(source));

        // What turns up afterwards is: a scan that was just saved, say.
        writeFile(folder.filePath(u"scan-0001.txt"_s), "Recognized text of a scanned gas bill.\n");
        m_library->rescanSource(source);
        settle();
        QCOMPARE(m_library->count(scope), 3);
        QCOMPARE(m_library->count(waiting), 1);
        const qint64 scan = Store::documentByUrl(QUrl::fromLocalFile(folder.filePath(u"scan-0001.txt"_s))).id;
        QVERIFY(Store::document(scan).inbox);

        // Filed is filed: a file that changes or moves does not come back into the inbox.
        m_library->setInbox({scan}, false);
        writeFile(folder.filePath(u"scan-0001.txt"_s), "Recognized text of a scanned gas bill, with a correction.\n");
        QVERIFY(QFile::rename(folder.filePath(u"ledger.txt"_s), folder.filePath(u"ledger of the comet.txt"_s)));
        m_library->rescanSource(source);
        settle();
        QCOMPARE(m_library->count(scope), 3);
        QCOMPARE(m_library->count(waiting), 0);
        QCOMPARE(m_library->count(scope + u" correction"_s), 1);

        // Having hidden files indexed brings in files that were there all along.
        IndexOptions options;
        options.ocrEnabled = false;
        options.indexHidden = true;
        m_library->setOptions(options);
        settle();
        QCOMPARE(m_library->count(scope + u" quince"_s), 1);
        QCOMPARE(m_library->count(waiting), 0);
        // After that, new is new again.
        writeFile(folder.filePath(u"scan-0002.txt"_s), "Recognized text of a scanned water bill.\n");
        m_library->rescanSource(source);
        settle();
        QCOMPARE(m_library->count(waiting), 1);

        // A location can do without: its new files are indexed and that is it.
        m_library->setSourceUsesInbox(source, false);
        QVERIFY(!m_library->sourceUsesInbox(source));
        writeFile(folder.filePath(u"scan-0003.txt"_s), "Recognized text of a scanned phone bill.\n");
        m_library->rescanSource(source);
        settle();
        QCOMPARE(m_library->count(scope + u" bill"_s), 3);
        QCOMPARE(m_library->count(waiting), 1);

        options.indexHidden = false;
        m_library->setOptions(options);
        m_library->removeSource(source);
        settle();
    }

    void obsidianCanvas()
    {
        // A vault: file cards are looked up relative to the folder that holds .obsidian.
        QDir(m_docs.path()).mkdir(u".obsidian"_s);
        QDir(m_docs.path()).mkdir(u"boards"_s);
        // Written with single quotes for readability (and because moc trips over raw strings).
        const QByteArray plan = QByteArray("{'nodes':["
                                           "{'id':'g','type':'group','x':-300,'y':-200,'width':700,'height':500,'label':'Greenhouse','color':'4'},"
                                           "{'id':'a','type':'text','x':-250,'y':-150,'width':200,'height':100,'text':'Order **tomato** seedlings'},"
                                           "{'id':'b','type':'file','x':100,'y':-150,'width':250,'height':200,'file':'page.html'},"
                                           "{'id':'c','type':'file','x':100,'y':100,'width':250,'height':100,'file':'../../../../etc/passwd'},"
                                           "{'id':'d','type':'link','x':500,'y':0,'width':200,'height':100,'url':'https://example.org/trellis'}],"
                                           "'edges':["
                                           "{'id':'e1','fromNode':'a','toNode':'b','label':'irrigation'},"
                                           "{'id':'e2','fromNode':'a','toNode':'d','fromSide':'bottom','toSide':'left','toEnd':'none','fromEnd':'arrow'},"
                                           "{'id':'e3','fromNode':'a','toNode':'nowhere'}]}")
                                    .replace('\'', '"');
        writeFile(m_docs.filePath(u"boards/plan.canvas"_s), plan);
        writeFile(m_docs.filePath(u"boards/empty.canvas"_s), "");
        m_library->rescanAll();
        settle();

        QCOMPARE(m_library->count(u"kind:canvas"_s), 2);
        // The words on the cards are indexed, the JSON around them is not.
        QCOMPARE(m_library->count(u"tomato kind:canvas"_s), 1);
        QCOMPARE(m_library->count(u"greenhouse"_s), 1);
        QCOMPARE(m_library->count(u"irrigation"_s), 1);
        QCOMPARE(m_library->count(u"trellis"_s), 1);
        QCOMPARE(m_library->count(u"fromNode"_s), 0);

        const qint64 id = idOf(u"boards/plan.canvas"_s);
        QCOMPARE(m_library->documentInfo(id).value(u"kind"_s).toInt(), int(Kind::Canvas));
        const QVariantMap canvas = m_library->canvas(id);
        QVERIFY(canvas.value(u"valid"_s).toBool());
        // 700 x 500 covered by the group, widened by the link card, plus the margin on all sides.
        QCOMPARE(canvas.value(u"width"_s).toDouble(), 1000.0 + 120.0);
        QCOMPARE(canvas.value(u"height"_s).toDouble(), 500.0 + 120.0);

        const QVariantList nodes = canvas.value(u"nodes"_s).toList();
        QCOMPARE(nodes.size(), 5);
        const QVariantMap group = nodes.at(0).toMap();
        QCOMPARE(group.value(u"type"_s).toString(), u"group"_s);
        QCOMPARE(group.value(u"text"_s).toString(), u"Greenhouse"_s);
        QCOMPARE(group.value(u"x"_s).toDouble(), 60.0); // coordinates start at the board's corner
        const QVariantMap fileCard = nodes.at(2).toMap();
        QCOMPARE(fileCard.value(u"fileKind"_s).toString(), u"other"_s);
        QCOMPARE(fileCard.value(u"documentId"_s).toLongLong(), idOf(u"page.html"_s));
        // A path that leaves the vault is not followed.
        QCOMPARE(nodes.at(3).toMap().value(u"fileKind"_s).toString(), u"missing"_s);
        QVERIFY(!nodes.at(3).toMap().contains(u"url"_s));

        // The connection to a card that does not exist is dropped.
        const QVariantList edges = canvas.value(u"edges"_s).toList();
        QCOMPARE(edges.size(), 2);
        const QVariantMap first = edges.at(0).toMap();
        QCOMPARE(first.value(u"start"_s).toPointF(), QPointF(310, 160)); // leaves the right side, facing the file card
        QCOMPARE(first.value(u"end"_s).toPointF(), QPointF(460, 210));
        QVERIFY(first.value(u"startArrow"_s).toList().isEmpty());
        QCOMPARE(first.value(u"endArrow"_s).toList().size(), 4);
        const QVariantMap second = edges.at(1).toMap();
        QCOMPARE(second.value(u"start"_s).toPointF(), QPointF(210, 210));
        QCOMPARE(second.value(u"startArrow"_s).toList().size(), 4);
        QVERIFY(second.value(u"endArrow"_s).toList().isEmpty());

        // Obsidian leaves a canvas nothing was put on yet as an empty file.
        const QVariantMap empty = m_library->canvas(idOf(u"boards/empty.canvas"_s));
        QVERIFY(empty.value(u"valid"_s).toBool());
        QVERIFY(empty.value(u"nodes"_s).toList().isEmpty());

        writeFile(m_docs.filePath(u"boards/broken.canvas"_s), "this is not JSON");
        m_library->rescanAll();
        settle();
        QVERIFY(!m_library->canvas(idOf(u"boards/broken.canvas"_s)).value(u"valid"_s).toBool());
    }

    void removingALocationKeepsWorkedOnDocuments()
    {
        const qint64 source = m_library->sources().first().toMap().value(u"id"_s).toLongLong();
        const int tagged = m_library->count(u"-is:untagged sourceid:%1"_s.arg(source));
        QVERIFY(tagged > 0);
        m_library->removeSource(source);
        QVERIFY(m_library->sources().isEmpty());
        QCOMPARE(m_library->count(u"sourceid:%1"_s.arg(source)), 0);
        QVERIFY(m_library->count(u"-is:untagged"_s) >= tagged);
        QVERIFY(QFile::exists(m_docs.filePath(u"report.pdf"_s)));
    }

    void indexesRemoteLocation()
    {
        // KIO's archive worker serves as a location that is not a path in the file system,
        // the way a server would be.
        QTemporaryDir remote;
        writeFile(remote.filePath(u"far.txt"_s), "A faraway file about a dirigible.\n");
        QDir(remote.path()).mkdir(u"deep"_s);
        writeFile(remote.filePath(u"deep/further.md"_s), "# Distant\n\nMentions a theremin.\n");
        const QString archive = m_data.filePath(u"remote.tar"_s);
        QCOMPARE(QProcess::execute(u"tar"_s, {u"-cf"_s, archive, u"-C"_s, remote.path(), u"far.txt"_s, u"deep"_s}), 0);

        QUrl url;
        url.setScheme(u"tar"_s);
        url.setPath(archive);
        const qint64 source = m_library->addSource(url, u"Archive"_s);
        QVERIFY(source > 0);
        const QString scope = u"sourceid:%1"_s.arg(source);
        if (!QTest::qWaitFor([&] { return m_library->count(scope) == 2 && !m_library->isBusy(); }, 20000)) {
            QSKIP("KIO cannot list tar archives here");
        }
        QCOMPARE(m_library->count(u"dirigible"_s), 1);
        QCOMPARE(m_library->count(u"theremin kind:text"_s), 1);
        QVERIFY(m_library->sources().first().toMap().value(u"available"_s).toBool());

        // The content is not kept on this computer; it is fetched when it is looked at.
        QUrl fileUrl = url;
        fileUrl.setPath(archive + "/far.txt"_L1);
        const Document doc = Store::documentByUrl(fileUrl);
        QVERIFY(doc.isValid());
        QVERIFY(m_library->contentPath(doc.id).isEmpty());
        QSignalSpy ready(m_library.get(), &Library::contentReady);
        m_library->requestContent(doc.id);
        QVERIFY(ready.wait(20000));
        const QString fetched = ready.first().at(1).toString();
        QVERIFY(fetched.startsWith(Paths::cacheDir()));
        QCOMPARE(readFile(fetched), QByteArray("A faraway file about a dirigible.\n"));

        // Dropped onto the window or sent from the file manager, a folder on a server becomes
        // a location once the server has said that it is a folder. A file there is not taken.
        const QString second = m_data.filePath(u"second.tar"_s);
        QCOMPARE(QProcess::execute(u"tar"_s, {u"-cf"_s, second, u"-C"_s, remote.path(), u"deep"_s}), 0);
        QUrl folderUrl = url;
        folderUrl.setPath(second + "/deep"_L1);
        QUrl looseUrl = url;
        looseUrl.setPath(second + "/deep/further.md"_L1);
        const int sourcesBefore = m_library->sources().size();
        QSignalSpy told(m_library.get(), &Library::message);
        QCOMPARE(m_library->addFolders({looseUrl}), 0);
        QVERIFY(told.wait(20000));
        QVERIFY(told.constFirst().constFirst().toString().contains(u"further.md"_s));
        QVERIFY(!Store::documentByUrl(looseUrl).isValid());
        QCOMPARE(m_library->sources().size(), sourcesBefore);

        QCOMPARE(m_library->addFolders({folderUrl}), 0); // not yet: the server is asked first
        QVERIFY(QTest::qWaitFor([&] { return m_library->sources().size() == sourcesBefore + 1; }, 20000));
        QVERIFY(QTest::qWaitFor([&] { return Store::documentByUrl(looseUrl).isValid() && !m_library->isBusy(); }, 20000));
        const qint64 sent = Store::documentByUrl(looseUrl).sourceId;
        QVERIFY(sent > 0 && sent != source);
        m_library->removeSource(sent);
        QCOMPARE(m_library->sources().size(), sourcesBefore);
        QVERIFY(QFile::remove(second));

        // When the location cannot be reached its documents stay in the index.
        QVERIFY(QFile::remove(archive));
        m_library->rescanSource(source);
        QVERIFY(QTest::qWaitFor([&] { return !m_library->isSourceAvailable(source); }, 20000));
        QCOMPARE(m_library->count(scope), 2);
        QCOMPARE(m_library->count(u"dirigible"_s), 1);
        QVERIFY(m_library->documentInfo(doc.id).value(u"offline"_s).toBool());
    }

    void convertsHtmlToText()
    {
        QCOMPARE(htmlToPlainText(u"<p>Caf&eacute; &amp; bar &#8212; 5 &lt; 6</p><script>var x = '<p>no</p>';</script>"
                                 "<div>second<br>line</div><!-- gone --><table><tr><td>a</td><td>b</td></tr></table>"_s),
                 u"Café & bar — 5 < 6\nsecond\nline\na b"_s);
        // Text that only looks like markup stays as it is.
        QCOMPARE(htmlToPlainText(u"a < b and c > d, AT&T"_s), u"a < b and c > d, AT&T"_s);
        QCOMPARE(htmlToPlainText(u"<pre>one\n  two</pre>"_s), u"one\n  two"_s);
        // Markup that was cut off does not take the rest of the text with it.
        QCOMPARE(htmlToPlainText(u"before <svg width='1'><path d='M0'> <!-- open <script>x after"_s), u"before open x after"_s);

        // Megabytes of data with markup strewn in, which the type detection takes for HTML,
        // once kept the indexer busy for minutes.
        QString big;
        for (int i = 0; i < 150000; ++i) {
            big += u"{\"html\": \"<div class='x'><p>Row <b>%1</b> &amp; more<p><span>never closed\", \"n\": %1}\n"_s.arg(i);
        }
        QVERIFY(big.size() > 10 * 1024 * 1024);
        QElapsedTimer timer;
        timer.start();
        const QString text = htmlToPlainText(big);
        QVERIFY2(timer.elapsed() < 5000, qPrintable(QString::number(timer.elapsed())));
        QVERIFY(text.contains(u"Row 149999 & more"_s));
    }

    void laysOutJson()
    {
        // Keys keep their order, values their spelling, strings their content.
        QCOMPARE(reindentJson(u"{\"b\":1.50,\"a\":[true,null,{}],\"s\":\"x, {y}: \\\"z\\\"\",\"e\":[]}"),
                 u"{\n  \"b\": 1.50,\n  \"a\": [\n    true,\n    null,\n    {}\n  ],\n  \"s\": \"x, {y}: \\\"z\\\"\",\n  \"e\": []\n}"_s);
        // Cut-off JSON comes out as far as it goes.
        QCOMPARE(reindentJson(u"{\"a\":[1,2"), u"{\n  \"a\": [\n    1,\n    2"_s);

        const QString lines = u"{\"n\":1}\n\n  \nnot json at all\n[1,2]\n{\"n\":3}\n"_s;
        FormattedJson all = formatJsonLines(lines);
        QCOMPARE(all.text, u"{\n  \"n\": 1\n}\nnot json at all\n[\n  1,\n  2\n]\n{\n  \"n\": 3\n}"_s);
        QCOMPARE(all.recordLines, (QList<int>{0, 3, 4, 8}));
        QCOMPARE(all.sourceLines, (QList<int>{1, 4, 5, 6}));
        QVERIFY(!all.cut);
        const FormattedJson some = formatJsonLines(lines, 2);
        QCOMPARE(some.recordLines.size(), 2);
        QVERIFY(some.cut);

        QVERIFY(isJsonLinesFile(u"Data.JSONL"_s));
        QVERIFY(!isJsonLinesFile(u"data.json"_s));
        QVERIFY(!isMinifiedJson(u"{\n  \"a\": 1\n}"));
        QVERIFY(isMinifiedJson(QString(u"{\"a\":\"" + QString(500, u'x') + u"\"}")));
    }

    void jsonLinesDocuments()
    {
        QVERIFY(QDir(m_docs.path()).mkpath(u"formats"_s));
        // Records that quote markup made the type detection take the file for a web page.
        QByteArray content;
        for (int i = 1; i <= 30; ++i) {
            content += "{\"url\": \"https://example.org/" + QByteArray::number(i)
                + "\", \"snip\": \"<html><head><title>Moved</title></head><body><h1>quokka</h1></body></html>\"}\n";
        }
        const QString path = m_docs.filePath(u"formats/routes.jsonl"_s);
        writeFile(path, content);
        writeFile(m_docs.filePath(u"formats/small.json"_s), "{\n  \"pretty\": true\n}\n");
        writeFile(m_docs.filePath(u"formats/packed.json"_s), "{\"packed\":\"" + QByteArray(600, 'y') + "\",\"n\":[1,2]}");
        indexFormats();

        const Document doc = Store::documentByUrl(QUrl::fromLocalFile(path));
        QVERIFY(doc.isValid());
        QCOMPARE(doc.kind, Kind::PlainText);
        QCOMPARE(doc.mimeType, u"application/json"_s);
        QCOMPARE(m_library->count(u"quokka name:routes"_s), 1);

        const QVariantMap formatted = m_library->formattedText(doc.id, 10);
        QCOMPARE(formatted.value(u"shown"_s).toInt(), 10);
        QCOMPARE(formatted.value(u"total"_s).toInt(), 30);
        QVERIFY(formatted.value(u"lines"_s).toBool());
        QVERIFY(formatted.value(u"text"_s).toString().startsWith(u"{\n  \"url\": \"https://example.org/1\",\n  \"snip\": "_s));
        QCOMPARE(formatted.value(u"recordLines"_s).value<QList<int>>().at(1), 4);
        QCOMPARE(formatted.value(u"sourceLines"_s).value<QList<int>>().at(9), 10);
        QCOMPARE(m_library->formattedText(doc.id, 0).value(u"shown"_s).toInt(), 30);

        // JSON that is already laid out is left alone, packed JSON is opened up.
        QVERIFY(m_library->formattedText(idOf(u"formats/small.json"_s), 0).isEmpty());
        const QVariantMap packed = m_library->formattedText(idOf(u"formats/packed.json"_s), 0);
        QVERIFY(!packed.value(u"lines"_s).toBool());
        QVERIFY(packed.value(u"text"_s).toString().endsWith(u"\"n\": [\n    1,\n    2\n  ]\n}"_s));
        QVERIFY(m_library->formattedText(idOf(u"shopping.txt"_s), 0).isEmpty());
    }

    void countsTheRecordsOfLargeFilesLater()
    {
        QVERIFY(QDir(m_docs.path()).mkpath(u"formats"_s));
        // More than the preview reads: its records are not counted while the user waits for
        // the document to show, but afterwards, away from the user interface.
        QByteArray content;
        const QByteArray filler(900, 'x');
        int records = 0;
        while (content.size() < m_library->maxTextSize() + 512 * 1024) {
            content += "{\"n\": " + QByteArray::number(++records) + ", \"filler\": \"" + filler + "\"}\n";
        }
        content += "\n   \n"; // lines with nothing on them are no records
        const QString path = m_docs.filePath(u"formats/long.jsonl"_s);
        writeFile(path, content);
        indexFormats();
        const qint64 id = idOf(u"formats/long.jsonl"_s);
        QVERIFY(id != 0);

        const QVariantMap formatted = m_library->formattedText(id, 10);
        QCOMPARE(formatted.value(u"shown"_s).toInt(), 10);
        QCOMPARE(formatted.value(u"total"_s).toInt(), -1);
        QVERIFY(formatted.value(u"more"_s).toBool());

        QSignalSpy counted(m_library.get(), &Library::recordsCounted);
        m_library->countRecords(id);
        QVERIFY(counted.wait(10000));
        QCOMPARE(counted.constFirst().at(0).toLongLong(), id);
        QCOMPARE(counted.constFirst().at(1).toInt(), records);

        // A file the preview reads whole is counted on the spot, as before.
        const QVariantMap small = m_library->formattedText(idOf(u"formats/routes.jsonl"_s), 10);
        QCOMPARE(small.value(u"total"_s).toInt(), 30);
        QVERIFY(small.value(u"more"_s).toBool());
        QVERIFY(!m_library->formattedText(idOf(u"formats/routes.jsonl"_s), 0).value(u"more"_s).toBool());
    }

    void officeDocuments()
    {
        QVERIFY(QDir(m_docs.path()).mkpath(u"formats"_s));
        QVERIFY(Office::isOfficeDocument(u"application/vnd.openxmlformats-officedocument.wordprocessingml.document"_s));
        QVERIFY(Office::isOfficeDocument(u"application/vnd.ms-excel"_s));
        QVERIFY(!Office::isOfficeDocument(u"application/pdf"_s));
        QVERIFY(!Office::isOfficeDocument(u"text/plain"_s));

        // RTF is the one Office format that can be written out by hand here.
        const QString path = m_docs.filePath(u"formats/memo.rtf"_s);
        const QByteArray content = "{\\rtf1\\ansi{\\fonttbl{\\f0 Arial;}}\\f0 Memo about the \\b okapi\\b0  enclosure.\\par Second paragraph.}";
        writeFile(path, content);
        indexFormats();
        const qint64 id = idOf(u"formats/memo.rtf"_s);
        QVERIFY(id > 0);
        QVERIFY(m_library->documentInfo(id).value(u"office"_s).toBool());
        QCOMPARE(m_library->count(u"okapi"_s), 1);

        if (!m_library->officeViewAvailable()) {
            QSKIP("LibreOffice is not installed");
        }
        QSignalSpy ready(m_library.get(), &Library::renditionReady);
        QSignalSpy failed(m_library.get(), &Library::renditionFailed);
        m_library->requestRendition(id);
        QVERIFY2(ready.wait(120000), failed.isEmpty() ? "no answer" : qPrintable(failed.first().at(1).toString()));
        QCOMPARE(ready.first().at(0).toLongLong(), id);
        const QString pdf = ready.first().at(1).toUrl().toLocalFile();
        QVERIFY(pdf.startsWith(Paths::cacheDir()));
        QVERIFY(readFile(pdf).startsWith("%PDF"));
        // The document was only read.
        QCOMPARE(readFile(path), content);

        // Asked again, the same rendition is at hand without LibreOffice.
        ready.clear();
        QElapsedTimer timer;
        timer.start();
        m_library->requestRendition(id);
        QVERIFY(ready.wait(5000));
        QCOMPARE(ready.first().at(1).toUrl().toLocalFile(), pdf);
        QVERIFY(timer.elapsed() < 1000);
    }

    void switchesDatabases()
    {
        // A database is a folder; this one stands in for a share on a NAS.
        QTemporaryDir share;
        QVERIFY(share.isValid());
        const QString first = share.filePath(u"database"_s);
        QVERIFY(QDir().mkpath(first));
        const QString home = QDir::cleanPath(m_library->databaseDir());
        const int before = m_library->count(QString());
        QVERIFY(before > 0);

        // An empty folder is no database, but one can be started in it.
        QVERIFY(!Library::databaseProblem(first, false).isEmpty());
        QCOMPARE(Library::databaseProblem(first, true), QString());
        QCOMPARE(m_library->switchDatabase(first, true), QString());
        QCOMPARE(QDir::cleanPath(m_library->databaseDir()), first);
        QCOMPARE(m_library->count(QString()), 0);

        // What is created in a database is stored in its folder, not on this computer.
        const qint64 id = m_library->createDocument(u"Harbour log"_s, u"text"_s, u"The ketch left at dawn."_s);
        QVERIFY(id > 0);
        settle();
        QVERIFY(Store::document(id).url.toLocalFile().startsWith(first + "/files/"_L1));
        QCOMPARE(m_library->count(u"ketch"_s), 1);

        // A folder that cannot be reached is refused and the open database stays open.
        QVERIFY(!m_library->switchDatabase(share.filePath(u"not-mounted"_s)).isEmpty());
        QVERIFY(!QFileInfo::exists(share.filePath(u"not-mounted"_s))); // and is not made either
        QCOMPARE(QDir::cleanPath(m_library->databaseDir()), first);
        QCOMPARE(m_library->count(u"ketch"_s), 1);

        // Back to the first database: its documents are all there, the other one's are not.
        QCOMPARE(m_library->switchDatabase(QString()), QString());
        QCOMPARE(QDir::cleanPath(m_library->databaseDir()), home);
        QCOMPARE(m_library->count(QString()), before);
        QCOMPARE(m_library->count(u"ketch"_s), 0);

        // The share is mounted somewhere else the next time: the database still finds its own.
        const QString second = share.filePath(u"elsewhere"_s);
        QVERIFY(QDir().rename(first, second));
        QCOMPARE(m_library->switchDatabase(second), QString());
        QCOMPARE(m_library->count(u"ketch"_s), 1);
        const Document moved = Store::document(id);
        QVERIFY(moved.url.toLocalFile().startsWith(second + "/files/"_L1));
        QCOMPARE(m_library->readText(id), u"The ketch left at dawn."_s);
        QCOMPARE(m_library->switchDatabase(QString()), QString());

        // Open on another computer: stay out.
        writeFile(second + "/indeks-in-use"_L1, "othercomputer\n4711\n");
        QVERIFY(Library::databaseProblem(second, false).contains(u"othercomputer"_s));
        QVERIFY(!m_library->switchDatabase(second).isEmpty());
        QCOMPARE(QDir::cleanPath(m_library->databaseDir()), home);
        QCOMPARE(m_library->count(QString()), before);
    }

    void tellsItsIndexFromOtherFiles()
    {
        QTemporaryDir share;
        QVERIFY(share.isValid());
        const QString home = QDir::cleanPath(m_library->databaseDir());
        const QString indexName = Paths::indexFileName();
        // Runs one statement on an SQLite file, the way another program would.
        auto sqlite = [](const QString &file, const QString &sql) {
            QVariant result;
            {
                QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, u"librarytest-other"_s);
                db.setDatabaseName(file);
                if (db.open()) {
                    QSqlQuery query(db);
                    if (query.exec(sql) && query.next()) {
                        result = query.value(0);
                    }
                    query.finish();
                    db.close();
                }
            }
            QSqlDatabase::removeDatabase(u"librarytest-other"_s);
            return result;
        };

        // The index that is open carries the mark of Indeks.
        QCOMPARE(Database::value(u"PRAGMA application_id"_s).toInt(), Database::ApplicationId);
        QCOMPARE(Database::inspectFile(home + u'/' + indexName).kind, Database::FileKind::Own);

        // The database of another program that happens to be called like an index: it is
        // not opened, nothing is started beside it, and not a byte of it changes.
        const QString other = share.filePath(u"other"_s);
        QVERIFY(QDir().mkpath(other));
        const QString otherIndex = other + u'/' + indexName;
        sqlite(otherIndex, u"CREATE TABLE recipes (id INTEGER PRIMARY KEY, name TEXT)"_s);
        sqlite(otherIndex, u"INSERT INTO recipes (name) VALUES ('Borscht')"_s);
        const QByteArray untouched = readFile(otherIndex);
        QVERIFY(!untouched.isEmpty());
        QCOMPARE(Database::inspectFile(otherIndex).kind, Database::FileKind::Foreign);
        QVERIFY(Library::databaseProblem(other, false).contains(indexName));
        QVERIFY(Library::databaseProblem(other, true).contains(indexName));
        QVERIFY(!m_library->switchDatabase(other, true).isEmpty());
        QVERIFY(!m_library->switchDatabase(other, false).isEmpty());
        QCOMPARE(QDir::cleanPath(m_library->databaseDir()), home);
        QCOMPARE(readFile(otherIndex), untouched);
        QCOMPARE(QDir(other).entryList(QDir::AllEntries | QDir::NoDotAndDotDot), QStringList{indexName});

        // So with another program's mark on it, and with a file that is no database at all.
        const QString marked = share.filePath(u"marked"_s);
        QVERIFY(QDir().mkpath(marked));
        sqlite(marked + u'/' + indexName, u"PRAGMA application_id = 4711"_s);
        QCOMPARE(Database::inspectFile(marked + u'/' + indexName).kind, Database::FileKind::Foreign);
        const QString scribble = share.filePath(u"scribble"_s);
        QVERIFY(QDir().mkpath(scribble));
        writeFile(scribble + u'/' + indexName, "Not a database, just called like one.\n");
        QCOMPARE(Database::inspectFile(scribble + u'/' + indexName).kind, Database::FileKind::Foreign);
        QVERIFY(!m_library->switchDatabase(scribble, true).isEmpty());
        QCOMPARE(readFile(scribble + u'/' + indexName), QByteArray("Not a database, just called like one.\n"));

        // A file with nothing in it is where a start was cut short: no database, but one
        // can begin there.
        const QString blank = share.filePath(u"blank"_s);
        QVERIFY(QDir().mkpath(blank));
        writeFile(blank + u'/' + indexName, QByteArray());
        QCOMPARE(Database::inspectFile(blank + u'/' + indexName).kind, Database::FileKind::Blank);
        QVERIFY(!Library::databaseProblem(blank, false).isEmpty());
        QCOMPARE(Library::databaseProblem(blank, true), QString());
        QCOMPARE(Database::inspectFile(share.filePath(u"nowhere/"_s) + indexName).kind, Database::FileKind::Missing);

        // An index from before Indeks marked its files is known by what is in it, and gets
        // its mark when it is opened.
        const QString old = share.filePath(u"old"_s);
        QVERIFY(QDir().mkpath(old));
        QCOMPARE(m_library->switchDatabase(old, true), QString());
        QCOMPARE(m_library->switchDatabase(QString()), QString());
        const QString oldIndex = old + u'/' + indexName;
        QCOMPARE(sqlite(oldIndex, u"PRAGMA application_id"_s).toInt(), Database::ApplicationId);
        sqlite(oldIndex, u"PRAGMA application_id = 0"_s);
        QCOMPARE(sqlite(oldIndex, u"PRAGMA application_id"_s).toInt(), 0);
        const Database::FileState before = Database::inspectFile(oldIndex);
        QCOMPARE(before.kind, Database::FileKind::Own);
        QCOMPARE(before.version, Database::SchemaVersion);
        QCOMPARE(Library::databaseProblem(old, false), QString());
        QCOMPARE(m_library->switchDatabase(old), QString());
        QCOMPARE(Database::value(u"PRAGMA application_id"_s).toInt(), Database::ApplicationId);
        QCOMPARE(m_library->switchDatabase(QString()), QString());
        QCOMPARE(QDir::cleanPath(m_library->databaseDir()), home);

        // A database is added by its index file: the folder is not enough, and neither is
        // another file in it.
        QString problem;
        QCOMPARE(Library::databaseOfIndexFile(oldIndex, &problem), old);
        QCOMPARE(problem, QString());
        QCOMPARE(Library::databaseOfIndexFile(old, &problem), QString());
        QVERIFY(problem.contains(indexName));
        writeFile(old + "/backup.sqlite"_L1, readFile(oldIndex));
        QCOMPARE(Library::databaseOfIndexFile(old + "/backup.sqlite"_L1, &problem), QString());
        QVERIFY(problem.contains(u"backup.sqlite"_s));
        QCOMPARE(Library::databaseOfIndexFile(old + "/missing.sqlite"_L1, &problem), QString());
        QVERIFY(!problem.isEmpty());

        // Whether there is anything in a folder, for the warning before a database is
        // started among other things.
        const QString bare = share.filePath(u"bare"_s);
        QVERIFY(QDir().mkpath(bare));
        QVERIFY(DatabaseFolder::inspect(bare).empty);
        writeFile(bare + "/.hidden"_L1, "x");
        QVERIFY(!DatabaseFolder::inspect(bare).empty);
        QVERIFY(!DatabaseFolder::inspect(old).empty);
    }

    void documentsKeptForTheUsersWorkGoWithIt()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        for (const char *name : {"noted.txt", "late.txt", "tagged.txt", "edited.txt"}) {
            writeFile(folder.filePath(QString::fromLatin1(name)), QByteArray("A text called ") + name + '\n');
        }
        writePdf(folder.filePath(u"drawn.pdf"_s), u"A page to scribble on"_s);
        const qint64 source = m_library->addSource(QUrl::fromLocalFile(folder.path()));
        settle();
        auto idIn = [&](const QString &name) {
            return Store::documentByUrl(QUrl::fromLocalFile(folder.filePath(name))).id;
        };
        const qint64 noted = idIn(u"noted.txt"_s);
        const qint64 drawn = idIn(u"drawn.pdf"_s);
        const qint64 late = idIn(u"late.txt"_s);
        const qint64 tagged = idIn(u"tagged.txt"_s);
        const qint64 edited = idIn(u"edited.txt"_s);
        QVERIFY(noted > 0 && drawn > 0 && late > 0 && tagged > 0 && edited > 0);

        // A note that came and went leaves nothing that would hold the document back …
        m_library->removeNote(m_library->addNote(noted, u"temporary"_s));
        // … and neither does a drawing, although drawing put a copy of the file aside.
        m_library->setInk(drawn, 0, u"[{\"color\":\"#ff0000\",\"width\":0.004,\"points\":[0.1,0.1,0.2,0.2]}]"_s);
        QVERIFY(m_library->documentInfo(drawn).value(u"hasLocalCopy"_s).toBool());
        m_library->clearInk(drawn);
        // This one still has its note when the file goes, and this one was rewritten in Indeks.
        const qint64 lateNote = m_library->addNote(late, u"still here"_s);
        QVERIFY(m_library->saveText(edited, u"My own version."_s));
        settle();

        for (const char *name : {"noted.txt", "drawn.pdf", "late.txt", "edited.txt"}) {
            QVERIFY(QFile::remove(folder.filePath(QString::fromLatin1(name))));
        }
        m_library->rescanSource(source);
        settle();
        // All four are in the trash for their files. Two have nothing of the user's any more:
        // they cannot be restored by hand, and are forgotten when the trash is emptied.
        for (qint64 id : {noted, drawn, late, edited}) {
            QVERIFY(Store::document(id).trashed);
            QVERIFY(Store::document(id).missing);
        }
        m_library->restoreFromTrash(ids({noted, drawn, late, edited}));
        QVERIFY(Store::document(noted).trashed);
        QVERIFY(Store::document(drawn).trashed);
        // The other two come back for what is attached to them, as documents without a file.
        QVERIFY(!Store::document(late).trashed);
        QVERIFY(Store::document(late).missing);
        // What was written in Indeks is the user's work and stays readable.
        QVERIFY(!Store::document(edited).trashed);
        QVERIFY(Store::document(edited).missing);
        QCOMPARE(m_library->readText(edited), u"My own version."_s);

        // When the note goes, the document it was kept for goes back into the trash; no scan
        // needed. Emptying the trash is what forgets it.
        m_library->removeNote(lateNote);
        QVERIFY2(Store::document(late).trashed, "a missing document stayed after its last note was removed");
        m_library->emptyTrash();
        QVERIFY(!Store::document(noted).isValid());
        QVERIFY(!Store::document(drawn).isValid());
        QVERIFY(!Store::document(late).isValid());

        // The same after its location is no longer indexed: kept for its tag, gone with it.
        m_library->addTag(ids({tagged}), u"keep"_s);
        m_library->removeSource(source);
        QVERIFY(Store::document(tagged).isValid());
        m_library->removeTag(ids({tagged}), u"keep"_s);
        QVERIFY2(!Store::document(tagged).isValid(), "a document of a removed location stayed after its last tag was removed");
        // Only Indeks forgets it: the file is the user's.
        QVERIFY(QFile::exists(folder.filePath(u"tagged.txt"_s)));

        // The edited one is still the user's work, until they discard it.
        QVERIFY(Store::document(edited).isValid());
        m_library->discardLocalCopy(edited);
        QVERIFY(!Store::document(edited).isValid());
    }

    void indexGivesSpaceBack()
    {
        // A new index can hand free pages back without being written out anew.
        QCOMPARE(Database::value(u"PRAGMA auto_vacuum"_s).toInt(), 2);
        auto indexSize = [] {
            Database::exec(u"PRAGMA wal_checkpoint(TRUNCATE)"_s);
            return QFileInfo(Paths::databaseFile()).size();
        };
        const int before = m_library->count(QString());
        const qint64 sizeBefore = indexSize();

        // Many documents with many different words …
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        for (int file = 0; file < 250; ++file) {
            QByteArray text;
            for (int word = 0; word < 2500; ++word) {
                text += "w" + QByteArray::number(file) + "x" + QByteArray::number(word) + ' ';
            }
            writeFile(folder.filePath(u"words-%1.txt"_s.arg(file)), text);
        }
        const qint64 source = m_library->addSource(QUrl::fromLocalFile(folder.path()));
        settle();
        QCOMPARE(m_library->count(u"sourceid:%1"_s.arg(source)), 250);
        const qint64 sizeFull = indexSize();
        QVERIFY2(sizeFull > sizeBefore + 4 * 1024 * 1024, qPrintable(QString::number(sizeFull - sizeBefore)));

        // … leave the file no smaller when they go: their pages are merely unused.
        m_library->removeSource(source);
        settle();
        QCOMPARE(m_library->count(QString()), before);
        QVERIFY(indexSize() > sizeBefore + 4 * 1024 * 1024);

        // Tidying merges the full text index and gives the pages back.
        const qint64 freed = Maintenance::tidy();
        QVERIFY2(freed > 4 * 1024 * 1024, qPrintable(QString::number(freed)));
        QCOMPARE(Database::value(u"PRAGMA freelist_count"_s).toInt(), 0);
        QVERIFY2(indexSize() < sizeBefore + 1024 * 1024, qPrintable(QString::number(indexSize() - sizeBefore)));
        // What remains is all there and can be searched as before.
        QCOMPARE(m_library->count(QString()), before);
        QCOMPARE(m_library->count(u"pineapple"_s), 1);
        QCOMPARE(m_library->count(u"w17x42"_s), 0);
    }

    // Keep this one last: it replaces the library.
    void stopsInTheMiddleOfIndexing()
    {
        QTemporaryDir many;
        QVERIFY(many.isValid());
        const int total = 6000;
        for (int i = 0; i < total; ++i) {
            writeFile(many.filePath(u"file-%1.txt"_s.arg(i)), QByteArray("words about gondolas ").repeated(40) + QByteArray::number(i));
        }
        const qint64 source = m_library->addSource(QUrl::fromLocalFile(many.path()));
        const QString scope = u"sourceid:%1"_s.arg(source);

        // Once the first files are in, the list of what is going on has the file being read at
        // the top and the waiting ones below it.
        QVERIFY(QTest::qWaitFor([&] { return m_library->count(scope) > 0; }, 20000));
        const QList<QueueItem> activity = m_library->activity();
        QVERIFY(activity.size() > 1);
        QVERIFY(activity.constFirst().isActive());
        QVERIFY(activity.constFirst().size > 0);
        QVERIFY(!activity.at(1).isActive());
        QVERIFY(activity.at(1).name.startsWith(u"file-"_s));

        // Stopping does not wait for the queue.
        QElapsedTimer timer;
        timer.start();
        QVERIFY(m_library->shutdown(10000));
        QVERIFY2(timer.elapsed() < 3000, qPrintable(QString::number(timer.elapsed())));
        const int indexed = m_library->count(scope);
        QVERIFY(indexed > 0);
        QVERIFY2(indexed < total, "the indexer was through before it could be stopped");

        // Had Indeks gone down while reading one document for text, that document is left
        // alone from the next start on instead of bringing Indeks down again and again.
        const qint64 culprit = Database::value(u"SELECT id FROM documents WHERE kind = 3 LIMIT 1"_s).toLongLong();
        QVERIFY(culprit > 0);
        Database::exec(u"UPDATE documents SET ocr = 1 WHERE id = ?"_s, {culprit});
        Database::exec(u"INSERT OR REPLACE INTO meta (key, value) VALUES ('ocr_at', ?)"_s, {QString::number(culprit)});

        // The next start picks up where this one left off.
        m_library.reset();
        m_library = std::make_unique<Library>();
        m_library->start(true);
        QVERIFY(QTest::qWaitFor([&] { return int(Store::document(culprit).ocr) == int(OcrState::Failed); }, 20000));
        QCOMPARE(Database::value(u"SELECT value FROM meta WHERE key = 'ocr_at'"_s).toString(), QString());
        QVERIFY(QTest::qWaitFor([&] { return m_library->count(scope) == total; }, 60000));
        settle();
        QVERIFY(m_library->activity().isEmpty());
    }
};

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // Make the helper that reads files fail on two file names, for filesThatBreakTheReader().
    qputenv("INDEKS_EXTRACTOR_TEST_CRASH", "crash-the-reader");
    qputenv("INDEKS_EXTRACTOR_TEST_HANG", "hang-the-reader");
    qputenv("INDEKS_EXTRACTOR_TIMEOUT_MS", "4000");
    QGuiApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    LibraryTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "librarytest.moc"
