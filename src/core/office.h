// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

/**
 * Office documents (Word, Excel, PowerPoint, OpenDocument, RTF) as they look: Indeks has
 * LibreOffice make a PDF of them and shows that. The PDF is a rendition kept in the cache; the
 * document itself is only read.
 *
 * LibreOffice runs without a window, with a settings profile of its own (so that it neither
 * disturbs nor is handed over to a LibreOffice the user has open) and, where bubblewrap is
 * installed, without network access: nothing in a document gets to load anything from outside.
 */
namespace Indeks::Office
{

/** Whether @p mimeType is a document LibreOffice can make a rendition of. */
bool isOfficeDocument(const QString &mimeType);

/** Whether LibreOffice is installed. */
bool converterAvailable();

/** Where the rendition of content with the given hash is kept. Identical files share it. */
QString renditionPath(const QString &contentHash);

/**
 * Returns the PDF rendition of the file at @p path, making it first if there is none yet.
 * Blocks while LibreOffice runs, so call it from a worker thread; it gives up when the
 * calling thread's task is aborted (see Task) or @p timeoutMs have passed.
 *
 * @p contentHash is the hash of the file (hashFile()). @p profile names the LibreOffice profile
 * to use; callers that may run at the same time must use different ones.
 * Returns an empty string on failure and says why in @p error.
 */
QString renderToPdf(const QString &path, const QString &contentHash, const QString &profile, int timeoutMs, QString *error = nullptr);

/** Removes renditions that have not been looked at for @p maxAgeDays. */
void pruneRenditions(int maxAgeDays);

}
