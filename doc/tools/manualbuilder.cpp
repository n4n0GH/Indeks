// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

// Makes the manual, a PDF, from its chapters, which are Markdown files in doc/manual.
//
// The chapters are laid out one after the other in two columns, each starting on a new page
// under its title, with a list of what is in it. In front of them go a cover and a table of
// contents, and every page says at its foot which chapter and page it is. Only Qt is needed for it, so the manual is built
// wherever Indeks is, and it is built with Indeks: it cannot fall behind unnoticed.

#include <QAbstractTextDocumentLayout>
#include <QCommandLineParser>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QLocale>
#include <QMap>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextFrame>
#include <QTextList>
#include <QTextTable>

using namespace Qt::StringLiterals;

namespace
{
constexpr int Resolution = 300; // dots per inch the pages are laid out in
const QColor Accent(0x1f, 0x6f, 0xa8);
const QColor Faint(0x6b, 0x71, 0x76);
const QColor CodeBackground(0xf2, 0xf3, 0xf4);
const QColor NoteBackground(0xe9, 0xf3, 0xfa);
const QColor RuleColor(0xc9, 0xcd, 0xd1);

qreal mm(qreal millimetres)
{
    return millimetres / 25.4 * Resolution;
}

struct Chapter {
    QString title;
    QStringList sections;
    QString markdown; // without its title
};

/** A file of the manual: its first heading is the title of the chapter. */
Chapter readChapter(const QString &path)
{
    Chapter chapter;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qFatal("Cannot read %s", qPrintable(path));
    }
    QString text = QString::fromUtf8(file.readAll());
    // The note on who may copy the file is for those who read the file, not the manual.
    if (text.startsWith("<!--"_L1)) {
        const qsizetype end = text.indexOf("-->"_L1);
        if (end > 0) {
            text = text.mid(end + 3);
        }
    }
    QStringList rest;
    bool inCode = false;
    const QStringList lines = text.split(u'\n');
    for (const QString &line : lines) {
        if (line.startsWith("```"_L1)) {
            inCode = !inCode;
        }
        if (!inCode && chapter.title.isEmpty() && line.startsWith("# "_L1)) {
            chapter.title = line.mid(2).trimmed();
            continue;
        }
        if (!inCode && line.startsWith("## "_L1)) {
            chapter.sections.append(line.mid(3).trimmed());
        }
        rest.append(line);
    }
    if (chapter.title.isEmpty()) {
        qFatal("%s has no title: it must start with a line like \"# Title\"", qPrintable(path));
    }
    chapter.markdown = rest.join(u'\n');
    return chapter;
}

QFont bodyFont()
{
    QFont font(u"Noto Sans"_s);
    font.setPointSizeF(10);
    return font;
}

QTextCharFormat headingFormat(qreal points, const QColor &color)
{
    QTextCharFormat format;
    format.setFontPointSize(points);
    format.setFontWeight(QFont::DemiBold);
    format.setForeground(color);
    return format;
}

/** A table of titles with room for the pages they are on, which are filled in later. */
QTextTable *insertContents(QTextCursor &cursor, const QList<std::pair<QString, bool>> &entries, qreal points)
{
    QTextTableFormat format;
    format.setBorder(0);
    format.setCellPadding(mm(0.25));
    format.setCellSpacing(0);
    format.setWidth(QTextLength(QTextLength::PercentageLength, 100));
    format.setColumnWidthConstraints({QTextLength(QTextLength::PercentageLength, 88), QTextLength(QTextLength::PercentageLength, 12)});
    QTextTable *table = cursor.insertTable(int(entries.size()), 2, format);
    for (int row = 0; row < entries.size(); ++row) {
        const auto &[title, chapter] = entries.at(row);
        QTextCharFormat text;
        text.setFontPointSize(chapter ? points + 1 : points);
        text.setFontWeight(chapter ? QFont::DemiBold : QFont::Normal);
        text.setForeground(chapter ? Accent : QColor(Qt::black));
        QTextBlockFormat block;
        block.setLeftMargin(chapter ? 0 : mm(4));
        block.setTopMargin(chapter ? mm(1.6) : 0);
        QTextCursor left = table->cellAt(row, 0).firstCursorPosition();
        left.setBlockFormat(block);
        left.insertText(title, text);
        QTextBlockFormat number = block;
        number.setLeftMargin(0);
        number.setAlignment(Qt::AlignRight);
        QTextCursor right = table->cellAt(row, 1).firstCursorPosition();
        right.setBlockFormat(number);
        right.insertText(u"000"_s, text);
    }
    cursor.movePosition(QTextCursor::End);
    return table;
}

void setPage(QTextTable *table, int row, int page)
{
    QTextTableCell cell = table->cellAt(row, 1);
    QTextCursor cursor = cell.firstCursorPosition();
    const QTextCharFormat format = cursor.charFormat();
    cursor.setPosition(cell.lastCursorPosition().position(), QTextCursor::KeepAnchor);
    cursor.insertText(QString::number(page), format);
}

// ---------------------------------------------------------------------------------------------
// Getting around in the PDF
//
// Qt writes the pages but knows neither of places within a document to jump to nor of the
// outline that viewers show beside it as bookmarks. Both are added to the finished file
// here, the way the PDF format means later additions to be made: after the end of what is
// there, with a table of what was added and a pointer back to the table before it.

/** A place in the manual: a page, counted from 0, and how far up from its foot, in points. */
struct Place {
    int page = 0;
    qreal top = 0;
};
/** An area of a page, in points from its lower left corner, that leads to a place. */
struct Link {
    int page = 0;
    QRectF area;
    Place target;
};
struct Bookmark {
    QString title;
    int level = 1; // 1 for a chapter, 2 for a section of the one before it
    Place target;
};

/** A text for the viewer to show, in the one form that takes every letter. */
QByteArray pdfText(const QString &text)
{
    QByteArray bytes("FEFF");
    for (const QChar character : text) {
        bytes += QByteArray::number(character.unicode(), 16).rightJustified(4, '0').toUpper();
    }
    return '<' + bytes + '>';
}

class PdfUpdate
{
public:
    explicit PdfUpdate(const QString &path)
        : m_path(path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qFatal("Cannot read %s back", qPrintable(path));
        }
        m_data = file.readAll();

        // The table that says where each object of the file begins.
        m_tableAt = m_data.mid(m_data.lastIndexOf("startxref") + 9).trimmed().split('\n').constFirst().trimmed().toLongLong();
        qsizetype at = m_data.indexOf('\n', m_tableAt) + 1; // past "xref"
        const qsizetype trailerAt = m_data.indexOf("trailer", m_tableAt);
        while (at < trailerAt) {
            const QList<QByteArray> head = m_data.mid(at, m_data.indexOf('\n', at) - at).trimmed().split(' ');
            if (head.size() != 2) {
                break;
            }
            at = m_data.indexOf('\n', at) + 1;
            const int first = head.at(0).toInt();
            const int count = head.at(1).toInt();
            for (int i = 0; i < count; ++i, at += 20) {
                if (m_data.at(at + 17) == 'n') {
                    m_offsets.insert(first + i, m_data.mid(at, 10).toLongLong());
                }
            }
        }
        m_trailer = QString::fromLatin1(m_data.mid(trailerAt, m_data.indexOf("startxref", trailerAt) - trailerAt));
        m_next = number(m_trailer, u"/Size"_s);
        m_root = number(m_trailer, u"/Root"_s);
        if (m_offsets.isEmpty() || m_next <= 0 || m_root <= 0) {
            qFatal("%s is not laid out as expected; cannot add links and bookmarks", qPrintable(path));
        }
        const QString pagesObject = object(number(object(m_root), u"/Pages"_s));
        const QRegularExpression kids(u"/Kids\\s*\\[([^\\]]*)\\]"_s);
        static const QRegularExpression reference(u"(\\d+)\\s+0\\s+R"_s);
        auto it = reference.globalMatch(kids.match(pagesObject).captured(1));
        while (it.hasNext()) {
            m_pages.append(it.next().captured(1).toInt());
        }
    }

    int pageCount() const
    {
        return int(m_pages.size());
    }
    QByteArray pageReference(int page) const
    {
        return QByteArray::number(m_pages.value(page)) + " 0 R";
    }
    QByteArray destination(const Place &place) const
    {
        // From the left edge: some viewers go by the place only if both of its parts are given.
        return '[' + pageReference(place.page) + " /XYZ 0 " + QByteArray::number(place.top, 'f', 1) + " null]";
    }
    /** What is between "obj" and "endobj" of an object that is in the file. */
    QString object(int id) const
    {
        const qsizetype start = m_data.indexOf("obj", m_offsets.value(id)) + 3;
        return QString::fromLatin1(m_data.mid(start, m_data.indexOf("endobj", start) - start));
    }
    int reserve()
    {
        return m_next++;
    }
    /** An object that is new, or one that takes the place of the one with its number. */
    void put(int id, const QByteArray &body)
    {
        m_written.insert(id, body);
    }

    void addLinks(const QList<Link> &links)
    {
        QMap<int, QList<int>> byPage;
        for (const Link &link : links) {
            if (link.page < 0 || link.page >= pageCount() || link.target.page < 0 || link.target.page >= pageCount()) {
                continue;
            }
            const int id = reserve();
            const QRectF &area = link.area;
            put(id,
                "<< /Type /Annot /Subtype /Link /Border [0 0 0] /Rect [" + QByteArray::number(area.left(), 'f', 1) + ' ' + QByteArray::number(area.top(), 'f', 1) + ' '
                    + QByteArray::number(area.right(), 'f', 1) + ' ' + QByteArray::number(area.bottom(), 'f', 1) + "] /Dest " + destination(link.target) + " >>");
            byPage[link.page].append(id);
        }
        // A page lists what can be clicked on it: what Qt put there, and these.
        static const QRegularExpression indirect(u"/Annots\\s+(\\d+)\\s+0\\s+R"_s);
        static const QRegularExpression direct(u"/Annots\\s*\\[([^\\]]*)\\]"_s);
        for (auto page = byPage.constBegin(); page != byPage.constEnd(); ++page) {
            QString dictionary = object(m_pages.at(page.key())).trimmed();
            QString present;
            if (const auto match = indirect.match(dictionary); match.hasMatch()) {
                present = object(match.captured(1).toInt()).trimmed();
                present = present.mid(1, present.size() - 2); // without its brackets
                dictionary.remove(match.capturedStart(), match.capturedLength());
            } else if (const auto match = direct.match(dictionary); match.hasMatch()) {
                present = match.captured(1);
                dictionary.remove(match.capturedStart(), match.capturedLength());
            }
            QByteArray list = present.simplified().toLatin1();
            for (int id : page.value()) {
                list += ' ' + QByteArray::number(id) + " 0 R";
            }
            dictionary.insert(dictionary.lastIndexOf(u">>"_s), u"/Annots [%1 ]\n"_s.arg(QString::fromLatin1(list)));
            put(m_pages.at(page.key()), dictionary.toLatin1());
        }
    }

    void addBookmarks(const QList<Bookmark> &bookmarks)
    {
        if (bookmarks.isEmpty()) {
            return;
        }
        struct Item {
            int id;
            Bookmark bookmark;
            QList<int> children; // places in the list of items
            int parent = -1;
        };
        QList<Item> items;
        QList<int> chapters;
        for (const Bookmark &bookmark : bookmarks) {
            Item item{reserve(), bookmark, {}, -1};
            if (bookmark.level > 1 && !chapters.isEmpty()) {
                item.parent = chapters.constLast();
                items[item.parent].children.append(int(items.size()));
            } else {
                chapters.append(int(items.size()));
            }
            items.append(item);
        }
        const int outline = reserve();
        const auto reference = [&items](int index) {
            return QByteArray::number(items.at(index).id) + " 0 R";
        };
        for (int index = 0; index < items.size(); ++index) {
            const Item &item = items.at(index);
            const QList<int> &siblings = item.parent >= 0 ? items.at(item.parent).children : chapters;
            const int place = int(siblings.indexOf(index));
            QByteArray body = "<< /Title " + pdfText(item.bookmark.title) + " /Parent "
                + (item.parent >= 0 ? reference(item.parent) : QByteArray::number(outline) + " 0 R") + " /Dest " + destination(item.bookmark.target);
            if (place > 0) {
                body += " /Prev " + reference(siblings.at(place - 1));
            }
            if (place < siblings.size() - 1) {
                body += " /Next " + reference(siblings.at(place + 1));
            }
            if (!item.children.isEmpty()) {
                // A count below nought: the sections are there, folded away under the chapter.
                body += " /First " + reference(item.children.constFirst()) + " /Last " + reference(item.children.constLast()) + " /Count -"
                    + QByteArray::number(item.children.size());
            }
            put(item.id, body + " >>");
        }
        put(outline,
            "<< /Type /Outlines /First " + reference(chapters.constFirst()) + " /Last " + reference(chapters.constLast()) + " /Count "
                + QByteArray::number(chapters.size()) + " >>");

        // The document says that it has an outline, and that it is to be shown.
        QString catalog = object(m_root).trimmed();
        catalog.insert(catalog.lastIndexOf(u">>"_s), u"/Outlines %1 0 R\n/PageMode /UseOutlines\n"_s.arg(outline));
        put(m_root, catalog.toLatin1());
    }

    void write()
    {
        QByteArray added = "\n";
        QMap<int, qint64> offsets;
        for (auto object = m_written.constBegin(); object != m_written.constEnd(); ++object) {
            offsets.insert(object.key(), m_data.size() + added.size());
            added += QByteArray::number(object.key()) + " 0 obj\n" + object.value() + "\nendobj\n";
        }
        const qint64 tableAt = m_data.size() + added.size();
        added += "xref\n";
        // In runs of numbers that follow each other, as the format wants them.
        const QList<int> ids = offsets.keys();
        for (qsizetype start = 0; start < ids.size();) {
            qsizetype end = start + 1;
            while (end < ids.size() && ids.at(end) == ids.at(end - 1) + 1) {
                ++end;
            }
            added += QByteArray::number(ids.at(start)) + ' ' + QByteArray::number(end - start) + '\n';
            for (qsizetype i = start; i < end; ++i) {
                added += QByteArray::number(offsets.value(ids.at(i))).rightJustified(10, '0') + " 00000 n \n";
            }
            start = end;
        }
        static const QRegularExpression size(u"/Size\\s+\\d+"_s);
        QString trailer = m_trailer.trimmed();
        trailer.replace(size, u"/Size %1"_s.arg(m_next));
        trailer.insert(trailer.lastIndexOf(u">>"_s), u"/Prev %1\n"_s.arg(m_tableAt));
        added += trailer.toLatin1() + "\nstartxref\n" + QByteArray::number(tableAt) + "\n%%EOF\n";

        QFile file(m_path);
        if (!file.open(QIODevice::Append) || file.write(added) != added.size()) {
            qFatal("Cannot write the links and bookmarks to %s", qPrintable(m_path));
        }
    }

private:
    static int number(const QString &dictionary, const QString &key)
    {
        const QRegularExpression pattern(QRegularExpression::escape(key) + u"\\s+(\\d+)"_s);
        return pattern.match(dictionary).captured(1).toInt();
    }

    QString m_path;
    QByteArray m_data;
    qint64 m_tableAt = 0;
    QHash<int, qint64> m_offsets;
    QString m_trailer;
    int m_next = 0;
    int m_root = 0;
    QList<int> m_pages;
    QMap<int, QByteArray> m_written;
};

void collectTables(QTextFrame *frame, QList<QTextTable *> &tables)
{
    const QList<QTextFrame *> children = frame->childFrames();
    for (QTextFrame *child : children) {
        if (auto table = qobject_cast<QTextTable *>(child)) {
            tables.append(table);
        }
        collectTables(child, tables);
    }
}
}

// ---------------------------------------------------------------------------------------------
// Two columns
//
// The text is laid out as one long run of columns: what the document takes for a page is a
// column, and two of them are painted side by side on each page of the PDF. Whatever is to
// reach across both columns, the title of a chapter and pictures too wide for one, is not
// part of that run. It is painted over the top of a page by hand, and the run is kept clear
// of it by an empty block of its height at the top of both columns of that page.

constexpr int SpacerProperty = QTextFormat::UserProperty + 1;

// The height of a block of a set height is asked for as on a screen, in 96 to the inch, and
// brought to the resolution of the page by the layout; these go from the one to the other.
qreal asSet(qreal onPage)
{
    return onPage * 96.0 / Resolution;
}
qreal onPage(qreal asSet)
{
    return asSet * Resolution / 96.0;
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    QCommandLineParser parser;
    parser.addOptions({
        {u"output"_s, u"The PDF to write."_s, u"file"_s},
        {u"name"_s, u"What the manual is of."_s, u"name"_s, u"Indeks"_s},
        {u"release"_s, u"The version of it the manual is for."_s, u"version"_s},
        {u"logo"_s, u"A picture for the cover."_s, u"file"_s},
    });
    parser.addPositionalArgument(u"chapters"_s, u"The chapters, Markdown files, in their order."_s);
    parser.process(application);
    const QStringList files = parser.positionalArguments();
    if (files.isEmpty() || !parser.isSet(u"output"_s)) {
        qFatal("Usage: manualbuilder --output manual.pdf [--release 1.0] [--logo logo.svg] chapter.md …");
    }
    const QString name = parser.value(u"name"_s);
    const QString release = parser.value(u"release"_s);

    QList<Chapter> chapters;
    QDateTime changed;
    for (const QString &file : files) {
        chapters.append(readChapter(file));
        // The manual is as old as the chapter that was changed last, whenever it is built.
        changed = std::max(changed, QFileInfo(file).lastModified());
    }
    const QString baseDir = QFileInfo(files.constFirst()).absolutePath();

    QPdfWriter writer(parser.value(u"output"_s));
    writer.setResolution(Resolution);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
    writer.setTitle(release.isEmpty() ? u"%1 Manual"_s.arg(name) : u"%1 %2 Manual"_s.arg(name, release));
    writer.setCreator(name);

    const qreal pageWidth = writer.width();
    const qreal pageHeight = writer.height();
    const qreal left = mm(18);
    const qreal top = mm(20);
    const qreal bottom = mm(24);
    const qreal gutter = mm(9);
    const qreal fullWidth = pageWidth - 2 * left;
    // What the document takes for a page: one column.
    const QSizeF column((fullWidth - gutter) / 2, pageHeight - top - bottom);
    const qreal titleHeight = mm(30);

    QTextDocument document;
    document.documentLayout()->setPaintDevice(&writer);
    document.setDefaultFont(bodyFont());
    document.setDocumentMargin(0);
    document.setPageSize(column);
    document.setBaseUrl(QUrl::fromLocalFile(baseDir + u'/'));

    QTextCursor cursor(&document);
    const auto spacerFormat = [](qreal height, bool newColumn) {
        QTextBlockFormat format;
        format.setLineHeight(asSet(height), QTextBlockFormat::FixedHeight);
        format.setProperty(SpacerProperty, true);
        if (newColumn) {
            format.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
        }
        return format;
    };

    // ------------------------------------------------------------------------ contents
    QTextBlockFormat plainBlock;
    {
        QTextBlockFormat titleBlock;
        titleBlock.setBottomMargin(mm(4));
        cursor.setBlockFormat(titleBlock);
        cursor.insertText(u"Contents"_s, headingFormat(22, Accent));
        cursor.insertBlock(plainBlock, QTextCharFormat());
    }
    QList<std::pair<QString, bool>> allEntries;
    for (const Chapter &chapter : std::as_const(chapters)) {
        allEntries.append({chapter.title, true});
        for (const QString &section : chapter.sections) {
            allEntries.append({section, false});
        }
    }
    QTextTable *contents = insertContents(cursor, allEntries, 9);

    // ------------------------------------------------------------------------ chapters
    QList<QTextTable *> chapterContents;
    QList<QTextTable *> own = {contents};
    for (const Chapter &chapter : std::as_const(chapters)) {
        // The title is painted across the page; here is the room it takes in this column,
        // a heading for all that goes by headings, with nothing to show in it.
        QTextBlockFormat titleBlock = spacerFormat(titleHeight, true);
        titleBlock.setHeadingLevel(1);
        cursor.insertBlock(titleBlock, QTextCharFormat());

        if (!chapter.sections.isEmpty()) {
            QTextBlockFormat label;
            label.setBottomMargin(mm(1));
            QTextCharFormat labelText;
            labelText.setFontPointSize(8);
            labelText.setFontCapitalization(QFont::AllUppercase);
            labelText.setFontLetterSpacing(108);
            labelText.setForeground(Faint);
            cursor.insertBlock(label, labelText);
            cursor.insertText(u"In this chapter"_s);
            QList<std::pair<QString, bool>> entries;
            for (const QString &section : chapter.sections) {
                entries.append({section, false});
            }
            cursor.insertBlock(plainBlock, QTextCharFormat());
            QTextTable *table = insertContents(cursor, entries, 9);
            chapterContents.append(table);
            own.append(table);
        } else {
            chapterContents.append(nullptr);
        }

        QTextDocument part;
        part.setDefaultFont(bodyFont());
        part.setMarkdown(chapter.markdown, QTextDocument::MarkdownDialectGitHub);
        cursor.movePosition(QTextCursor::End);
        QTextBlockFormat gap;
        gap.setTopMargin(mm(3));
        cursor.insertBlock(gap, QTextCharFormat());
        cursor.insertFragment(QTextDocumentFragment(&part));
        cursor.movePosition(QTextCursor::End);
    }

    // ------------------------------------------------------------------------ pictures
    // A picture that fits a column stands in it, with what it shows written below. One that
    // is too wide for that is taken out of the text and painted across a page; where it
    // stood, a mark stays that says which page.
    struct Figure {
        QImage image;
        QString caption;
        QSizeF size; // as it is painted, across the page
        QTextCursor anchor; // where it stood in the text
        QTextCursor spacer; // the room made for it at the top of a page
        qreal offset = 0; // down from the top of that room, if it shares it
        qreal height() const
        {
            return size.height() + mm(12);
        }
    };
    QList<Figure> figures;
    struct Picture {
        int position;
        int length;
        QTextImageFormat format;
    };
    QList<Picture> pictures;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (fragment.charFormat().isImageFormat()) {
                pictures.append({fragment.position(), fragment.length(), fragment.charFormat().toImageFormat()});
            }
        }
    }
    for (auto picture = pictures.crbegin(); picture != pictures.crend(); ++picture) {
        QTextImageFormat format = picture->format;
        const QString path = QUrl::fromLocalFile(baseDir + u'/').resolved(QUrl(format.name())).toLocalFile();
        const QImage image(path);
        QTextCursor at(&document);
        at.setPosition(picture->position);
        at.setPosition(picture->position + picture->length, QTextCursor::KeepAnchor);
        if (image.isNull()) {
            qWarning("The picture %s is missing", qPrintable(path));
            at.removeSelectedText();
            continue;
        }
        const QString caption = format.stringProperty(QTextFormat::ImageAltText);
        // A picture of the screen is shown at about two thirds of its size on the screen.
        const qreal natural = image.width() / 144.0 * Resolution;
        // Too wide for a column: one that would have to shrink to less than three fifths.
        if (natural > column.width() * 1.7) {
            at.removeSelectedText();
            QTextBlockFormat mark;
            mark.setLineHeight(1, QTextBlockFormat::FixedHeight);
            at.setBlockFormat(mark);
            const qreal width = std::min(fullWidth, natural);
            figures.prepend({image, caption, QSizeF(width, width * image.height() / image.width()), at, QTextCursor(), 0});
            continue;
        }
        document.addResource(QTextDocument::ImageResource, QUrl(format.name()), image);
        // The size of a picture is asked for as on a screen, in 96 to the inch, and brought
        // to the resolution of the page by the layout.
        const qreal width = std::min(column.width(), natural) * 96.0 / Resolution;
        format.setWidth(width);
        format.setHeight(width * image.height() / image.width());
        at.setCharFormat(format);
        QTextBlockFormat centred = at.blockFormat();
        centred.setAlignment(Qt::AlignHCenter);
        centred.setTopMargin(mm(3));
        at.setBlockFormat(centred);
        if (!caption.isEmpty()) {
            at.clearSelection();
            at.movePosition(QTextCursor::EndOfBlock);
            QTextBlockFormat below;
            below.setAlignment(Qt::AlignHCenter);
            below.setTopMargin(mm(1.5));
            below.setBottomMargin(mm(4));
            QTextCharFormat small;
            small.setFontPointSize(8);
            small.setFontItalic(true);
            small.setForeground(Faint);
            at.insertBlock(below, small);
            at.insertText(caption);
        }
    }

    // ------------------------------------------------------------------------ looks
    struct Heading {
        QTextCursor at;
        int level;
        QString text;
    };
    QList<Heading> headings;
    int chapterIndex = 0;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        QTextCursor at(block);
        QTextBlockFormat format = block.blockFormat();
        if (format.boolProperty(SpacerProperty) || format.lineHeightType() == QTextBlockFormat::FixedHeight) {
            if (format.headingLevel() == 1) {
                headings.append({at, 1, chapters.at(chapterIndex++).title});
            }
            continue; // made above, as it is to be
        }
        const int level = format.headingLevel();
        if (level >= 2) {
            at.select(QTextCursor::BlockUnderCursor);
            QTextCharFormat text = headingFormat(level == 2 ? 14 : level == 3 ? 11 : 10, level == 2 ? Accent : QColor(Qt::black));
            at.mergeCharFormat(text);
            format.setTopMargin(level == 2 ? mm(6) : mm(4));
            format.setBottomMargin(mm(1.6));
            // A heading is not left alone at the foot of a column.
            format.setProperty(QTextFormat::BlockNonBreakableLines, true);
            QTextCursor(block).setBlockFormat(format);
            if (level == 2) {
                headings.append({QTextCursor(block), 2, block.text()});
            }
            continue;
        }
        if (at.currentTable() && own.contains(at.currentTable())) {
            continue;
        }
        if (format.hasProperty(QTextFormat::BlockCodeFence) || format.hasProperty(QTextFormat::BlockCodeLanguage)) {
            // Lines of code that are longer than a column is wide go on below.
            format.setBackground(CodeBackground);
            format.setLeftMargin(mm(1.5));
            format.setRightMargin(mm(1.5));
            at.setBlockFormat(format);
            at.select(QTextCursor::BlockUnderCursor);
            QTextCharFormat mono;
            mono.setFontFamilies({u"Hack"_s, u"Noto Sans Mono"_s, u"monospace"_s});
            mono.setFontPointSize(7.5);
            at.mergeCharFormat(mono);
            continue;
        }
        if (format.intProperty(QTextFormat::BlockQuoteLevel) > 0) {
            format.setBackground(NoteBackground);
            format.setLeftMargin(mm(2.5));
            format.setRightMargin(mm(2.5));
            format.setTopMargin(mm(1));
            format.setBottomMargin(mm(1));
            format.setLineHeight(125, QTextBlockFormat::ProportionalHeight);
            at.setBlockFormat(format);
            continue;
        }
        if (!at.currentTable() && block.length() > 1 && format.alignment() != Qt::AlignHCenter) {
            format.setLineHeight(126, QTextBlockFormat::ProportionalHeight);
            if (!block.textList()) {
                format.setBottomMargin(mm(2));
            } else {
                format.setTopMargin(0);
                format.setBottomMargin(mm(0.7));
            }
            at.setBlockFormat(format);
        }
    }
    // Text set as code within a line: the same fixed-width letters, a little smaller.
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().hasProperty(QTextFormat::BlockCodeFence)) {
            continue;
        }
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (fragment.charFormat().fontFixedPitch() && !fragment.charFormat().isImageFormat()) {
                QTextCursor at(&document);
                at.setPosition(fragment.position());
                at.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
                QTextCharFormat mono;
                mono.setFontFamilies({u"Hack"_s, u"Noto Sans Mono"_s, u"monospace"_s});
                mono.setFontPointSize(fragment.charFormat().fontPointSize() > 0 ? fragment.charFormat().fontPointSize() * 0.9 : 8.5);
                at.mergeCharFormat(mono);
            }
        }
    }
    // Tables of the chapters: ruled, with a head that stands out.
    QList<QTextTable *> tables;
    collectTables(document.rootFrame(), tables);
    for (QTextTable *table : std::as_const(tables)) {
        if (own.contains(table)) {
            continue;
        }
        QTextTableFormat format = table->format();
        format.setBorder(0.6);
        format.setBorderBrush(RuleColor);
        format.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
        format.setBorderCollapse(true);
        format.setCellPadding(mm(1));
        format.setCellSpacing(0);
        format.setWidth(QTextLength(QTextLength::PercentageLength, 100));
        format.setTopMargin(mm(2));
        format.setBottomMargin(mm(4));
        // The head is not said again where a table runs on into the next column: Qt does
        // that also where nothing of the table is left to run on, over what comes next.
        format.setHeaderRowCount(0);
        table->setFormat(format);
        for (int column = 0; column < table->columns(); ++column) {
            QTextTableCell cell = table->cellAt(0, column);
            QTextCharFormat head = cell.format();
            head.setBackground(CodeBackground);
            cell.setFormat(head);
            QTextCursor in = cell.firstCursorPosition();
            in.setPosition(cell.lastCursorPosition().position(), QTextCursor::KeepAnchor);
            QTextCharFormat bold;
            bold.setFontWeight(QFont::DemiBold);
            in.mergeCharFormat(bold);
        }
        for (int row = 0; row < table->rows(); ++row) {
            for (int column = 0; column < table->columns(); ++column) {
                QTextCursor in = table->cellAt(row, column).firstCursorPosition();
                in.setPosition(table->cellAt(row, column).lastCursorPosition().position(), QTextCursor::KeepAnchor);
                QTextCharFormat small;
                small.setFontPointSize(8.5);
                in.mergeCharFormat(small);
            }
        }
    }

    // ------------------------------------------------------------------------ room across the page
    const auto rectOf = [&](const QTextBlock &block) {
        return document.documentLayout()->blockBoundingRect(block);
    };
    // The column a block is in, or ends in: one that begins a column is said to begin where
    // the column before it left off.
    const auto columnOf = [&](const QTextBlock &block) {
        return int((rectOf(block).bottom() - 1) / column.height());
    };
    // Makes room at the top of a column: an empty block of that height, before whatever
    // stands there now. A paragraph or table that runs over from the column before goes into
    // this one whole, behind the room. Returns the block that is the room.
    const auto makeRoom = [&](int wanted, qreal height) {
        const qreal columnTop = wanted * column.height();
        QTextBlock first;
        for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
            if (rectOf(block).bottom() > columnTop + 1) {
                first = block;
                break;
            }
        }
        if (!first.isValid()) {
            // The text ends before this column: it is run on to it.
            QTextCursor end(&document);
            end.movePosition(QTextCursor::End);
            end.insertBlock(spacerFormat(height, true), QTextCharFormat());
            while (columnOf(end.block()) < wanted) {
                QTextCursor before(end.block());
                before.insertBlock(end.blockFormat(), QTextCharFormat());
                QTextCursor filler(before.block().previous());
                QTextBlockFormat empty;
                empty.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
                empty.setLineHeight(1, QTextBlockFormat::FixedHeight);
                filler.setBlockFormat(empty);
                end = QTextCursor(before.block());
            }
            return end.block();
        }
        if (first.blockFormat().boolProperty(SpacerProperty) && first.blockFormat().headingLevel() == 0) {
            // Room is there already, for something else; the two share it.
            QTextBlockFormat format = first.blockFormat();
            format.setLineHeight(format.lineHeight() + asSet(height), QTextBlockFormat::FixedHeight);
            QTextCursor(first).setBlockFormat(format);
            return first;
        }
        QTextCursor at(first);
        if (QTextTable *table = at.currentTable()) {
            // Not into the middle of a table: before all of it.
            at.setPosition(table->firstPosition() - 1);
            at.insertBlock(spacerFormat(height, true), QTextCharFormat());
            return at.block();
        }
        // The block that was there keeps what it is; a new one before it is the room.
        const QTextBlockFormat format = first.blockFormat();
        const QTextCharFormat text = first.charFormat();
        at.movePosition(QTextCursor::StartOfBlock);
        at.insertBlock(format, text);
        QTextBlock room = at.block().previous();
        if (QTextList *list = room.textList()) {
            list->remove(room);
        }
        QTextCursor(room).setBlockFormat(spacerFormat(height, true));
        return room;
    };

    // In the order of the text, since room that is made moves what comes after it.
    struct Banner {
        int position;
        int chapter; // or -1
        int figure; // or -1
    };
    QList<Banner> banners;
    for (int i = 0, chapter = 0; i < headings.size(); ++i) {
        if (headings.at(i).level == 1) {
            banners.append({headings.at(i).at.position(), chapter++, -1});
        }
    }
    for (int i = 0; i < figures.size(); ++i) {
        banners.append({figures.at(i).anchor.position(), -1, i});
    }
    std::sort(banners.begin(), banners.end(), [](const Banner &a, const Banner &b) {
        return a.position < b.position;
    });
    QList<QTextCursor> chapterStarts;
    for (const Heading &heading : std::as_const(headings)) {
        if (heading.level == 1) {
            chapterStarts.append(heading.at);
        }
    }
    for (const Banner &banner : std::as_const(banners)) {
        if (banner.chapter >= 0) {
            // A chapter begins a page, which is to say in a left column: if it would begin
            // in a right one, that column is left empty.
            const QTextCursor &start = chapterStarts.at(banner.chapter);
            if (columnOf(start.block()) % 2 == 1) {
                const QTextBlockFormat format = start.blockFormat();
                QTextCursor at(start.block());
                at.movePosition(QTextCursor::StartOfBlock);
                at.insertBlock(format, QTextCharFormat());
                QTextBlockFormat empty;
                empty.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
                empty.setLineHeight(1, QTextBlockFormat::FixedHeight);
                QTextCursor(at.block().previous()).setBlockFormat(empty);
            }
            // The title reaches over the right column too, if the chapter gets that far.
            const int leftColumn = columnOf(start.block());
            const int next = banner.chapter + 1 < chapterStarts.size() ? chapterStarts.at(banner.chapter + 1).position() : document.characterCount();
            bool reaches = false;
            for (QTextBlock block = start.block().next(); block.isValid() && block.position() < next; block = block.next()) {
                if (rectOf(block).bottom() > (leftColumn + 1) * column.height() + 1) {
                    reaches = true;
                    break;
                }
            }
            if (reaches) {
                makeRoom(leftColumn + 1, titleHeight);
            }
        } else {
            // A wide picture goes to the top of the page after the one it is spoken of on.
            Figure &figure = figures[banner.figure];
            const int page = columnOf(figure.anchor.block()) / 2 + 1;
            QTextBlock room = makeRoom(page * 2, figure.height());
            figure.offset = onPage(room.blockFormat().lineHeight()) - figure.height();
            figure.spacer = QTextCursor(room);
            makeRoom(columnOf(room) + 1, figure.height());
        }
    }

    // ------------------------------------------------------------------------ pages
    // The cover is page 1; the first two columns are page 2.
    const auto pageOf = [&](const QTextCursor &at) {
        return columnOf(at.block()) / 2 + 2;
    };
    // Twice: the numbers take their room, and what follows may move for it.
    for (int round = 0; round < 2; ++round) {
        int entry = 0;
        int chapter = -1;
        int section = 0;
        for (const Heading &heading : std::as_const(headings)) {
            const int page = pageOf(heading.at);
            setPage(contents, entry++, page);
            if (heading.level == 1) {
                ++chapter;
                section = 0;
            } else if (chapter >= 0 && chapterContents.at(chapter)) {
                setPage(chapterContents.at(chapter), section++, page);
            }
        }
    }
    const int columns = document.pageCount();
    const int pages = (columns + 1) / 2;

    // Where things are on the finished pages, for the links and bookmarks: in points, up
    // from the foot of the page, as the PDF counts. The cover is the first page.
    const qreal points = 72.0 / Resolution;
    const qreal sheetHeight = pageHeight * points;
    const auto columnLeft = [&](int index) {
        return left + (index % 2) * (column.width() + gutter);
    };
    const auto placeOf = [&](const QTextBlock &block, bool chapter) {
        const int index = columnOf(block);
        if (chapter) {
            return Place{index / 2 + 1, sheetHeight};
        }
        const qreal down = top + rectOf(block).top() - index * column.height();
        // A little above the heading, so that it does not stand right at the edge.
        return Place{index / 2 + 1, std::min(sheetHeight, sheetHeight - down * points + 14)};
    };
    const auto areaOf = [&](const QTextBlock &block, int *page) {
        const QRectF rect = rectOf(block);
        const int index = columnOf(block);
        const qreal down = top + rect.top() - index * column.height();
        *page = index / 2 + 1;
        // Across the whole line, title and number alike.
        return QRectF(QPointF(columnLeft(index) * points, sheetHeight - (down + rect.height()) * points),
                      QPointF((columnLeft(index) + column.width()) * points, sheetHeight - down * points));
    };
    QList<Link> links;
    QList<Bookmark> bookmarks;
    bookmarks.append({u"Contents"_s, 1, Place{1, sheetHeight}});
    {
        int entry = 0;
        int chapter = -1;
        int section = 0;
        const auto link = [&](QTextTable *table, int row, const Place &target) {
            Link made;
            made.area = areaOf(table->cellAt(row, 0).firstCursorPosition().block(), &made.page);
            made.target = target;
            links.append(made);
        };
        for (const Heading &heading : std::as_const(headings)) {
            const Place place = placeOf(heading.at.block(), heading.level == 1);
            bookmarks.append({heading.text, heading.level, place});
            link(contents, entry++, place);
            if (heading.level == 1) {
                ++chapter;
                section = 0;
            } else if (chapter >= 0 && chapterContents.at(chapter)) {
                link(chapterContents.at(chapter), section++, place);
            }
        }
    }

    QPainter painter(&writer);
    // The cover.
    {
        qreal y = pageHeight * 0.2;
        if (parser.isSet(u"logo"_s)) {
            const int side = int(mm(52));
            QImageReader reader(parser.value(u"logo"_s));
            reader.setScaledSize(QSize(side, side));
            const QImage logo = reader.read();
            if (!logo.isNull()) {
                painter.drawImage(QPointF((pageWidth - side) / 2, y), logo);
            }
            y += side + mm(14);
        }
        QFont big = bodyFont();
        big.setPointSizeF(44);
        big.setWeight(QFont::DemiBold);
        painter.setFont(big);
        painter.setPen(Accent);
        painter.drawText(QRectF(0, y, pageWidth, mm(22)), Qt::AlignHCenter | Qt::AlignVCenter, name);
        y += mm(24);
        QFont medium = bodyFont();
        medium.setPointSizeF(15);
        medium.setLetterSpacing(QFont::PercentageSpacing, 112);
        medium.setCapitalization(QFont::AllUppercase);
        painter.setFont(medium);
        painter.setPen(Faint);
        painter.drawText(QRectF(0, y, pageWidth, mm(9)), Qt::AlignHCenter | Qt::AlignVCenter, u"Manual"_s);
        y += mm(11);
        if (!release.isEmpty()) {
            painter.drawText(QRectF(0, y, pageWidth, mm(9)), Qt::AlignHCenter | Qt::AlignVCenter, u"Version %1"_s.arg(release));
        }
        QFont small = bodyFont();
        small.setPointSizeF(9);
        painter.setFont(small);
        painter.drawText(QRectF(0, pageHeight - mm(36), pageWidth, mm(8)),
                         Qt::AlignHCenter | Qt::AlignVCenter,
                         u"Last changed on %1"_s.arg(QLocale(QLocale::English).toString(changed.date(), u"d MMMM yyyy"_s)));
    }

    QFont footFont = bodyFont();
    footFont.setPointSizeF(8);
    QFont titleFont = bodyFont();
    titleFont.setPointSizeF(27);
    titleFont.setWeight(QFont::DemiBold);
    QFont captionFont = bodyFont();
    captionFont.setPointSizeF(8);
    captionFont.setItalic(true);
    const QString foot = release.isEmpty() ? u"%1 Manual"_s.arg(name) : u"%1 %2 Manual"_s.arg(name, release);
    for (int page = 0; page < pages; ++page) {
        writer.newPage();
        for (int side = 0; side < 2; ++side) {
            const int index = page * 2 + side;
            if (index >= columns) {
                break;
            }
            painter.save();
            painter.translate(columnLeft(index), top - index * column.height());
            QAbstractTextDocumentLayout::PaintContext context;
            context.clip = QRectF(0, index * column.height(), column.width(), column.height());
            context.palette.setColor(QPalette::Text, Qt::black);
            painter.setClipRect(context.clip);
            document.documentLayout()->draw(&painter, context);
            painter.restore();
        }

        // What reaches across both columns of this page: the title of a chapter that begins
        // here, and pictures.
        QString chapter;
        for (const Heading &heading : std::as_const(headings)) {
            if (heading.level != 1) {
                continue;
            }
            if (pageOf(heading.at) <= page + 2) {
                chapter = heading.text; // the last one that began on this page or before
            }
            if (pageOf(heading.at) == page + 2) {
                painter.setFont(titleFont);
                painter.setPen(Accent);
                painter.drawText(QRectF(left, top, fullWidth, titleHeight - mm(12)), Qt::AlignHCenter | Qt::AlignVCenter, heading.text);
                painter.setPen(QPen(RuleColor, mm(0.2)));
                painter.drawLine(QPointF(left, top + titleHeight - mm(8)), QPointF(left + fullWidth, top + titleHeight - mm(8)));
            }
        }
        for (const Figure &figure : std::as_const(figures)) {
            if (figure.spacer.isNull() || columnOf(figure.spacer.block()) / 2 != page) {
                continue;
            }
            const qreal y = top + figure.offset;
            const QRectF where(left + (fullWidth - figure.size.width()) / 2, y, figure.size.width(), figure.size.height());
            painter.drawImage(where, figure.image);
            painter.setPen(QPen(RuleColor, mm(0.15)));
            painter.drawRect(where);
            painter.setFont(captionFont);
            painter.setPen(Faint);
            painter.drawText(QRectF(left, where.bottom() + mm(1.5), fullWidth, mm(5)), Qt::AlignHCenter | Qt::AlignVCenter, figure.caption);
        }

        const QRectF line(left, pageHeight - mm(15), fullWidth, mm(6));
        painter.setPen(QPen(RuleColor, mm(0.15)));
        painter.drawLine(line.topLeft(), line.topRight());
        painter.setFont(footFont);
        painter.setPen(Faint);
        painter.drawText(line, Qt::AlignLeft | Qt::AlignVCenter, chapter);
        painter.drawText(line, Qt::AlignRight | Qt::AlignVCenter, u"%1, page %2"_s.arg(foot).arg(page + 2));
    }
    painter.end();

    // The file is whole now. What makes it one to find one's way in is added to it.
    PdfUpdate update(parser.value(u"output"_s));
    if (update.pageCount() != pages + 1) {
        qFatal("The PDF has %d pages where %d were made", update.pageCount(), pages + 1);
    }
    update.addLinks(links);
    update.addBookmarks(bookmarks);
    update.write();
    return 0;
}
