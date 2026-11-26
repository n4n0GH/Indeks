// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ocr.h"

#include "indeks_core_debug.h"
#include "paths.h"
#include "task.h"

#include <QBuffer>
#include <QColorSpace>
#include <QThread>
#include <QTemporaryDir>
#include <QProcessEnvironment>
#include <QFile>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QImageReader>
#include <QMutex>
#include <QPainter>
#include <QPdfDocument>
#include <QProcess>
#include <QStandardPaths>

#include <algorithm>
#include <iterator>
#include <memory>
#include <vector>

#include <unistd.h>

using namespace Qt::StringLiterals;

namespace Indeks::Ocr
{

namespace
{
constexpr int MaxImageEdge = 4000;
constexpr int PageTimeoutMs = 180000;
constexpr double PdfRenderDpi = 200.0;
constexpr int MinTextEdge = 12;
constexpr qint64 MinTextArea = 64 * 64;

QString tesseractPath()
{
    static const QString path = QStandardPaths::findExecutable(u"tesseract"_s);
    return path;
}
}

bool isAvailable()
{
    return !tesseractPath().isEmpty();
}

QStringList installedLanguages()
{
    static QMutex mutex;
    static QStringList languages;
    static bool loaded = false;

    QMutexLocker locker(&mutex);
    if (loaded || !isAvailable()) {
        return languages;
    }
    loaded = true;
    QProcess process;
    process.start(tesseractPath(), {u"--list-langs"_s});
    if (!process.waitForFinished(10000)) {
        process.kill();
        return languages;
    }
    const QStringList lines = QString::fromUtf8(process.readAllStandardOutput()).split(u'\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        // The first line is a heading that ends with a colon; "osd" is not a language.
        if (trimmed.endsWith(u':') || trimmed.contains(u' ') || trimmed == "osd"_L1) {
            continue;
        }
        languages.append(trimmed);
    }
    languages.sort();
    return languages;
}

QString usableLanguages(const QString &wanted)
{
    const QStringList installed = installedLanguages();
    QStringList usable;
    const QStringList wantedList = wanted.split(u'+', Qt::SkipEmptyParts);
    for (const QString &language : wantedList) {
        if (installed.contains(language.trimmed()) && !usable.contains(language.trimmed())) {
            usable.append(language.trimmed());
        }
    }
    if (usable.isEmpty() && !installed.isEmpty()) {
        usable.append(installed.contains(u"eng"_s) ? u"eng"_s : installed.first());
    }
    return usable.join(u'+');
}

namespace
{
/** What tesseract gets to read: grey, not larger than it can make use of. */
QImage prepared(const QImage &source)
{
    QImage image = source;
    if (qMax(image.width(), image.height()) > MaxImageEdge) {
        image = image.scaled(MaxImageEdge, MaxImageEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (image.hasAlphaChannel()) {
        // Tesseract treats transparency as black, which hides dark text.
        QImage flattened(image.size(), QImage::Format_RGB32);
        flattened.fill(Qt::white);
        QPainter painter(&flattened);
        painter.drawImage(0, 0, image);
        painter.end();
        image = flattened;
    }
    // Tesseract works on grey levels anyway; handing them over directly makes for a third of
    // the data, written without the time compression takes.
    //
    // The colour profile goes first. With one, Qt converts by way of the profile, which reading
    // text gains nothing from, and which crashes on some embedded profiles (seen with a JPEG
    // carrying an "Apple Poppy Output Profile", Qt 6.12).
    image.setColorSpace(QColorSpace());
    return image.convertToFormat(QImage::Format_Grayscale8);
}

struct Running {
    int index = 0;
    QString file;
    QSize size; ///< of the image tesseract was given: what its places are counted in
    std::unique_ptr<QProcess> process;
    QElapsedTimer timer;
};
}

Page parseLayout(const QByteArray &tsv, const QSize &size)
{
    // level, page, block, paragraph, line, word, left, top, width, height, confidence, text;
    // the first row names them. A row of level 5 is a word; the others are what holds words.
    Page words;
    if (size.isEmpty()) {
        return words;
    }
    int line = -1;
    int lastLine[3] = {-1, -1, -1}; // block, paragraph, line of the word before
    int lastParagraph[2] = {-1, -1};
    const QList<QByteArray> rows = tsv.split('\n');
    for (const QByteArray &row : rows) {
        const QList<QByteArray> cells = row.split('\t');
        if (cells.size() < 12 || cells.at(0) != "5") {
            continue;
        }
        Word word;
        // A word that has a tab in it, should there ever be one, is still one word.
        word.text = QString::fromUtf8(cells.mid(11).join('\t')).trimmed();
        if (word.text.isEmpty()) {
            continue;
        }
        const int at[3] = {cells.at(2).toInt(), cells.at(3).toInt(), cells.at(4).toInt()};
        if (at[0] != lastLine[0] || at[1] != lastLine[1] || at[2] != lastLine[2]) {
            ++line;
            word.paragraph = line > 0 && (at[0] != lastParagraph[0] || at[1] != lastParagraph[1]);
            lastParagraph[0] = at[0];
            lastParagraph[1] = at[1];
            std::copy(std::begin(at), std::end(at), std::begin(lastLine));
        }
        word.line = line;
        const QRectF pixels(cells.at(6).toDouble(), cells.at(7).toDouble(), cells.at(8).toDouble(), cells.at(9).toDouble());
        word.box = QRectF(pixels.x() / size.width(), pixels.y() / size.height(), pixels.width() / size.width(), pixels.height() / size.height())
                       .intersected(QRectF(0, 0, 1, 1));
        if (word.box.isEmpty()) {
            continue;
        }
        words.append(word);
    }
    return words;
}

QString textOf(const Page &words)
{
    QString text;
    int line = -1;
    for (const Word &word : words) {
        if (word.line != line) {
            if (line >= 0) {
                text += word.paragraph ? "\n\n"_L1 : "\n"_L1;
            }
            line = word.line;
        } else {
            text += u' ';
        }
        text += word.text;
    }
    return text;
}

bool isTooSmallForText(const QSize &size)
{
    // Icons, bullets, spacers: nothing a line of text fits on.
    return size.isValid() && (qMin(size.width(), size.height()) < MinTextEdge || qint64(size.width()) * size.height() < MinTextArea);
}

int defaultParallelism()
{
    // Most of the cores, not all: the processes run at low priority, but the desktop and
    // the rest of Indeks should not have to wait for a core at all.
    return qMax(1, QThread::idealThreadCount() * 3 / 4);
}

QList<Recognized> recognizeAll(int count, const std::function<QImage(int)> &image, const QString &languages, int parallel, const std::function<void(int)> &progress)
{
    QList<Recognized> results(count);
    const QString usable = usableLanguages(languages);
    if (!isAvailable() || usable.isEmpty() || count <= 0) {
        return results;
    }
    parallel = qBound(1, parallel <= 0 ? defaultParallelism() : parallel, 64);

    // The images go to tesseract as files of our own in the cache; each is removed as soon as
    // it has been read.
    QDir().mkpath(Paths::cacheDir() + "/ocr"_L1);
    QTemporaryDir work(Paths::cacheDir() + "/ocr/work-XXXXXX"_L1);
    if (!work.isValid()) {
        return results;
    }
    // One thread per process: tesseract's own threads gain little, and what is gained here
    // comes from running a process on each core.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(u"OMP_THREAD_LIMIT"_s, u"1"_s);

    std::vector<Running> running;
    int next = 0;
    int done = 0;
    bool broken = false; // tesseract cannot be run at all
    auto finish = [&](Running &job, bool ok) {
        if (ok) {
            results[job.index].ok = true;
            results[job.index].words = parseLayout(job.process->readAllStandardOutput(), job.size);
            results[job.index].text = textOf(results[job.index].words);
        }
        QFile::remove(job.file);
        if (progress) {
            progress(++done);
        }
    };

    while ((next < count && !broken) || !running.empty()) {
        if (Task::aborted()) {
            for (Running &job : running) {
                job.process->kill();
                job.process->waitForFinished(2000);
            }
            return QList<Recognized>(count);
        }
        // Fill up to as many processes as may run at once.
        while (int(running.size()) < parallel && next < count && !broken) {
            const int index = next++;
            const QImage page = image ? image(index) : QImage();
            if (page.isNull()) {
                if (progress) {
                    progress(++done);
                }
                continue;
            }
            if (isTooSmallForText(page.size())) {
                results[index].ok = true; // looked at; there is nothing to read
                if (progress) {
                    progress(++done);
                }
                continue;
            }
            Running job;
            job.index = index;
            job.file = work.filePath(QString::number(index) + ".pgm"_L1);
            const QImage ready = prepared(page);
            job.size = ready.size();
            if (!ready.save(job.file, "PGM")) {
                if (progress) {
                    progress(++done);
                }
                continue;
            }
            job.process = std::make_unique<QProcess>();
            job.process->setProcessChannelMode(QProcess::SeparateChannels);
            job.process->setProcessEnvironment(environment);
            // Out of the way of whatever else the computer is doing.
            job.process->setChildProcessModifier([] {
                if (::nice(10) == -1) {
                    // Not being allowed to step back is no reason not to run.
                }
            });
            // As a table of the words and where they stand ("tsv"), which the text is put
            // together from as well; see parseLayout().
            job.process->start(tesseractPath(), {job.file, u"stdout"_s, u"-l"_s, usable, u"tsv"_s});
            if (!job.process->waitForStarted(10000)) {
                qCWarning(INDEKS_CORE) << "Cannot start tesseract:" << job.process->errorString();
                QFile::remove(job.file);
                broken = true;
                break;
            }
            job.process->closeWriteChannel();
            job.timer.start();
            running.push_back(std::move(job));
        }
        // See who is through. Each look also takes what the process has written so far.
        for (auto it = running.begin(); it != running.end();) {
            if (it->process->waitForFinished(running.size() > 1 ? 4 : 50)) {
                const bool ok = it->process->exitStatus() == QProcess::NormalExit && it->process->exitCode() == 0;
                if (!ok) {
                    qCInfo(INDEKS_CORE) << "tesseract failed:" << it->process->readAllStandardError().left(200);
                }
                finish(*it, ok);
                it = running.erase(it);
            } else if (it->timer.elapsed() > PageTimeoutMs || it->process->state() == QProcess::NotRunning) {
                it->process->kill();
                it->process->waitForFinished(2000);
                qCWarning(INDEKS_CORE) << "tesseract took too long or went away";
                finish(*it, false);
                it = running.erase(it);
            } else {
                ++it;
            }
        }
    }
    return results;
}

QString recognize(const QImage &source, const QString &languages, bool *ok)
{
    const QList<Recognized> result = recognizeAll(
        1,
        [&source](int) {
            return source;
        },
        languages,
        1);
    if (ok) {
        *ok = result.constFirst().ok;
    }
    return result.constFirst().text;
}

QImage readImageFile(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    // Vector images are rendered at their nominal size; huge ones are read smaller right away.
    const QSize size = reader.size();
    if (size.isValid() && qMax(size.width(), size.height()) > MaxImageEdge) {
        reader.setScaledSize(size.scaled(MaxImageEdge, MaxImageEdge, Qt::KeepAspectRatio));
    }
    return reader.read();
}

QString recognizeImageFile(const QString &path, const QString &languages, bool *ok, QList<Page> *layout)
{
    if (layout) {
        layout->clear();
    }
    const QImage image = readImageFile(path);
    if (image.isNull()) {
        if (ok) {
            *ok = false;
        }
        return {};
    }
    const QList<Recognized> result = recognizeAll(
        1,
        [&image](int) {
            return image;
        },
        languages,
        1);
    if (ok) {
        *ok = result.constFirst().ok;
    }
    if (layout && result.constFirst().ok) {
        layout->append(result.constFirst().words);
    }
    return result.constFirst().text;
}

QString recognizePdfFile(const QString &path, const QString &languages, int maxPages, int parallel, bool *ok, QList<Page> *layout)
{
    if (ok) {
        *ok = false;
    }
    if (layout) {
        layout->clear();
    }
    QPdfDocument document;
    if (document.load(path) != QPdfDocument::Error::None) {
        return {};
    }
    const int count = qMin(document.pageCount(), maxPages);
    // Pages are rendered here, one after the other, and read by tesseract several at a time.
    auto render = [&document](int page) {
        const QSizeF points = document.pagePointSize(page);
        QSize pixels = (points * (PdfRenderDpi / 72.0)).toSize();
        if (pixels.isEmpty()) {
            return QImage();
        }
        if (qMax(pixels.width(), pixels.height()) > MaxImageEdge) {
            pixels.scale(MaxImageEdge, MaxImageEdge, Qt::KeepAspectRatio);
        }
        return document.render(page, pixels);
    };
    const QList<Recognized> recognized = recognizeAll(count, render, languages, parallel, [count](int done) {
        if (count > 1) {
            Task::reportProgress(double(done) / count);
        }
    });
    QStringList pages;
    bool anyOk = false;
    for (const Recognized &page : recognized) {
        anyOk = anyOk || page.ok;
        if (!page.text.isEmpty()) {
            pages.append(page.text);
        }
        // A page for every page, also where nothing was read: they go by their numbers.
        if (layout) {
            layout->append(page.words);
        }
    }
    if (ok) {
        *ok = anyOk && !Task::aborted();
    }
    if (layout && (!anyOk || Task::aborted())) {
        layout->clear();
    }
    return pages.join("\n\n"_L1);
}

}
