#include "ImageView.h"

#include <QGraphicsPixmapItem>
#include <QWheelEvent>
#include <QtMath>

namespace {
constexpr qreal MinZoom = 0.01;
constexpr qreal MaxZoom = 32.0;
constexpr qreal ZoomStep = 1.25;
}

ImageView::ImageView(QWidget *parent)
    : QGraphicsView(parent)
{
    auto *scene = new QGraphicsScene(this);
    m_item = new QGraphicsPixmapItem;
    m_item->setTransformationMode(Qt::SmoothTransformation);
    scene->addItem(m_item);
    setScene(scene);

    setFrameShape(QFrame::NoFrame);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // pan by dragging
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ImageView::setImage(const QImage &image, const QSize &fullSize)
{
    m_item->setPixmap(QPixmap::fromImage(image));
    // Stretch a downscaled image back to full size in scene coordinates
    m_item->setScale(fullSize.isValid() && !image.isNull() ? qreal(fullSize.width()) / image.width() : 1.0);
    m_detailRequested = false;
    placeItem();
    setSceneRect(m_item->sceneBoundingRect());
    m_fit = true;
    applyZoom(fitFactor());
}

void ImageView::setFullImage(const QImage &image)
{
    if (!hasImage() || image.isNull())
        return;
    // Same scene footprint, so the view transform and scroll position stay put
    m_item->setPixmap(QPixmap::fromImage(image));
    m_item->setScale(1.0);
    placeItem();
}

void ImageView::placeItem()
{
    // Scale about the pixmap's centre, and keep that centre at the
    // centre of the full-size picture, so swapping resolutions doesn't move it
    const QPointF center = m_item->boundingRect().center();
    m_item->setTransformOriginPoint(center);
    m_item->setPos(center * (m_item->scale() - 1.0));
}

void ImageView::clear()
{
    m_item->setPixmap(QPixmap());
    setSceneRect(QRectF());
    resetTransform();
    m_zoom = 1.0;
    emit imageRectChanged();
}

bool ImageView::hasImage() const
{
    return !m_item->pixmap().isNull();
}

QRect ImageView::imageRect() const
{
    if (!hasImage())
        return {};
    return mapFromScene(m_item->sceneBoundingRect()).boundingRect().intersected(viewport()->rect());
}

QSizeF ImageView::fittedSize() const
{
    return m_item->sceneBoundingRect().size() * fitFactor();
}

void ImageView::zoomIn()
{
    setZoom(m_zoom * ZoomStep);
}

void ImageView::zoomOut()
{
    setZoom(m_zoom / ZoomStep);
}

void ImageView::setZoom(qreal factor)
{
    m_fit = false;
    applyZoom(factor);
    emit zoomChanged(m_zoom);
}

void ImageView::fitToWindow()
{
    m_fit = true;
    applyZoom(fitFactor());
    emit zoomChanged(m_zoom);
}

void ImageView::actualSize()
{
    setZoom(1.0);
}

void ImageView::toggleActualSize()
{
    if (!m_fit && qFuzzyCompare(m_zoom, 1.0))
        fitToWindow();
    else
        actualSize();
}

void ImageView::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0)
        return;
    event->accept();
    if (event->modifiers() & Qt::ControlModifier) {
        // 120 units per notch -> one ZoomStep per notch, smooth for touchpads
        if (hasImage())
            setZoom(m_zoom * qPow(ZoomStep, delta / 120.0));
        return;
    }
    // One image per notch; touchpads send small steps, so add them up
    if ((m_wheelRest < 0) != (delta < 0))
        m_wheelRest = 0;
    m_wheelRest += delta;
    const int steps = m_wheelRest / 120;
    m_wheelRest -= steps * 120;
    for (int i = 0; i < qAbs(steps); ++i)
        emit navigateRequested(steps > 0 ? -1 : +1); // away from you = previous
}

void ImageView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::MiddleButton || !hasImage())
        return QGraphicsView::mousePressEvent(event);
    if (m_fit)
        actualSize();
    else
        fitToWindow();
}

void ImageView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return QGraphicsView::mouseDoubleClickEvent(event);
    emit doubleClicked();
}

void ImageView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (m_fit && hasImage())
        applyZoom(fitFactor());
    emit imageRectChanged();
}

void ImageView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    emit imageRectChanged();
}

void ImageView::applyZoom(qreal factor)
{
    m_zoom = qBound(MinZoom, factor, MaxZoom);
    setTransform(QTransform::fromScale(m_zoom, m_zoom));
    emit imageRectChanged();
    checkDetail();
}

void ImageView::checkDetail()
{
    // Screen pixels per pixel of the shown image; above 1 it is being upscaled
    const qreal magnification = m_zoom * m_item->scale() * devicePixelRatioF();
    if (!m_detailRequested && hasImage() && m_item->scale() > 1.0 && magnification > 1.02) {
        m_detailRequested = true;
        emit detailNeeded();
    }
}

qreal ImageView::fitFactor() const
{
    const QRectF rect = m_item->sceneBoundingRect();
    if (rect.isEmpty())
        return 1.0;
    const QSize vp = viewport()->size();
    const qreal factor = qMin(vp.width() / rect.width(), vp.height() / rect.height());
    // Don't upscale images smaller than the window
    return qMin(factor, 1.0);
}
