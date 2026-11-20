#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <utility>

// One group of the EXIF panel: a title ("Camera", "Exposure"…) and
// label / value rows, all ready to show.
struct ExifSection
{
    QString title;
    QList<std::pair<QString, QString>> rows;
};

// The EXIF block of a JPEG, PNG, WebP or TIFF file, starting with "Exif\0\0"
// followed by the TIFF-structured data. Empty if the file has none.
QByteArray readExifBlock(const QString &path);

// The file's EXIF details that mean something to a person (camera, lens,
// exposure, date, place), worded and formatted for the user's language.
// Groups without any data are left out; empty if the file has no EXIF.
QList<ExifSection> readExifDetails(const QString &path);
