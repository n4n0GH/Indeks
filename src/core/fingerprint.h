// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "types.h"

#include <QString>

#include <array>

class QImage;

namespace Indeks
{

/**
 * What a document looks like, in so few numbers that hundreds of thousands of documents can
 * be compared with one in a moment: for finding the documents that look like a given one,
 * whatever they are called and whatever is written in them.
 *
 * Two things are told apart, because "looks like" means two things:
 *
 * - **A page**, light with something on it: a letter, an invoice, a form, a scan of one. What
 *   makes two pages alike is their head: the same letterhead, the same logo at the same
 *   place. Below it, every invoice of a sender has other lines. So the fingerprint of a page
 *   is of its upper quarter: a grid of 32 by 8 fields, and for each whether it is darker
 *   than the fields are on average.
 * - **A picture**, anything else, and also a page with next to nothing in its upper quarter:
 *   compared as a whole. A grid of 16 by 16 fields, and for each whether it is lighter than
 *   the field to its left, which stays as it is when a picture is made lighter, smaller or
 *   is saved again.
 *
 * Both come to 256 bits. Two fingerprints of the same shape are the nearer the fewer bits
 * they differ in; a page and a picture are not compared.
 *
 * Made from the first page of a PDF and from a picture file; no other kind of document has
 * one. Kept in the table `fingerprints` (see Store), where SearchQuery compares them
 * (`similar:`), in SQL: SQLite cannot count bits, so the counting is spelt out
 * (sqlDistance()).
 */
struct Fingerprint {
    enum Shape {
        None = 0, ///< nothing to go by: not looked at, not a kind that has one, or not readable
        Page = 1,
        Picture = 2,
    };
    static constexpr int Words = 8;

    Shape shape = None;
    /** How many of the bits are set. For a page: how much there is in its head. */
    int ink = 0;
    std::array<quint32, Words> bits{};

    bool isValid() const
    {
        return shape != None;
    }
    bool operator==(const Fingerprint &other) const = default;

    /** Whether documents of this kind have a fingerprint at all. */
    static bool applies(Kind kind)
    {
        return kind == Kind::Pdf || kind == Kind::Image;
    }
    static Fingerprint ofImage(const QImage &image);
    /** Of a file: the first page of a PDF, a picture. Reads the file; safe in any thread and process. */
    static Fingerprint ofFile(const QString &path, Kind kind);

    /** In how many bits two fingerprints differ; -1 where they are not of one shape. */
    static int distance(const Fingerprint &a, const Fingerprint &b);
    /**
     * How far apart two fingerprints of this shape may be and still count as alike: a
     * number of bits for each shape, fewer where the two have little set between them.
     */
    static int allowance(Shape shape, int inkA, int inkB, bool roughly);
    static bool similar(const Fingerprint &a, const Fingerprint &b, bool roughly);

    /**
     * The same question in SQL, of a row `f` of `fingerprints` and the row `r` of the
     * document that is asked about: a condition that holds where they are alike.
     */
    static QString sqlSimilar(bool roughly);
};

}
