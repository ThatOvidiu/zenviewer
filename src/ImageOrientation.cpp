#include "ImageOrientation.h"

#include "ExifInfo.h"

#include <QImageReader>
#include <QtEndian>

namespace {
// Orientation tag (0x0112) of IFD0 in a TIFF-structured EXIF block, or 0
int exifOrientation(QByteArray exif)
{
    if (exif.startsWith("Exif\0\0"))
        exif.remove(0, 6);
    if (exif.size() < 8)
        return 0;
    const bool little = exif.startsWith("II");
    if (!little && !exif.startsWith("MM"))
        return 0;
    const auto u16 = [&](qsizetype at) -> quint16 {
        return little ? qFromLittleEndian<quint16>(exif.constData() + at) : qFromBigEndian<quint16>(exif.constData() + at);
    };
    const auto u32 = [&](qsizetype at) -> quint32 {
        return little ? qFromLittleEndian<quint32>(exif.constData() + at) : qFromBigEndian<quint32>(exif.constData() + at);
    };

    const qsizetype ifd = u32(4);
    if (ifd + 2 > exif.size())
        return 0;
    const int count = u16(ifd);
    for (int i = 0; i < count; ++i) {
        const qsizetype entry = ifd + 2 + i * 12;
        if (entry + 12 > exif.size())
            return 0;
        if (u16(entry) == 0x0112)
            return u16(entry + 8);
    }
    return 0;
}

QImageIOHandler::Transformations fromExif(int orientation)
{
    using T = QImageIOHandler::Transformation;
    switch (orientation) {
    case 2: return T::TransformationMirror;
    case 3: return T::TransformationRotate180;
    case 4: return T::TransformationFlip;
    case 5: return T::TransformationFlipAndRotate90;
    case 6: return T::TransformationRotate90;
    case 7: return T::TransformationMirrorAndRotate90;
    case 8: return T::TransformationRotate270;
    default: return T::TransformationNone;
    }
}
}

QImageIOHandler::Transformations missingTransformation(QImageReader &reader)
{
    if (reader.supportsOption(QImageIOHandler::ImageTransformation) || reader.format() != "webp")
        return QImageIOHandler::TransformationNone;
    return fromExif(exifOrientation(readExifBlock(reader.fileName())));
}

QImage applyTransformation(const QImage &image, QImageIOHandler::Transformations transformation)
{
    // Mirror/flip first, then rotate clockwise (QImageIOHandler's definition)
    QImage result = image;
    const bool mirror = transformation & QImageIOHandler::TransformationMirror;
    const bool flip = transformation & QImageIOHandler::TransformationFlip;
    if (mirror || flip)
        result = result.transformed(QTransform::fromScale(mirror ? -1 : 1, flip ? -1 : 1));
    if (transformation & QImageIOHandler::TransformationRotate90)
        result = result.transformed(QTransform().rotate(90));
    return result;
}
