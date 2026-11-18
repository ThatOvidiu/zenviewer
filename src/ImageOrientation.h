#pragma once

#include <QImage>
#include <QImageIOHandler>

class QImageReader;

// EXIF orientation that the reader's format plugin can't apply itself (Qt's
// WebP plugin ignores it). None when the plugin handles it or there is none.
QImageIOHandler::Transformations missingTransformation(QImageReader &reader);

// Applies an orientation the way QImageReader's autoTransform would.
QImage applyTransformation(const QImage &image, QImageIOHandler::Transformations transformation);
