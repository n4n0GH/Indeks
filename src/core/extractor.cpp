// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "extractor.h"

#include "canvas.h"
#include "email.h"
#include "indeks_core_debug.h"
#include "ocr.h"
#include "office.h"
#include "task.h"
#include "weblink.h"

#include <KFileMetaData/Extractor>
#include <KFileMetaData/ExtractorCollection>
#include <KFileMetaData/PropertyInfo>
#include <KFileMetaData/SimpleExtractionResult>

#include <QCryptographicHash>
#include <QFile>
#include <QImageReader>
#include <QFileInfo>
#include <QLocale>
#include <QStringDecoder>

using namespace Qt::StringLiterals;

namespace Indeks
{

namespace
{
constexpr qsizetype MaxTextLength = 8 * 1024 * 1024; // characters kept per document
constexpr qint64 FullHashLimit = 64LL * 1024 * 1024;
constexpr int IndexConversionTimeoutMs = 60000;
constexpr qint64 PartialHashChunk = 4LL * 1024 * 1024;

QString decodeText(const QByteArray &data, bool html)
{
    if (html) {
        QStringDecoder decoder = QStringDecoder::decoderForHtml(data);
        return decoder(data);
    }
    if (const auto encoding = QStringConverter::encodingForData(data)) {
        QStringDecoder decoder(*encoding);
        return decoder(data);
    }
    QStringDecoder utf8(QStringConverter::Utf8);
    const QString text = utf8(data);
    if (!utf8.hasError()) {
        return text;
    }
    return QString::fromLatin1(data);
}

void extractWithKFileMetaData(const QString &path, const QString &mimeType, Extracted *result)
{
    // Loading the extractor plugins is expensive, so keep one collection per thread.
    static thread_local KFileMetaData::ExtractorCollection collection;

    const QList<KFileMetaData::Extractor *> extractors = collection.fetchExtractors(mimeType);
    if (extractors.isEmpty()) {
        return;
    }
    KFileMetaData::SimpleExtractionResult extraction(path,
                                                     mimeType,
                                                     KFileMetaData::ExtractionResult::ExtractMetaData | KFileMetaData::ExtractionResult::ExtractPlainText);
    for (KFileMetaData::Extractor *extractor : extractors) {
        extractor->extract(&extraction);
    }
    result->text = extraction.text();

    const auto properties = extraction.properties();
    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        const KFileMetaData::PropertyInfo info(it.key());
        if (it.key() == KFileMetaData::Property::PageCount) {
            result->pages = it.value().toInt();
        }
        const QString value = info.formatAsDisplayString(it.value()).trimmed();
        if (value.isEmpty() || info.name().isEmpty()) {
            continue;
        }
        QString &slot = result->metadata[info.name()];
        slot = slot.isEmpty() ? value : slot + ", "_L1 + value;
    }
}

void describeEmails(const QList<Email> &emails, Extracted *result)
{
    QStringList parts;
    for (const Email &email : emails) {
        parts << email.subject << email.from << email.to << email.cc << email.bcc << email.textBody;
        for (const EmailAttachment &attachment : email.attachments) {
            parts << attachment.name;
        }
    }
    parts.removeAll(QString());
    result->text = parts.join(u'\n');

    if (!emails.isEmpty()) {
        const Email &first = emails.first();
        auto add = [result](const QString &key, const QString &value) {
            if (!value.isEmpty()) {
                result->metadata.insert(key, value);
            }
        };
        add(u"subject"_s, first.subject);
        add(u"emailFrom"_s, first.from);
        add(u"emailTo"_s, first.to);
        add(u"emailCc"_s, first.cc);
        if (first.date.isValid()) {
            add(u"emailDate"_s, QLocale().toString(first.date.toLocalTime(), QLocale::ShortFormat));
        }
        if (emails.size() > 1) {
            add(u"emailCount"_s, QString::number(emails.size()));
        }
        int attachments = 0;
        for (const Email &email : emails) {
            attachments += email.attachments.size();
        }
        if (attachments > 0) {
            add(u"emailAttachments"_s, QString::number(attachments));
        }
    }
}

void extractEmail(const QString &path, Extracted *result)
{
    describeEmails(readEmails(path), result);
}
}

Extracted extractMessage(const QByteArray &raw)
{
    Extracted result;
    const Email email = readEmail(raw);
    if (email.valid) {
        describeEmails({email}, &result);
        if (email.date.isValid()) {
            result.metadata.insert(u"emailDateSecs"_s, QString::number(email.date.toSecsSinceEpoch()));
        }
    }
    if (result.text.size() > MaxTextLength) {
        result.text.truncate(MaxTextLength);
    }
    return result;
}

QString readTextFile(const QString &path, qint64 maxBytes)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray data = file.read(maxBytes);
    return decodeText(data, false);
}

QString rtfToPlainText(const QByteArray &rtf)
{
    static const QSet<QByteArray> skippedDestinations = {
        "fonttbl",   "colortbl", "stylesheet", "info",   "pict",     "object",    "header",   "footer",    "headerl", "headerr",
        "footerl",   "footerr",  "footnote",   "themedata", "datastore", "generator", "listtable", "listoverridetable", "rsidtbl", "xmlnstbl",
        "latentstyles", "fldinst",
    };

    QString out;
    QList<bool> skipStack;
    bool skip = false;
    int unicodeSkip = 1; // bytes to skip after a \u escape
    int pendingSkip = 0;
    QByteArray pendingBytes; // \'hh escapes, decoded in one go

    auto flushBytes = [&] {
        if (!pendingBytes.isEmpty()) {
            if (!skip) {
                out += QString::fromLatin1(pendingBytes);
            }
            pendingBytes.clear();
        }
    };

    const qsizetype n = rtf.size();
    for (qsizetype i = 0; i < n; ++i) {
        if ((i & 0xffff) == 0 && Task::aborted()) {
            break;
        }
        const char c = rtf[i];
        if (c == '{') {
            flushBytes();
            skipStack.append(skip);
        } else if (c == '}') {
            flushBytes();
            skip = skipStack.isEmpty() ? false : skipStack.takeLast();
        } else if (c == '\\') {
            if (i + 1 >= n) {
                break;
            }
            const char next = rtf[i + 1];
            if (next == '\\' || next == '{' || next == '}') {
                flushBytes();
                if (!skip && pendingSkip == 0) {
                    out += QLatin1Char(next);
                }
                pendingSkip = qMax(0, pendingSkip - 1);
                ++i;
            } else if (next == '\'') {
                if (i + 3 < n) {
                    bool ok = false;
                    const char byte = char(rtf.mid(i + 2, 2).toInt(&ok, 16));
                    if (pendingSkip > 0) {
                        --pendingSkip;
                    } else if (ok) {
                        pendingBytes.append(byte);
                    }
                }
                i += 3;
            } else if (next == '*') {
                flushBytes();
                skip = true; // optional destination we do not know
                ++i;
            } else if (next == '~') {
                flushBytes();
                if (!skip) {
                    out += QChar(0x00a0);
                }
                ++i;
            } else if (std::isalpha(static_cast<unsigned char>(next))) {
                flushBytes();
                qsizetype j = i + 1;
                while (j < n && std::isalpha(static_cast<unsigned char>(rtf[j]))) {
                    ++j;
                }
                const QByteArray word = rtf.mid(i + 1, j - i - 1);
                qsizetype k = j;
                if (k < n && (rtf[k] == '-' || std::isdigit(static_cast<unsigned char>(rtf[k])))) {
                    ++k;
                    while (k < n && std::isdigit(static_cast<unsigned char>(rtf[k]))) {
                        ++k;
                    }
                }
                const bool hasParameter = k > j;
                const int parameter = hasParameter ? rtf.mid(j, k - j).toInt() : 0;
                if (k < n && rtf[k] == ' ') {
                    ++k; // the delimiting space belongs to the control word
                }
                i = k - 1;

                if (skippedDestinations.contains(word)) {
                    skip = true;
                } else if (word == "par" || word == "line" || word == "sect" || word == "page" || word == "row") {
                    if (!skip) {
                        out += u'\n';
                    }
                } else if (word == "tab" || word == "cell") {
                    if (!skip) {
                        out += u'\t';
                    }
                } else if (word == "uc" && hasParameter) {
                    unicodeSkip = parameter;
                } else if (word == "u" && hasParameter) {
                    if (!skip) {
                        out += QChar(char16_t(parameter < 0 ? parameter + 65536 : parameter));
                    }
                    pendingSkip = unicodeSkip;
                }
            } else {
                ++i; // other control symbols
            }
        } else if (c == '\r' || c == '\n') {
            // Line breaks in the source carry no meaning.
        } else {
            flushBytes();
            if (pendingSkip > 0) {
                --pendingSkip;
            } else if (!skip) {
                out += QLatin1Char(c);
            }
        }
    }
    flushBytes();
    return out.trimmed();
}

QString hashFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha1);
    const qint64 size = file.size();
    if (size <= FullHashLimit) {
        if (!hash.addData(&file)) {
            return {};
        }
    } else {
        // Hashing only both ends keeps huge files cheap; the size goes in to tell them apart.
        hash.addData(file.read(PartialHashChunk));
        file.seek(size - PartialHashChunk);
        hash.addData(file.read(PartialHashChunk));
        hash.addData(QByteArray::number(size));
    }
    return QString::fromLatin1(hash.result().toHex());
}

Extracted extractContent(const QString &path, const QString &mimeType, Kind kind, const IndexOptions &options)
{
    Extracted result;
    const QFileInfo info(path);
    if (!info.isFile() || !info.isReadable()) {
        return result;
    }
    if (isContainerMimeType(mimeType)) {
        // A mailbox is its messages, and those are read one by one (see Indexing::Reading).
        return result;
    }
    const bool tooLarge = info.size() > qint64(options.maxContentSizeMb) * 1024 * 1024;
    if (!options.indexContent || tooLarge) {
        return result;
    }

    if (kind == Kind::Email) {
        extractEmail(path, &result);
    } else if (kind == Kind::Canvas) {
        // What is written on the cards, not the JSON around it.
        const Canvas canvas = readCanvas(path);
        result.text = canvasText(canvas);
        if (canvas.valid) {
            result.metadata.insert(u"canvasCards"_s, QString::number(canvas.nodes.size()));
            result.metadata.insert(u"canvasConnections"_s, QString::number(canvas.edges.size()));
        }
    } else if (kind == Kind::Website) {
        // The address is all there is to find such a document by, beside its name.
        const QUrl address = WebLink::read(path);
        if (address.isValid()) {
            result.text = address.toDisplayString();
            result.metadata.insert(u"webAddress"_s, result.text);
        }
    } else if (mimeType == "text/html"_L1 || mimeType == "application/xhtml+xml"_L1) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            result.text = htmlToPlainText(decodeText(file.readAll(), true));
        }
    } else if (mimeType == "text/rtf"_L1 || mimeType == "application/rtf"_L1) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            result.text = rtfToPlainText(file.readAll());
        }
    } else if (kind == Kind::PlainText) {
        // Read text files ourselves: the encoding guesses of the generic extractor
        // turn short files into nonsense now and then.
        result.text = readTextFile(path);
    } else {
        extractWithKFileMetaData(path, mimeType, &result);
        if (result.text.trimmed().isEmpty() && options.officeConversion && Office::isOfficeDocument(mimeType) && Office::converterAvailable()) {
            // The old binary formats of Word, Excel and PowerPoint, mostly: nothing here reads
            // them. LibreOffice does; the text is taken from the PDF it makes, which is also
            // what is shown when the document is looked at.
            const QString pdf = Office::renderToPdf(path, hashFile(path), options.officeProfile, IndexConversionTimeoutMs);
            if (!pdf.isEmpty()) {
                Extracted rendition;
                extractWithKFileMetaData(pdf, u"application/pdf"_s, &rendition);
                result.text = rendition.text;
            }
        }
    }

    if (result.text.size() > MaxTextLength) {
        result.text.truncate(MaxTextLength);
    }

    if (kind == Kind::Image) {
        result.wantsOcr = mimeType != "image/svg+xml"_L1 && mimeType != "image/svg+xml-compressed"_L1
            // Icons and the like: looking at the size is free, looking for text in them is not.
            && !Ocr::isTooSmallForText(QImageReader(path).size());
    } else if (kind == Kind::Pdf) {
        // A PDF with next to no text per page is a scan.
        const qsizetype letters = std::count_if(result.text.cbegin(), result.text.cend(), [](QChar c) {
            return c.isLetterOrNumber();
        });
        result.wantsOcr = letters < 24 * qMax(1, result.pages);
    }
    return result;
}

}
