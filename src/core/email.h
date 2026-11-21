// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

namespace Indeks
{

struct EmailAttachment {
    QString name;
    QString mimeType;
    qint64 size = 0;
};

struct Email {
    bool valid = false;
    QString subject;
    QString from;
    QString to;
    QString cc;
    QString bcc; ///< only messages that were kept by their sender have it
    QDateTime date;
    QString textBody; ///< plain text; derived from the HTML part if the message has no text part
    QString htmlBody; ///< empty if the message has no HTML part
    QList<EmailAttachment> attachments;
};

/**
 * Reads the messages of an exported e-mail file. An .eml file holds one message,
 * an mbox file any number.
 */
QList<Email> readEmails(const QString &path, int maxMessages = 1000);

/** Reads one message from the bytes it is made of. */
Email readEmail(const QByteArray &raw);

/** Decoded content of attachment @p attachmentIndex of message @p messageIndex. */
QByteArray readEmailAttachment(const QString &path, int messageIndex, int attachmentIndex);

/** Plain text rendition of an HTML fragment. */
QString htmlToPlainText(const QString &html);

}
