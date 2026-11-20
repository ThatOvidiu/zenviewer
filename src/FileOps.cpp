#include "FileOps.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

namespace {
// Raw formats of the camera makers, lower case
const QSet<QString> &rawExtensions()
{
    static const QSet<QString> extensions = {
        "3fr", "fff",               // Hasselblad
        "arw", "srf", "sr2",        // Sony
        "cr2", "cr3", "crw",        // Canon
        "dcr", "kdc",               // Kodak
        "dng",                      // Adobe, Leica, Pentax, Ricoh, phones
        "erf",                      // Epson
        "gpr",                      // GoPro
        "iiq",                      // Phase One
        "mef",                      // Mamiya
        "mos",                      // Leaf
        "mrw",                      // Minolta
        "nef", "nrw",               // Nikon
        "orf", "ori",               // Olympus, OM System
        "pef", "ptx",               // Pentax
        "raf",                      // Fujifilm
        "raw", "rw2", "rwl",        // Panasonic, Leica
        "srw",                      // Samsung
        "x3f",                      // Sigma
    };
    return extensions;
}

#ifdef Q_OS_WIN
constexpr Qt::CaseSensitivity NameCase = Qt::CaseInsensitive;
#else
constexpr Qt::CaseSensitivity NameCase = Qt::CaseSensitive;
#endif
}

QStringList rawCompanions(const QString &path)
{
    const QFileInfo info(path);
    const QString base = info.completeBaseName();
    QStringList raws;
    // Listed rather than guessed, so the names come out as they are on disk (DSC_1.NEF, not .nef)
    const QFileInfoList entries = info.dir().entryInfoList(QDir::Files | QDir::Hidden, QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.completeBaseName().compare(base, NameCase) == 0
            && entry.fileName().compare(info.fileName(), NameCase) != 0
            && rawExtensions().contains(entry.suffix().toLower()))
            raws << entry.absoluteFilePath();
    }
    return raws;
}

QStringList existingTargets(const QStringList &files, const QString &folder)
{
    QStringList existing;
    const QDir dir(folder);
    for (const QString &file : files) {
        const QString target = dir.filePath(QFileInfo(file).fileName());
        if (QFileInfo::exists(target))
            existing << target;
    }
    return existing;
}

QString transferFiles(const QStringList &files, const QString &folder, bool move)
{
    const QDir dir(folder);
    for (const QString &file : files) {
        const QString name = QFileInfo(file).fileName();
        const QString target = dir.filePath(name);
        if (QFileInfo(file).canonicalFilePath() == QFileInfo(target).canonicalFilePath())
            continue; // already there
        // Replacing was agreed before; copy and rename don't overwrite on their own
        if (QFileInfo::exists(target) && !QFile::remove(target))
            return name;
        // rename() falls back to copying and removing when the folder is on another drive
        if (!(move ? QFile::rename(file, target) : QFile::copy(file, target)))
            return name;
    }
    return {};
}
