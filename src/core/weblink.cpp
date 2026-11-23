// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "weblink.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringDecoder>

using namespace Qt::StringLiterals;

namespace Indeks::WebLink
{

namespace
{
// A link file is a few lines; whatever is longer is something else.
constexpr qint64 MaxLinkFileBytes = 64 * 1024;

/** Only pages on the web count: what a browser is the right program for. */
QUrl onTheWeb(const QUrl &url)
{
    const bool web = url.isValid() && (url.scheme() == "http"_L1 || url.scheme() == "https"_L1) && !url.host().isEmpty();
    return web ? url : QUrl();
}
}

QUrl addressOfText(const QString &text)
{
    const QString word = text.trimmed();
    static const QRegularExpression space(u"\\s"_s);
    if (word.isEmpty() || word.size() > 2000 || word.contains(space)) {
        return {};
    }
    if (!word.startsWith("http://"_L1, Qt::CaseInsensitive) && !word.startsWith("https://"_L1, Qt::CaseInsensitive)) {
        return {};
    }
    return onTheWeb(QUrl(word, QUrl::StrictMode));
}

QUrl read(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray data = file.read(MaxLinkFileBytes);
    // Windows writes some of these as UTF-16, with a mark at the start that says so.
    QStringDecoder decoder(QStringConverter::encodingForData(data).value_or(QStringConverter::Utf8));
    const QString text = decoder.decode(data);

    QString address;
    if (path.endsWith(".webloc"_L1, Qt::CaseInsensitive)) {
        static const QRegularExpression entry(u"<key>\\s*URL\\s*</key>\\s*<string>([^<]*)</string>"_s);
        address = entry.match(text).captured(1);
        address.replace("&amp;"_L1, "&"_L1);
    } else {
        static const QRegularExpression entry(u"^\\s*URL\\s*=(.*)$"_s, QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
        address = entry.match(text).captured(1);
    }
    return onTheWeb(QUrl(address.trimmed()));
}

QByteArray fileContent(const QUrl &address)
{
    // The form of an internet shortcut, lines ended the way Windows ends them.
    return "[InternetShortcut]\r\nURL=" + address.toEncoded() + "\r\n";
}

QString nameFor(const QUrl &address)
{
    QString host = address.host();
    if (host.startsWith("www."_L1)) {
        host.remove(0, 4);
    }
    QString name = host + u' ' + address.path(QUrl::FullyDecoded);
    // Not every place a database can be in takes every name.
    static const QRegularExpression unwelcome(u"[\\\\/:*?\"<>|\\x00-\\x1f]"_s);
    name.replace(unwelcome, u" "_s);
    name = name.simplified();
    const int longest = 60;
    if (name.size() > longest) {
        const qsizetype space = name.lastIndexOf(u' ', longest);
        name.truncate(space > longest / 2 ? space : longest);
    }
    return name.trimmed();
}

}
