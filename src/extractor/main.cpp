// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

// indeks-extractor: reads files for Indeks' index.
//
// Indeks starts a few of these and sends them the files to read (see ExtractorPool). Being a
// process of its own is the point: the format libraries run here, and if one of them crashes
// or hangs on a file, this process is lost and Indeks is not. It never opens the index and
// writes nothing but renditions of Office documents into the cache.

#include "extractorpool.h"
#include "fingerprint.h"
#include "indexing.h"
#include "paths.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QMimeDatabase>
#include <QTextStream>
#include <QThread>

#include <cerrno>
#include <csignal>
#include <cstdlib>

#include <sys/prctl.h>
#include <unistd.h>

using namespace Indeks;
using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    // Not to outlive Indeks, whatever it is busy with when Indeks goes.
    prctl(PR_SET_PDEATHSIG, SIGKILL);

    // Results go out on what was standard output. Libraries that print to standard output
    // would garble them, so from here on that leads to standard error.
    const int replyDescriptor = dup(STDOUT_FILENO);
    dup2(STDERR_FILENO, STDOUT_FILENO);

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(u"indeks-extractor"_s);
    QCommandLineParser parser;
    parser.addOption({u"database-dir"_s, u"Folder of the database Indeks has open."_s, u"dir"_s});
    parser.addOption({u"slot"_s, u"Number of this helper among its siblings."_s, u"n"_s, u"0"_s});
    parser.addOption({u"look"_s, u"Print what the files given look like (their fingerprints) and exit; for trying things out."_s});
    parser.process(app);
    if (parser.isSet(u"look"_s)) {
        // One line per file: shape, ink and the bits, as Indeks would note them.
        QTextStream out(stderr);
        static const QMimeDatabase mimeDatabase;
        const QStringList files = parser.positionalArguments();
        for (const QString &file : files) {
            const Kind kind = kindForFile(file, mimeDatabase.mimeTypeForFile(file).name());
            const Fingerprint print = Fingerprint::ofFile(file, kind);
            out << int(print.shape) << ' ' << print.ink;
            for (const quint32 word : print.bits) {
                out << ' ' << word;
            }
            out << '\t' << file << '\n';
        }
        return 0;
    }
    // The cache, where renditions of Office documents go, belongs to the database.
    Paths::setDatabaseDir(parser.value(u"database-dir"_s));
    const QString slot = parser.value(u"slot"_s);

    // For the tests: file names to crash or hang on, to see that Indeks copes.
    const QString crashOn = qEnvironmentVariable("INDEKS_EXTRACTOR_TEST_CRASH");
    const QString hangOn = qEnvironmentVariable("INDEKS_EXTRACTOR_TEST_HANG");

    QFile replies;
    if (replyDescriptor < 0 || !replies.open(replyDescriptor, QIODevice::WriteOnly | QIODevice::Unbuffered)) {
        return 1;
    }

    QByteArray buffer;
    for (;;) {
        QByteArray payload;
        while (!ExtractorProtocol::takeFrame(buffer, payload)) {
            // Plain read(): it comes back with what there is. QFile would wait until it has
            // all it was asked for, which a request never fills.
            char chunk[64 * 1024];
            const ssize_t got = ::read(STDIN_FILENO, chunk, sizeof(chunk));
            if (got < 0 && errno == EINTR) {
                continue;
            }
            if (got <= 0) {
                return 0; // Indeks closed the line: done
            }
            buffer.append(chunk, got);
        }
        IndexOptions options;
        QList<ExtractorPool::Request> tasks;
        if (!ExtractorProtocol::readBatch(payload, options, tasks)) {
            return 2;
        }
        // Every helper has LibreOffice use a profile of its own, so that several can convert
        // at the same time.
        options.officeProfile = "index-"_L1 + slot;

        for (const ExtractorPool::Request &task : std::as_const(tasks)) {
            replies.write(ExtractorProtocol::startedMessage(task.id));
            if (!crashOn.isEmpty() && task.reading.name.contains(crashOn)) {
                std::abort();
            }
            if (!hangOn.isEmpty() && task.reading.name.contains(hangOn)) {
                QThread::sleep(3600);
            }
            replies.write(ExtractorProtocol::resultMessage(task.id, Indexing::readFile(task.reading, options)));
        }
        replies.write(ExtractorProtocol::batchDoneMessage());
    }
}
