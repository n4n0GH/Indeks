// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "email.h"

#include "task.h"

#include <KMime/Content>
#include <KMime/Message>
#include <KMime/Util>

#include <QFile>
#include <QHash>
#include <QSet>

using namespace Qt::StringLiterals;

namespace Indeks
{

namespace
{
constexpr qint64 MaxMailFileSize = 256LL * 1024 * 1024;

/** Splits an mbox file at its "From " separator lines. A file without separators is one message. */
QList<QByteArray> splitMessages(const QByteArray &data, int maxMessages)
{
    QList<QByteArray> messages;
    if (!data.startsWith("From ")) {
        messages.append(data);
        return messages;
    }
    qsizetype start = 0;
    while (start < data.size() && messages.size() < maxMessages) {
        // Skip the separator line itself.
        const qsizetype lineEnd = data.indexOf('\n', start);
        if (lineEnd < 0) {
            break;
        }
        qsizetype next = data.indexOf("\nFrom ", lineEnd);
        const qsizetype end = next < 0 ? data.size() : next + 1;
        messages.append(data.mid(lineEnd + 1, end - lineEnd - 1));
        start = end;
    }
    return messages;
}

void collectParts(KMime::Content *content, Email *email, QList<KMime::Content *> *attachments)
{
    const auto children = content->contents();
    if (!children.isEmpty()) {
        for (KMime::Content *child : children) {
            collectParts(child, email, attachments);
        }
        return;
    }

    const KMime::Headers::ContentType *type = std::as_const(*content).contentType();
    const KMime::Headers::ContentDisposition *disposition = std::as_const(*content).contentDisposition();
    QString fileName = disposition ? disposition->filename() : QString();
    if (fileName.isEmpty() && type) {
        fileName = type->name();
    }
    const bool isText = !type || type->isPlainText() || type->isHTMLText();
    const bool isAttachment = KMime::isAttachment(content) || (!fileName.isEmpty() && !isText);

    if (isAttachment || !isText) {
        if (attachments) {
            attachments->append(content);
        }
        if (email) {
            EmailAttachment attachment;
            attachment.name = fileName.isEmpty() ? u"attachment"_s : fileName;
            attachment.mimeType = type ? QString::fromLatin1(type->mimeType()) : u"application/octet-stream"_s;
            attachment.size = content->decodedBody().size();
            email->attachments.append(attachment);
        }
        return;
    }
    if (!email) {
        return;
    }
    const QString text = content->decodedText(KMime::Content::TrimSpaces);
    if (type && type->isHTMLText()) {
        if (email->htmlBody.isEmpty()) {
            email->htmlBody = text;
        }
    } else if (email->textBody.isEmpty()) {
        email->textBody = text;
    } else {
        email->textBody += u'\n' + text;
    }
}

std::unique_ptr<KMime::Message> parseMessage(const QByteArray &raw)
{
    auto message = std::make_unique<KMime::Message>();
    message->setContent(KMime::CRLFtoLF(raw));
    message->parse();
    return message;
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > MaxMailFileSize) {
        return {};
    }
    return file.readAll();
}
}

namespace
{
/** Decodes the character reference that starts at the '&' at @p pos; moves @p pos behind it. */
QString decodeEntity(const QString &html, qsizetype &pos)
{
    static const QHash<QString, char16_t> named = {
        {u"amp"_s, u'&'},      {u"lt"_s, u'<'},       {u"gt"_s, u'>'},       {u"quot"_s, u'"'},     {u"apos"_s, u'\''},    {u"nbsp"_s, u' '},
        {u"auml"_s, u'ä'},     {u"ouml"_s, u'ö'},     {u"uuml"_s, u'ü'},     {u"Auml"_s, u'Ä'},     {u"Ouml"_s, u'Ö'},     {u"Uuml"_s, u'Ü'},
        {u"szlig"_s, u'ß'},    {u"eacute"_s, u'é'},   {u"egrave"_s, u'è'},   {u"ecirc"_s, u'ê'},    {u"euml"_s, u'ë'},     {u"aacute"_s, u'á'},
        {u"agrave"_s, u'à'},   {u"acirc"_s, u'â'},    {u"aring"_s, u'å'},    {u"atilde"_s, u'ã'},   {u"aelig"_s, u'æ'},    {u"ccedil"_s, u'ç'},
        {u"iacute"_s, u'í'},   {u"igrave"_s, u'ì'},   {u"icirc"_s, u'î'},    {u"iuml"_s, u'ï'},     {u"ntilde"_s, u'ñ'},   {u"oacute"_s, u'ó'},
        {u"ograve"_s, u'ò'},   {u"ocirc"_s, u'ô'},    {u"otilde"_s, u'õ'},   {u"oslash"_s, u'ø'},   {u"uacute"_s, u'ú'},   {u"ugrave"_s, u'ù'},
        {u"ucirc"_s, u'û'},    {u"yacute"_s, u'ý'},   {u"Eacute"_s, u'É'},   {u"Egrave"_s, u'È'},   {u"Aacute"_s, u'Á'},   {u"Agrave"_s, u'À'},
        {u"Aring"_s, u'Å'},    {u"AElig"_s, u'Æ'},    {u"Ccedil"_s, u'Ç'},   {u"Ntilde"_s, u'Ñ'},   {u"Oslash"_s, u'Ø'},   {u"ndash"_s, u'–'},
        {u"mdash"_s, u'—'},    {u"hellip"_s, u'…'},   {u"lsquo"_s, u'‘'},    {u"rsquo"_s, u'’'},    {u"sbquo"_s, u'‚'},    {u"ldquo"_s, u'“'},
        {u"rdquo"_s, u'”'},    {u"bdquo"_s, u'„'},    {u"laquo"_s, u'«'},    {u"raquo"_s, u'»'},    {u"lsaquo"_s, u'‹'},   {u"rsaquo"_s, u'›'},
        {u"euro"_s, u'€'},     {u"pound"_s, u'£'},    {u"yen"_s, u'¥'},      {u"cent"_s, u'¢'},     {u"copy"_s, u'©'},     {u"reg"_s, u'®'},
        {u"trade"_s, u'™'},    {u"sect"_s, u'§'},     {u"para"_s, u'¶'},     {u"deg"_s, u'°'},      {u"plusmn"_s, u'±'},   {u"times"_s, u'×'},
        {u"divide"_s, u'÷'},   {u"micro"_s, u'µ'},    {u"middot"_s, u'·'},   {u"bull"_s, u'•'},     {u"dagger"_s, u'†'},   {u"frac12"_s, u'½'},
        {u"frac14"_s, u'¼'},   {u"frac34"_s, u'¾'},   {u"sup2"_s, u'²'},     {u"sup3"_s, u'³'},     {u"iexcl"_s, u'¡'},    {u"iquest"_s, u'¿'},
        {u"larr"_s, u'←'},     {u"rarr"_s, u'→'},     {u"uarr"_s, u'↑'},     {u"darr"_s, u'↓'},     {u"ensp"_s, u' '},     {u"emsp"_s, u' '},
        {u"thinsp"_s, u' '},   {u"shy"_s, u'­'}, {u"zwnj"_s, u'‌'}, {u"zwj"_s, u'‍'},
    };

    // The longest names are short; anything else is an ampersand that was not escaped.
    const qsizetype end = html.indexOf(u';', pos + 1);
    if (end < 0 || end - pos > 12) {
        ++pos;
        return u"&"_s;
    }
    const QStringView body = QStringView(html).mid(pos + 1, end - pos - 1);
    if (body.startsWith(u'#')) {
        bool ok = false;
        const bool hex = body.size() > 1 && (body[1] == u'x' || body[1] == u'X');
        const char32_t code = hex ? body.mid(2).toUInt(&ok, 16) : body.mid(1).toUInt(&ok, 10);
        if (ok && code > 0 && code <= 0x10ffff && !(code >= 0xd800 && code <= 0xdfff)) {
            pos = end + 1;
            return QString::fromUcs4(&code, 1);
        }
    } else if (const auto it = named.constFind(body.toString()); it != named.constEnd()) {
        pos = end + 1;
        return QString(QChar(*it));
    }
    ++pos;
    return u"&"_s;
}
}

QString htmlToPlainText(const QString &html)
{
    // One pass over the text and nothing that grows with nesting depth. Files that merely look
    // like HTML, such as megabytes of JSON with markup inside, must not stall the indexer, which
    // is what building a real document tree of them does.
    static const QSet<QString> blockTags = {
        u"p"_s,       u"div"_s,     u"br"_s,      u"li"_s,     u"ul"_s,     u"ol"_s,      u"tr"_s,      u"table"_s,      u"h1"_s,
        u"h2"_s,      u"h3"_s,      u"h4"_s,      u"h5"_s,     u"h6"_s,     u"pre"_s,     u"hr"_s,      u"blockquote"_s, u"section"_s,
        u"article"_s, u"header"_s,  u"footer"_s,  u"title"_s,  u"dt"_s,     u"dd"_s,      u"dl"_s,      u"address"_s,    u"figure"_s,
        u"figcaption"_s, u"nav"_s,  u"aside"_s,   u"main"_s,   u"form"_s,   u"option"_s,  u"body"_s,    u"head"_s,       u"html"_s,
    };
    static const QSet<QString> cellTags = {u"td"_s, u"th"_s};
    static const QSet<QString> skippedElements = {u"script"_s, u"style"_s, u"template"_s, u"svg"_s};

    QString out;
    out.reserve(html.size() / 2);
    const qsizetype n = html.size();
    bool pendingSpace = false;
    int preDepth = 0;
    // Elements and comments that are never closed further down, so that cut-off markup neither
    // hides everything behind it nor has the rest of the text searched once per opening tag.
    QSet<QString> neverClosed;
    bool commentsEnd = true;

    auto lineBreak = [&out, &pendingSpace] {
        pendingSpace = false;
        if (!out.isEmpty() && !out.endsWith(u'\n')) {
            out += u'\n';
        }
    };
    auto append = [&out, &pendingSpace](QStringView text) {
        if (pendingSpace && !out.isEmpty() && !out.endsWith(u'\n')) {
            out += u' ';
        }
        pendingSpace = false;
        out += text;
    };

    qsizetype i = 0;
    qsizetype sinceCheck = 0;
    while (i < n) {
        if (++sinceCheck > 65536) {
            sinceCheck = 0;
            if (Task::aborted()) {
                break;
            }
        }
        const QChar c = html[i];
        if (c == u'<') {
            if (QStringView(html).mid(i, 4) == "<!--"_L1) {
                const qsizetype end = commentsEnd ? html.indexOf("-->"_L1, i + 4) : -1;
                commentsEnd = end >= 0;
                i = end < 0 ? i + 4 : end + 3;
                continue;
            }
            const bool closing = i + 1 < n && html[i + 1] == u'/';
            qsizetype nameStart = i + (closing ? 2 : 1);
            qsizetype nameEnd = nameStart;
            while (nameEnd < n && nameEnd - nameStart < 16 && (html[nameEnd].isLetterOrNumber())) {
                ++nameEnd;
            }
            const bool isTag = nameEnd > nameStart || (i + 1 < n && (html[i + 1] == u'!' || html[i + 1] == u'?'));
            if (!isTag) {
                // A lone "<", as in "a < b".
                append(QStringView(html).mid(i, 1));
                ++i;
                continue;
            }
            const QString name = html.mid(nameStart, nameEnd - nameStart).toLower();
            // Tags do not span much; without a limit one stray "<" would swallow the rest.
            qsizetype tagEnd = html.indexOf(u'>', nameEnd);
            if (tagEnd < 0 || tagEnd - i > 8192) {
                append(QStringView(html).mid(i, 1));
                ++i;
                continue;
            }
            i = tagEnd + 1;

            if (!closing && skippedElements.contains(name) && !neverClosed.contains(name)) {
                const QString closeTag = "</"_L1 + name;
                const qsizetype close = html.indexOf(closeTag, i, Qt::CaseInsensitive);
                const qsizetype closeEnd = close < 0 ? -1 : html.indexOf(u'>', close);
                if (closeEnd < 0) {
                    neverClosed.insert(name); // only the tag itself is dropped
                } else {
                    i = closeEnd + 1;
                }
                continue;
            }
            if (name == "pre"_L1) {
                preDepth = qMax(0, preDepth + (closing ? -1 : 1));
            }
            if (blockTags.contains(name)) {
                lineBreak();
            } else if (cellTags.contains(name)) {
                pendingSpace = true;
            }
        } else if (c == u'&') {
            append(decodeEntity(html, i));
        } else if (c.isSpace()) {
            if (preDepth > 0) {
                pendingSpace = false;
                out += c;
            } else {
                pendingSpace = true;
            }
            ++i;
        } else {
            // Copy the whole run of ordinary characters at once.
            qsizetype end = i + 1;
            while (end < n && html[end] != u'<' && html[end] != u'&' && !html[end].isSpace()) {
                ++end;
            }
            append(QStringView(html).mid(i, end - i));
            sinceCheck += end - i;
            i = end;
        }
    }
    return out.trimmed();
}

Email readEmail(const QByteArray &raw)
{
    const auto message = parseMessage(raw);
    const KMime::Message &constMessage = *message;
    Email email;
    if (const auto *subject = constMessage.subject()) {
        email.subject = subject->asUnicodeString();
    }
    if (const auto *from = constMessage.from()) {
        email.from = from->asUnicodeString();
    }
    if (const auto *to = constMessage.to()) {
        email.to = to->asUnicodeString();
    }
    if (const auto *cc = constMessage.cc()) {
        email.cc = cc->asUnicodeString();
    }
    if (const auto *bcc = constMessage.bcc()) {
        email.bcc = bcc->asUnicodeString();
    }
    if (const auto *date = constMessage.date()) {
        email.date = date->dateTime();
    }
    collectParts(message.get(), &email, nullptr);
    if (email.textBody.isEmpty() && !email.htmlBody.isEmpty()) {
        email.textBody = htmlToPlainText(email.htmlBody);
    }
    email.valid = !email.subject.isEmpty() || !email.from.isEmpty() || !email.textBody.isEmpty() || !email.attachments.isEmpty();
    return email;
}

QList<Email> readEmails(const QString &path, int maxMessages)
{
    QList<Email> result;
    const QByteArray data = readFile(path);
    if (data.isEmpty()) {
        return result;
    }
    const QList<QByteArray> rawMessages = splitMessages(data, maxMessages);
    for (const QByteArray &raw : rawMessages) {
        const Email email = readEmail(raw);
        if (email.valid) {
            result.append(email);
        }
    }
    return result;
}

QByteArray readEmailAttachment(const QString &path, int messageIndex, int attachmentIndex)
{
    const QByteArray data = readFile(path);
    const QList<QByteArray> rawMessages = splitMessages(data, messageIndex + 1);
    if (messageIndex < 0 || messageIndex >= rawMessages.size()) {
        return {};
    }
    const auto message = parseMessage(rawMessages.at(messageIndex));
    QList<KMime::Content *> attachments;
    collectParts(message.get(), nullptr, &attachments);
    if (attachmentIndex < 0 || attachmentIndex >= attachments.size()) {
        return {};
    }
    return attachments.at(attachmentIndex)->decodedBody();
}

}
