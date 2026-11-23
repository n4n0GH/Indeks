// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

namespace Indeks
{

/**
 * A website as a document: a small file that holds nothing but the address of a page.
 * Indeks writes such files in the form other programs know as internet shortcuts (.url),
 * and reads those and the ones macOS writes (.webloc).
 */
namespace WebLink
{
/**
 * The address a text is, if it is one and nothing else: one word that begins with http://
 * or https:// and names a host. Invalid otherwise.
 */
QUrl addressOfText(const QString &text);

/** The address the link file @p path holds. Invalid if it holds none that is on the web. */
QUrl read(const QString &path);

/** What a link file for @p address holds. */
QByteArray fileContent(const QUrl &address);

/** A name for the document of @p address, made of its host and path. */
QString nameFor(const QUrl &address);
}

}
