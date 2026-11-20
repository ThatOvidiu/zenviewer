#include "ExifInfo.h"

#include <TinyEXIF.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QLocale>
#include <QtEndian>

#include <cmath>
#include <cstring>
#include <string>

namespace {
constexpr qint64 MaxBlockSize = 16 * 1024 * 1024; // more than any real EXIF block
constexpr qint64 TiffReadSize = 4 * 1024 * 1024;  // a TIFF's tags sit near its start
const QByteArray ExifHeader("Exif\0\0", 6);

QString tr(const char *text)
{
    return QCoreApplication::translate("ExifInfo", text);
}

QByteArray withHeader(const QByteArray &tiff)
{
    return tiff.startsWith(ExifHeader) ? tiff : ExifHeader + tiff;
}

// APP1 segments before the image data; the EXIF one starts "Exif\0\0"
QByteArray jpegBlock(QFile &file)
{
    file.seek(2);
    for (int segments = 0; segments < 1000; ++segments) {
        char byte = 0;
        if (!file.getChar(&byte) || quint8(byte) != 0xFF)
            return {};
        while (quint8(byte) == 0xFF) // fill bytes
            if (!file.getChar(&byte))
                return {};
        const quint8 marker = quint8(byte);
        if (marker == 0xD9 || marker == 0xDA) // end of image, start of scan
            return {};
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) // no length
            continue;
        char length[2];
        if (file.read(length, 2) != 2)
            return {};
        const int size = qFromBigEndian<quint16>(length) - 2;
        if (size < 0)
            return {};
        if (marker == 0xE1 && size >= 6) {
            const QByteArray data = file.read(size);
            if (data.startsWith(ExifHeader))
                return data;
        } else if (!file.seek(file.pos() + size)) {
            return {};
        }
    }
    return {};
}

// The eXIf chunk holds the TIFF data without the "Exif" header
QByteArray pngBlock(QFile &file)
{
    file.seek(8);
    while (true) {
        char chunk[8]; // big-endian length + type
        if (file.read(chunk, 8) != 8)
            return {};
        const quint32 size = qFromBigEndian<quint32>(chunk);
        if (memcmp(chunk + 4, "eXIf", 4) == 0)
            return size <= MaxBlockSize ? withHeader(file.read(size)) : QByteArray();
        if (memcmp(chunk + 4, "IEND", 4) == 0)
            return {};
        if (!file.seek(file.pos() + qint64(size) + 4)) // data + CRC
            return {};
    }
}

// The EXIF chunk, with or without the header depending on the writer
QByteArray webpBlock(QFile &file)
{
    file.seek(12);
    while (true) {
        char chunk[8]; // fourcc + little-endian size
        if (file.read(chunk, 8) != 8)
            return {};
        const quint32 size = qFromLittleEndian<quint32>(chunk + 4);
        if (memcmp(chunk, "EXIF", 4) == 0)
            return size <= MaxBlockSize ? withHeader(file.read(size)) : QByteArray();
        // Chunks are padded to an even size
        if (!file.seek(file.pos() + qint64(size) + (size & 1)))
            return {};
    }
}

// Text from the file, which may hold anything: no control characters, one line
QString cleanText(const std::string &text)
{
    QString clean;
    for (const QChar c : QString::fromUtf8(text.data(), qsizetype(text.size())))
        clean += c.isPrint() ? c : QChar(u' ');
    clean = clean.simplified();
    if (clean.size() > 300)
        clean = clean.left(300) + QChar(0x2026);
    return clean;
}

// 2.8, 4, 1.4: as few decimals as the value needs, at most `decimals`
QString number(double value, int decimals = 1)
{
    QString text = QLocale().toString(value, 'f', decimals);
    const QChar point = QLocale().decimalPoint().front();
    if (text.contains(point)) {
        while (text.endsWith(u'0'))
            text.chop(1);
        if (text.endsWith(point))
            text.chop(1);
    }
    return text;
}

QString exposureText(double seconds)
{
    if (seconds <= 0 || !std::isfinite(seconds))
        return {};
    if (seconds < 0.5)
        return tr("1/%1 s").arg(qRound(1 / seconds));
    return tr("%1 s").arg(number(seconds));
}

// "2024:07:14 18:03:59" in the user's language; as written if it isn't a date
QString dateText(const std::string &exif)
{
    const QString text = cleanText(exif);
    const QDateTime date = QDateTime::fromString(text.left(19), QStringLiteral("yyyy:MM:dd HH:mm:ss"));
    if (!date.isValid())
        return text;
    const QLocale locale;
    return locale.toString(date.date(), QLocale::LongFormat) + QStringLiteral("   ") +
           locale.toString(date.time(), QLocale::ShortFormat);
}

// 48° 51′ 29.6″ N
QString coordinateText(double value, bool latitude)
{
    const QString hemisphere = latitude ? (value < 0 ? tr("S") : tr("N")) : (value < 0 ? tr("W") : tr("E"));
    double rest = std::abs(value);
    const int degrees = int(rest);
    rest = (rest - degrees) * 60;
    int minutes = int(rest);
    double seconds = (rest - minutes) * 60;
    if (seconds >= 59.95) { // would print as 60.0
        seconds = 0;
        ++minutes;
    }
    return QStringLiteral("%1° %2′ %3″ %4").arg(degrees).arg(minutes).arg(QLocale().toString(seconds, 'f', 1), hemisphere);
}

QString programText(int program)
{
    switch (program) {
    case 1: return tr("Manual");
    case 2: return tr("Program");
    case 3: return tr("Aperture priority");
    case 4: return tr("Shutter priority");
    case 5: return tr("Creative");
    case 6: return tr("Action");
    case 7: return tr("Portrait");
    case 8: return tr("Landscape");
    default: return {};
    }
}

QString meteringText(int mode)
{
    switch (mode) {
    case 1: return tr("Average");
    case 2: return tr("Centre-weighted");
    case 3: return tr("Spot");
    case 4: return tr("Multi-spot");
    case 5: return tr("Pattern");
    case 6: return tr("Partial");
    default: return {};
    }
}

QString lightText(int source)
{
    switch (source) {
    case 1: return tr("Daylight");
    case 2: return tr("Fluorescent");
    case 3: return tr("Tungsten");
    case 4: return tr("Flash");
    case 9: return tr("Fine weather");
    case 10: return tr("Cloudy");
    case 11: return tr("Shade");
    case 12: case 13: case 14: case 15: return tr("Fluorescent");
    case 17: case 18: case 19: return tr("Standard light");
    case 20: case 21: case 22: case 23: return tr("Daylight (D%1)").arg(source == 20 ? 55 : source == 21 ? 65 : source == 22 ? 75 : 50);
    case 24: return tr("Studio tungsten");
    default: return {};
    }
}

QString orientationText(int orientation)
{
    switch (orientation) {
    case 2: return tr("Mirrored");
    case 3: return tr("Rotated 180°");
    case 4: return tr("Flipped");
    case 5: return tr("Mirrored, rotated 90° left");
    case 6: return tr("Rotated 90° right");
    case 7: return tr("Mirrored, rotated 90° right");
    case 8: return tr("Rotated 90° left");
    default: return {};
    }
}
}

QByteArray readExifBlock(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray magic = file.peek(12);
    if (magic.startsWith("\xFF\xD8"))
        return jpegBlock(file);
    if (magic.startsWith("\x89PNG\r\n\x1A\n"))
        return pngBlock(file);
    if (magic.startsWith("RIFF") && magic.mid(8, 4) == "WEBP")
        return webpBlock(file);
    // A TIFF file is itself the structure an EXIF block holds; its offsets count from the
    // start of the file, which is where they count from after the header too
    if (magic.startsWith(QByteArray("II*\0", 4)) || magic.startsWith(QByteArray("MM\0*", 4)))
        return withHeader(file.read(TiffReadSize));
    return {};
}

QList<ExifSection> readExifDetails(const QString &path)
{
    const QByteArray block = readExifBlock(path);
    if (block.isEmpty())
        return {};
    TinyEXIF::EXIFInfo exif;
    if (exif.parseFromEXIFSegment(reinterpret_cast<const uint8_t *>(block.constData()), unsigned(block.size()))
        != TinyEXIF::PARSE_SUCCESS)
        return {};
    const auto has = [&exif](TinyEXIF::FieldID id) { return exif.HasField(id); };
    using namespace TinyEXIF;

    ExifSection camera{tr("Camera"), {}};
    const QString make = cleanText(exif.Make);
    const QString model = cleanText(exif.Model);
    // Most cameras repeat the make in the model: "Canon" + "Canon EOS R6"
    const QString body = model.startsWith(make.section(u' ', 0, 0), Qt::CaseInsensitive) ? model
                                                                                         : (make + u' ' + model).trimmed();
    if (!body.isEmpty())
        camera.rows.append({tr("Camera"), body});
    QString lens = cleanText(exif.LensInfo.Model);
    const QString lensMake = cleanText(exif.LensInfo.Make);
    if (!lens.isEmpty() && !lensMake.isEmpty() && !lens.contains(lensMake, Qt::CaseInsensitive))
        lens = lensMake + u' ' + lens;
    if (lens.isEmpty() && has(FIELD_ID_LensInfo_FocalLengthMin) && exif.LensInfo.FocalLengthMin > 0) {
        const double wide = exif.LensInfo.FocalLengthMin, tele = exif.LensInfo.FocalLengthMax;
        lens = tele > wide ? tr("%1–%2 mm").arg(number(wide), number(tele)) : tr("%1 mm").arg(number(wide));
        if (has(FIELD_ID_LensInfo_FStopMin) && exif.LensInfo.FStopMin > 0)
            lens += QStringLiteral(" f/") + number(exif.LensInfo.FStopMin);
    }
    if (!lens.isEmpty())
        camera.rows.append({tr("Lens"), lens});
    if (const QString software = cleanText(exif.Software); !software.isEmpty())
        camera.rows.append({tr("Software"), software});

    ExifSection exposure{tr("Exposure"), {}};
    QString shutter;
    if (has(FIELD_ID_ExposureTime))
        shutter = exposureText(exif.ExposureTime);
    else if (has(FIELD_ID_ShutterSpeedValue))
        shutter = exposureText(exif.ShutterSpeedValue);
    if (!shutter.isEmpty())
        exposure.rows.append({tr("Shutter"), shutter});
    const double aperture = has(FIELD_ID_FNumber) ? exif.FNumber : has(FIELD_ID_ApertureValue) ? exif.ApertureValue : 0;
    if (aperture > 0 && std::isfinite(aperture))
        exposure.rows.append({tr("Aperture"), QStringLiteral("f/") + number(aperture)});
    if (has(FIELD_ID_ISOSpeedRatings) && exif.ISOSpeedRatings > 0)
        exposure.rows.append({tr("ISO"), QString::number(exif.ISOSpeedRatings)});
    if (has(FIELD_ID_FocalLength) && exif.FocalLength > 0) {
        QString focal = tr("%1 mm").arg(number(exif.FocalLength));
        const double fullFrame = exif.LensInfo.FocalLengthIn35mm;
        if (has(FIELD_ID_LensInfo_FocalLengthIn35mm) && fullFrame > 0 && qRound(fullFrame) != qRound(exif.FocalLength))
            focal += tr("  (%1 mm full frame)").arg(number(fullFrame, 0));
        exposure.rows.append({tr("Focal length"), focal});
    }
    if (has(FIELD_ID_ExposureBiasValue) && std::isfinite(exif.ExposureBiasValue)) {
        const double bias = exif.ExposureBiasValue;
        const QString sign = bias > 0.005 ? QStringLiteral("+") : bias < -0.005 ? QStringLiteral("−") : QString();
        exposure.rows.append({tr("Compensation"), tr("%1%2 EV").arg(sign, number(std::abs(bias)))});
    }
    if (const QString program = has(FIELD_ID_ExposureProgram) ? programText(exif.ExposureProgram) : QString();
        !program.isEmpty())
        exposure.rows.append({tr("Mode"), program});
    if (const QString metering = has(FIELD_ID_MeteringMode) ? meteringText(exif.MeteringMode) : QString();
        !metering.isEmpty())
        exposure.rows.append({tr("Metering"), metering});
    if (has(FIELD_ID_Flash)) {
        const int flash = exif.Flash;
        exposure.rows.append({tr("Flash"), (flash & 0x20) ? tr("None") : (flash & 1) ? tr("Fired") : tr("Did not fire")});
    }
    if (const QString light = has(FIELD_ID_LightSource) ? lightText(exif.LightSource) : QString(); !light.isEmpty())
        exposure.rows.append({tr("Light"), light});

    ExifSection picture{tr("Picture"), {}};
    const QString taken = has(FIELD_ID_DateTimeOriginal) ? dateText(exif.DateTimeOriginal)
                         : has(FIELD_ID_DateTimeDigitized) ? dateText(exif.DateTimeDigitized) : QString();
    if (!taken.isEmpty())
        picture.rows.append({tr("Taken"), taken});
    if (const QString changed = has(FIELD_ID_DateTime) ? dateText(exif.DateTime) : QString();
        !changed.isEmpty() && changed != taken)
        picture.rows.append({taken.isEmpty() ? tr("Date") : tr("Edited"), changed});
    if (has(FIELD_ID_ImageWidth) && has(FIELD_ID_ImageHeight) && exif.ImageWidth > 0 && exif.ImageHeight > 0)
        picture.rows.append({tr("Size"), QStringLiteral("%1 × %2").arg(exif.ImageWidth).arg(exif.ImageHeight)});
    if (const QString turned = has(FIELD_ID_Orientation) ? orientationText(exif.Orientation) : QString();
        !turned.isEmpty())
        picture.rows.append({tr("Orientation"), turned});
    if (const QString description = cleanText(exif.ImageDescription); !description.isEmpty())
        picture.rows.append({tr("Description"), description});
    if (const QString copyright = cleanText(exif.Copyright); !copyright.isEmpty())
        picture.rows.append({tr("Copyright"), copyright});

    ExifSection place{tr("Location"), {}};
    const auto &geo = exif.GeoLocation;
    // The has…() checks alone pass on TinyEXIF's defaults (0, 0) when there are no GPS tags
    if (has(FIELD_ID_GeoLocation_Latitude) && has(FIELD_ID_GeoLocation_Longitude) && geo.hasLatLon()
        && std::isfinite(geo.Latitude) && std::isfinite(geo.Longitude)
        && std::abs(geo.Latitude) <= 90 && std::abs(geo.Longitude) <= 180) {
        place.rows.append({tr("Latitude"), coordinateText(geo.Latitude, true)});
        place.rows.append({tr("Longitude"), coordinateText(geo.Longitude, false)});
    }
    if (has(FIELD_ID_GeoLocation_Altitude) && geo.hasAltitude() && std::isfinite(geo.Altitude))
        place.rows.append({tr("Altitude"), geo.Altitude < 0 ? tr("%1 m below sea level").arg(number(-geo.Altitude, 0))
                                                            : tr("%1 m").arg(number(geo.Altitude, 0))});

    QList<ExifSection> sections;
    for (const ExifSection &section : {camera, exposure, picture, place})
        if (!section.rows.isEmpty())
            sections.append(section);
    return sections;
}
