#include "Filmstrip.h"

#include "ImageLoader.h"
#include "ImageOrientation.h"
#include "OsdIcons.h"

#include <QImageReader>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <QtMath>

namespace {
const QSize ThumbSize(202, 151); // at size factor 1.0 and strip scale 1.0
constexpr qreal CenterScale = 1.2;
constexpr qreal EdgeScale = 0.35;  // asymptotic size far from the centre
constexpr qreal Falloff = 2.2;     // in items; larger = slower shrink
constexpr qreal Gap = 17;          // between thumbnail edges, at scale 1.0
constexpr qreal Padding = 14;
constexpr qreal TopFade = 48;      // the band fades in over this height, above the thumbnails
constexpr int VisibleSide = 5;     // thumbnails shown on each side of the current one
constexpr int PrefetchSide = 8;    // decoded ahead so scrolling doesn't show blanks
constexpr int KeepSide = 16;       // decoded thumbnails further away are dropped
constexpr qreal EndFade = 0.3;     // fraction of the width faded out at each end
const QColor BandColor(8, 8, 10, 190);

// Size factor for a thumbnail u items away from the centre
qreal scaleAt(qreal u)
{
    return EdgeScale + (CenterScale - EdgeScale) * qExp(-qAbs(u) / Falloff);
}

// 1 for the current thumbnail and VisibleSide on each side, fading to 0 one
// item further out, so items entering or leaving while scrolling don't pop
qreal visibilityAt(qreal u)
{
    return qBound(0.0, VisibleSide + 1 - qAbs(u), 1.0);
}

QImage makeThumbnail(const QString &path)
{
    const QSize target = ThumbSize * CenterScale * 2; // sharp on HiDPI at the largest size
    QImageReader reader(path);
    if (!imageFormats().contains(reader.format()))
        return {};
    reader.setAutoTransform(true);
    QSize size = reader.size();
    if (size.isValid()) {
        // Let the decoder scale (much faster for JPEG than decoding full size)
        size.scale(target, Qt::KeepAspectRatio);
        reader.setScaledSize(size);
    }
    // Plugins that can't apply EXIF orientation (WebP) get it applied here;
    // scaling first is fine since the aspect ratio is kept
    const QImageIOHandler::Transformations missing = missingTransformation(reader);
    const QImage image = applyTransformation(reader.read(), missing);
    if (image.isNull())
        return {};
    return image.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
}

Filmstrip::Filmstrip(QWidget *parent)
    : QWidget(parent)
    , m_scroll(new QVariantAnimation(this))
{
    setMouseTracking(true);
    setScale(1.0);

    // Thumbnails get their own pool so they never delay full-size decoding
    m_pool.setMaxThreadCount(2);

    m_scroll->setDuration(280);
    m_scroll->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_scroll, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        const int before = qRound(m_pos);
        m_pos = value.toReal();
        if (qRound(m_pos) != before)
            requestThumbnails();
        update();
    });
    connect(m_scroll, &QVariantAnimation::finished, this, [this] { m_pos = normalizedPos(m_pos); });

    m_spinnerTimer.setInterval(33);
    connect(&m_spinnerTimer, &QTimer::timeout, this, [this] {
        const int center = qRound(m_pos);
        for (int i = center - VisibleSide - 1; i <= center + VisibleSide + 1; ++i) {
            const int file = fileIndex(i);
            if (file >= 0 && m_requested.contains(m_files.at(file))) {
                update();
                return;
            }
        }
    });
}

Filmstrip::~Filmstrip()
{
    m_pool.clear();
    m_pool.waitForDone();
}

bool Filmstrip::wraps() const
{
    return m_wrapAround && m_files.size() >= 2 * VisibleSide + 3;
}

// -1 for positions past the ends of a strip that doesn't wrap
int Filmstrip::fileIndex(int position) const
{
    const int count = int(m_files.size());
    if (wraps())
        return (position % count + count) % count;
    return position >= 0 && position < count ? position : -1;
}

qreal Filmstrip::normalizedPos(qreal pos) const
{
    if (!wraps())
        return pos;
    const qreal count = m_files.size();
    return pos - qFloor(pos / count) * count;
}

int Filmstrip::distance(int a, int b) const
{
    const int direct = qAbs(a - b);
    return wraps() ? qMin(direct, int(m_files.size()) - direct) : direct;
}

void Filmstrip::setWrapAround(bool wrap)
{
    if (wrap == m_wrapAround)
        return;
    m_wrapAround = wrap;
    if (m_files.isEmpty())
        return;
    m_scroll->stop();
    if (m_current >= 0)
        m_pos = m_current;
    m_pos = wraps() ? normalizedPos(m_pos) : qBound(0.0, m_pos, qreal(m_files.size() - 1));
    requestThumbnails();
    update();
}

void Filmstrip::setFiles(const QStringList &files)
{
    ++m_generation;
    m_pool.clear();
    m_thumbs.clear();
    m_scroll->stop();
    m_files = files;
    m_pos = 0;
    m_current = m_hover = -1;
    m_jumpToNext = true;

    m_requested.clear();
    updateSpinnerTimer();
    update();
}

void Filmstrip::updateSpinnerTimer()
{
    if (m_requested.isEmpty())
        m_spinnerTimer.stop();
    else if (!m_spinnerTimer.isActive())
        m_spinnerTimer.start();
}

void Filmstrip::requestThumbnails()
{
    if (m_files.isEmpty())
        return;
    const int center = qRound(m_pos);
    m_center = fileIndex(center);

    // Drop thumbnails far from the centre so large folders stay cheap
    QHash<QString, QPixmap> kept;
    for (int position = center - KeepSide; position <= center + KeepSide; ++position) {
        const int file = fileIndex(position);
        if (file < 0)
            continue;
        if (const auto it = m_thumbs.constFind(m_files.at(file)); it != m_thumbs.cend())
            kept.insert(it.key(), it.value());
    }
    m_thumbs.swap(kept);

    const quint64 generation = m_generation;
    const int circle = wraps() ? int(m_files.size()) : 0; // the workers can't look at m_files
    for (int offset = 0; offset <= PrefetchSide; ++offset) {
        for (int position : {center - offset, center + offset}) {
            const int i = fileIndex(position);
            if (i < 0 || (offset == 0 && position != center))
                continue;
            const QString path = m_files.at(i);
            if (m_thumbs.contains(path) || m_requested.contains(path))
                continue;
            m_requested.insert(path);
            // Nearest first; safe to capture this: the destructor waits for the pool to drain
            m_pool.start([this, generation, path, i, circle] {
                // Skip work queued while scrolling past (e.g. Home/End jumps)
                const int direct = qAbs(i - m_center.load());
                const bool stale = (circle > 0 ? qMin(direct, circle - direct) : direct) > PrefetchSide;
                const QImage thumb = stale ? QImage() : makeThumbnail(path);
                QMetaObject::invokeMethod(this, [this, generation, path, thumb, stale] {
                    onThumbnailReady(generation, path, thumb, stale);
                }, Qt::QueuedConnection);
            }, PrefetchSide - offset);
        }
    }
    updateSpinnerTimer();
}

void Filmstrip::setCurrentIndex(int index)
{
    m_current = index;
    // Selections coming faster than the glide can play just jump
    const bool quick = m_scroll->state() == QAbstractAnimation::Running
        || (m_lastSelection.isValid() && !m_lastSelection.hasExpired(m_scroll->duration()));
    m_lastSelection.start();
    m_scroll->stop();
    // On a circle, go the short way round
    const qreal target = wraps() ? index + qRound((m_pos - index) / m_files.size()) * qreal(m_files.size()) : index;
    if (std::exchange(m_jumpToNext, false) || quick) {
        m_pos = normalizedPos(target);
        requestThumbnails();
    } else {
        m_scroll->setStartValue(m_pos);
        m_scroll->setEndValue(target);
        m_scroll->start();
    }
    update();
}

void Filmstrip::setScale(qreal scale)
{
    m_scale = scale;
    setFixedHeight(qCeil((ThumbSize.height() * CenterScale + 2 * Padding + TopFade) * scale));
    update();
}

void Filmstrip::setOpacity(qreal opacity)
{
    m_opacity = opacity;
    update();
}

void Filmstrip::removeAt(int index)
{
    if (index < 0 || index >= m_files.size())
        return;
    m_thumbs.remove(m_files.takeAt(index));
    if (m_current > index)
        --m_current;
    if (m_pos > index)
        m_pos -= 1;
    m_hover = -1;
    requestThumbnails();
    update();
}

QSizeF Filmstrip::baseSize(int position) const
{
    // Fitted into the landscape slot, so portraits are narrower, not taller
    const QPixmap thumb = m_thumbs.value(m_files.at(fileIndex(position)));
    const QSizeF slot = QSizeF(ThumbSize) * m_scale;
    return thumb.isNull() ? slot : QSizeF(thumb.size()).scaled(slot, Qt::KeepAspectRatio);
}

Filmstrip::Layout Filmstrip::layout() const
{
    Layout result;
    if (m_files.isEmpty())
        return result;

    // m_pos lies between the anchor item and the next one
    const int last = int(m_files.size()) - 1;
    const bool circular = wraps();
    const int anchor = circular ? qFloor(m_pos) : qBound(0, qFloor(m_pos), last);
    result.first = circular ? anchor - VisibleSide - 1 : qMax(0, anchor - VisibleSide - 1);
    const int end = circular ? anchor + VisibleSide + 2 : qMin(last, anchor + VisibleSide + 2);

    QList<QSizeF> sizes;
    for (int i = result.first; i <= end; ++i)
        sizes << baseSize(i) * scaleAt(i - m_pos);
    const auto gapAfter = [this](int i) { return Gap * m_scale * (scaleAt(i - m_pos) + scaleAt(i + 1 - m_pos)) / 2; };

    // Chain centres outwards from the anchor, with the same gap between
    // neighbouring edges whatever the thumbnails' aspect ratios
    QList<qreal> centers(sizes.size());
    const int a = anchor - result.first;
    for (int k = a + 1; k < centers.size(); ++k)
        centers[k] = centers[k - 1] + sizes[k - 1].width() / 2 + gapAfter(result.first + k - 1) + sizes[k].width() / 2;
    for (int k = a - 1; k >= 0; --k)
        centers[k] = centers[k + 1] - sizes[k + 1].width() / 2 - gapAfter(result.first + k) - sizes[k].width() / 2;

    qreal centerPos = centers[a];
    if (a + 1 < centers.size())
        centerPos += (m_pos - anchor) * (centers[a + 1] - centers[a]);
    const qreal shift = width() / 2.0 - centerPos;

    // Bottom-aligned: every thumbnail stands on the same baseline
    for (int k = 0; k < sizes.size(); ++k)
        result.rects << QRectF(shift + centers[k] - sizes[k].width() / 2, height() - Padding * m_scale - sizes[k].height(),
                               sizes[k].width(), sizes[k].height());
    return result;
}

int Filmstrip::indexAt(const QPointF &pos) const
{
    const Layout items = layout();
    for (int k = 0; k < items.rects.size(); ++k) {
        const int i = items.first + k;
        const QRectF rect = items.rects.at(k);
        if (visibilityAt(i - m_pos) > 0.0 && pos.x() >= rect.left() && pos.x() <= rect.right()
            && pos.y() >= rect.top())
            return fileIndex(i);
    }
    return -1;
}

void Filmstrip::paintEvent(QPaintEvent *)
{
    if (m_files.isEmpty())
        return;

    // Composed off-screen so the faded ends can be cut out with a gradient
    const qreal dpr = devicePixelRatioF();
    QImage buffer(size() * dpr, QImage::Format_ARGB32_Premultiplied);
    buffer.setDevicePixelRatio(dpr);
    buffer.fill(Qt::transparent);

    QPainter p(&buffer);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    QLinearGradient band(0, 0, 0, height());
    // Eased at both ends, so neither edge of the fade shows as a line
    constexpr int FadeStops = 8;
    for (int i = 0; i <= FadeStops; ++i) {
        const qreal t = qreal(i) / FadeStops;
        QColor color = BandColor;
        color.setAlphaF(BandColor.alphaF() * t * t * (3 - 2 * t));
        band.setColorAt(t * TopFade * m_scale / height(), color);
    }
    band.setColorAt(1, BandColor);
    p.fillRect(rect(), band);

    // Far thumbnails first, so nearer ones end up on top
    const Layout items = layout();
    QList<int> order;
    for (int k = 0; k < items.rects.size(); ++k) {
        const QRectF rect = items.rects.at(k);
        if (visibilityAt(items.first + k - m_pos) > 0.0 && rect.right() >= 0 && rect.left() <= width())
            order << items.first + k;
    }
    std::sort(order.begin(), order.end(), [this](int a, int b) {
        return qAbs(a - m_pos) > qAbs(b - m_pos);
    });

    for (int i : std::as_const(order)) {
        const QRectF target = items.rects.at(i - items.first);
        const qreal scale = scaleAt(i - m_pos);
        const int file = fileIndex(i);
        const QPixmap thumb = m_thumbs.value(m_files.at(file));
        const qreal radius = 6 * scale * m_scale;
        QPainterPath shape;
        shape.addRoundedRect(target, radius, radius);

        p.setOpacity((file == m_current ? 1.0 : (file == m_hover ? 0.95 : 0.75)) * visibilityAt(i - m_pos));
        if (thumb.isNull()) {
            p.fillPath(shape, QColor(255, 255, 255, 25));
            if (m_requested.contains(m_files.at(file)))
                drawLoadingSpinner(p, target.center(), qMin(target.width(), target.height()) * 0.16);
        } else {
            p.save();
            p.setClipPath(shape);
            p.drawPixmap(target, thumb, QRectF(thumb.rect()));
            p.restore();
        }
        if (file == m_current) {
            p.setPen(QPen(Qt::white, 2.5 * m_scale));
            p.setBrush(Qt::NoBrush);
            const qreal grow = 2 * m_scale;
            p.drawRoundedRect(target.adjusted(-grow, -grow, grow, grow), radius + grow, radius + grow);
        }
    }

    QLinearGradient mask(0, 0, width(), 0);
    mask.setColorAt(0, Qt::transparent);
    mask.setColorAt(EndFade, Qt::black);
    mask.setColorAt(1 - EndFade, Qt::black);
    mask.setColorAt(1, Qt::transparent);
    p.setOpacity(1.0);
    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    p.fillRect(rect(), mask);
    p.end();

    QPainter out(this);
    out.setOpacity(m_opacity);
    out.drawImage(0, 0, buffer);
}

void Filmstrip::wheelEvent(QWheelEvent *event)
{
    if (m_files.isEmpty())
        return;
    // Browse without changing the shown image; a new selection re-centres
    m_scroll->stop();
    const int delta = event->angleDelta().y() + event->angleDelta().x();
    m_pos = wraps() ? normalizedPos(m_pos - delta / 120.0) : qBound(0.0, m_pos - delta / 120.0, qreal(m_files.size() - 1));
    requestThumbnails();
    update();
    event->accept();
}

void Filmstrip::mouseMoveEvent(QMouseEvent *event)
{
    const int hover = indexAt(event->position());
    if (hover == m_hover)
        return;
    m_hover = hover;
    setCursor(hover >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update();
}

void Filmstrip::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const int index = indexAt(event->position());
    if (index >= 0 && index != m_current)
        emit activated(index);
}

void Filmstrip::leaveEvent(QEvent *)
{
    m_hover = -1;
    update();
}

void Filmstrip::onThumbnailReady(quint64 generation, const QString &path, const QImage &thumb, bool skipped)
{
    if (generation != m_generation)
        return;
    m_requested.remove(path);
    if (skipped) {
        requestThumbnails(); // the strip may have come back to it meanwhile
        update();
        return;
    }
    updateSpinnerTimer();
    const int index = int(m_files.indexOf(path));
    if (thumb.isNull() || index < 0 || distance(index, fileIndex(qRound(m_pos))) > KeepSide) {
        update(); // undecodable or scrolled far away: at least stop its spinner
        return;
    }
    m_thumbs.insert(path, QPixmap::fromImage(thumb));
    update();
}
