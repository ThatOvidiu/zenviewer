#include "ImageLoader.h"

#include "ImageOrientation.h"

#include <QFutureWatcher>
#include <QGuiApplication>
#include <QImageReader>
#include <QPromise>
#include <QScreen>

#include <memory>

namespace {
constexpr int CachedScreens = 6;       // cache holds this many screen-sized images
constexpr int MinCacheSizeKiB = 32 * 1024;
const QSize FallbackDisplayBox(3840, 2160);
constexpr int RequestPriority = 1; // the image to show now jumps queued prefetches
constexpr int PrefetchPriority = 0;

struct DecodeResult
{
    QImage image;
    QSize fullSize;
    QString error;
    bool skipped = false; // no longer wanted when its turn came
};

// Decodes path to fit inside box (in pixels); an invalid box means full size.
DecodeResult decode(const QString &path, const QSize &box)
{
    QImageReader reader(path);
    if (!imageFormats().contains(reader.format())) {
        DecodeResult unsupported;
        unsupported.error = QObject::tr("Unsupported format");
        return unsupported;
    }
    reader.setAutoTransform(true); // honour EXIF orientation

    // The reader scales before applying the EXIF rotation, so work in both orientations
    const QImageIOHandler::Transformations missing = missingTransformation(reader);
    const bool turned = (reader.transformation() | missing) & QImageIOHandler::TransformationRotate90;
    const QSize raw = reader.size();
    DecodeResult result;
    result.fullSize = turned ? raw.transposed() : raw;
    if (box.isValid() && result.fullSize.isValid()
        && (result.fullSize.width() > box.width() || result.fullSize.height() > box.height())) {
        const QSize target = result.fullSize.scaled(box, Qt::KeepAspectRatio);
        reader.setScaledSize(turned ? target.transposed() : target);
    }

    result.image = applyTransformation(reader.read(), missing);
    if (result.image.isNull())
        result.error = reader.errorString();
    else if (!result.fullSize.isValid())
        result.fullSize = result.image.size();
    return result;
}

template <typename Task>
QFuture<DecodeResult> spawn(QThreadPool &pool, int priority, Task task)
{
    // Shared because Qt before 6.6 only runs functions it can copy, and a promise can't be copied
    auto promise = std::make_shared<QPromise<DecodeResult>>();
    QFuture<DecodeResult> future = promise->future();
    pool.start([promise, task = std::move(task)] {
        promise->start();
        promise->addResult(task());
        promise->finish();
    }, priority);
    return future;
}
}

const QList<QByteArray> &imageFormats()
{
    static const QList<QByteArray> formats = {"bmp", "cur", "gif", "icns", "ico", "jpeg", "jpg", "mng", "pbm", "pgm",
                                              "png", "ppm", "svg", "svgz", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm"};
    return formats;
}

ImageLoader::ImageLoader(QObject *parent)
    : QObject(parent)
{
    // Decoders that can't scale (e.g. WebP) briefly hold the full image;
    // two at a time keeps that peak bounded for very large files.
    m_pool.setMaxThreadCount(2);
}

ImageLoader::~ImageLoader()
{
    m_pool.clear();
    m_pool.waitForDone();
}

QSize ImageLoader::displayBox()
{
    // Largest screen in physical pixels: a fitted image never needs more,
    // whichever screen the window is on.
    QSize box;
    for (const QScreen *screen : QGuiApplication::screens())
        box = box.expandedTo(screen->geometry().size() * screen->devicePixelRatio());
    if (box.isEmpty())
        box = FallbackDisplayBox;
    if (box != m_displayBox) {
        m_displayBox = box;
        m_cache.clear(); // decoded for a different screen setup
        // Enough for the current image, the prefetched neighbours and a few
        // recently viewed ones, at 4 bytes per pixel
        const qint64 screenKiB = qint64(box.width()) * box.height() * 4 / 1024;
        m_cache.setMaxCost(int(qMax<qint64>(MinCacheSizeKiB, CachedScreens * screenKiB)));
    }
    return box;
}

void ImageLoader::request(const QString &path)
{
    displayBox();
    {
        // A new current image: earlier prefetches are only worth decoding if asked again
        const QMutexLocker locker(&m_wantedMutex);
        m_wanted = {path};
    }
    if (const Entry *entry = m_cache.object(path)) {
        emit loaded(path, entry->image, entry->fullSize, {});
        return;
    }
    start(path, RequestPriority);
}

void ImageLoader::prefetch(const QString &path)
{
    displayBox();
    {
        const QMutexLocker locker(&m_wantedMutex);
        m_wanted.insert(path);
    }
    if (!m_cache.contains(path))
        start(path, PrefetchPriority);
}

bool ImageLoader::isWanted(const QString &path) const
{
    const QMutexLocker locker(&m_wantedMutex);
    return m_wanted.contains(path);
}

void ImageLoader::forget(const QString &path)
{
    m_cache.remove(path);
    if (path == m_fullPath)
        releaseFull();
}

void ImageLoader::start(const QString &path, int priority)
{
    if (m_pending.contains(path))
        return;
    m_pending.insert(path);

    auto *watcher = new QFutureWatcher<DecodeResult>(this);
    connect(watcher, &QFutureWatcher<DecodeResult>::finished, this, [this, watcher, path] {
        const DecodeResult result = watcher->result();
        watcher->deleteLater();
        m_pending.remove(path);
        if (result.skipped) {
            if (isWanted(path)) // wanted again while it sat in the queue
                start(path, PrefetchPriority);
            return;
        }
        if (!result.image.isNull()) {
            const qsizetype cost = qMax<qsizetype>(1, result.image.sizeInBytes() / 1024);
            m_cache.insert(path, new Entry{result.image, result.fullSize}, cost);
        }
        emit loaded(path, result.image, result.fullSize, result.error);
    });
    // Safe to capture this: the destructor drains the pool first
    const QSize box = m_displayBox;
    watcher->setFuture(spawn(m_pool, priority, [this, path, box] {
        // Skip images browsed past while queued (e.g. holding an arrow key)
        if (isWanted(path))
            return decode(path, box);
        DecodeResult skipped;
        skipped.skipped = true;
        return skipped;
    }));
}

bool ImageLoader::requestFull(const QString &path)
{
    if (path == m_fullPath)
        return false; // already decoding (or delivered)
    m_fullPath = path;

    // Global pool: never queued behind prefetches
    auto *watcher = new QFutureWatcher<DecodeResult>(this);
    connect(watcher, &QFutureWatcher<DecodeResult>::finished, this, [this, watcher, path] {
        const DecodeResult result = watcher->result();
        watcher->deleteLater();
        if (path == m_fullPath)
            emit fullLoaded(path, result.image);
    });
    watcher->setFuture(spawn(*QThreadPool::globalInstance(), 0, [path] { return decode(path, QSize()); }));
    return true;
}

void ImageLoader::releaseFull()
{
    m_fullPath.clear();
}
