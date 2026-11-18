#pragma once

#include <QCache>
#include <QMutex>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QThreadPool>

// Decodes images on worker threads. Normal requests deliver a "screen-sized"
// image: no larger than the biggest connected screen, so memory per image
// stays bounded however many megapixels the file has. Full resolution is
// decoded separately, only on demand (zooming in), and is not cached.
// The formats ZenViewer opens. Qt also loads any other image plugin installed on the
// system (PDF, HEIC, ...); those stay out, so all systems behave the same.
const QList<QByteArray> &imageFormats();

class ImageLoader : public QObject
{
    Q_OBJECT

public:
    explicit ImageLoader(QObject *parent = nullptr);
    ~ImageLoader() override;

    // Emits loaded() once the screen-sized image is available (immediately if cached).
    void request(const QString &path);
    // Decodes in the background so a later request() is instant.
    void prefetch(const QString &path);
    void forget(const QString &path);

    // Emits fullLoaded() with the full-resolution image, null if decoding
    // failed. Only one full decode is tracked at a time; releaseFull() drops it.
    // Returns false if nothing was started, so no fullLoaded() will follow.
    bool requestFull(const QString &path);
    void releaseFull();

signals:
    // fullSize is the image's real size (after EXIF rotation), image may be smaller
    void loaded(const QString &path, const QImage &image, const QSize &fullSize, const QString &error);
    void fullLoaded(const QString &path, const QImage &image);

private:
    struct Entry
    {
        QImage image;
        QSize fullSize;
    };

    void start(const QString &path, int priority);
    bool isWanted(const QString &path) const;
    QSize displayBox();

    QThreadPool m_pool;
    QCache<QString, Entry> m_cache;
    QSet<QString> m_pending;
    // Current image plus its prefetches; read by workers to skip stale jobs
    mutable QMutex m_wantedMutex;
    QSet<QString> m_wanted;
    QSize m_displayBox;
    QString m_fullPath;
};
