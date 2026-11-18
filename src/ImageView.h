#pragma once

#include <QGraphicsView>

class QGraphicsPixmapItem;

// Zoomable, pannable image canvas.
class ImageView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit ImageView(QWidget *parent = nullptr);

    // image may be a downscaled copy of a picture fullSize large; the view is
    // laid out in full-size coordinates either way (zoom 1.0 = 1:1)
    void setImage(const QImage &image, const QSize &fullSize = {});
    // Swaps in the full-resolution image, keeping zoom and position
    void setFullImage(const QImage &image);
    void clear();
    bool hasImage() const;
    // Visible part of the image, in view coordinates
    QRect imageRect() const;
    // Size on screen when fitted to the window, whatever the current zoom
    QSizeF fittedSize() const;

    qreal zoom() const { return m_zoom; }
    bool isFitMode() const { return m_fit; }

public slots:
    void zoomIn();
    void zoomOut();
    void setZoom(qreal factor);
    void fitToWindow();
    void actualSize();
    // 1:1, or back to fitting the window if already at 1:1
    void toggleActualSize();

signals:
    // Only for user-initiated zoom, not for automatic refits on resize/load
    void zoomChanged(qreal factor);
    void doubleClicked();
    // The wheel asks for the previous (-1) or next (+1) image
    void navigateRequested(int step);
    void imageRectChanged();
    // The zoom shows more detail than the downscaled image has (once per image)
    void detailNeeded();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    void applyZoom(qreal factor);
    qreal fitFactor() const;
    void checkDetail();
    void placeItem();

    QGraphicsPixmapItem *m_item = nullptr;
    qreal m_zoom = 1.0;
    bool m_fit = true;
    int m_wheelRest = 0; // wheel movement not yet worth a whole image
    bool m_detailRequested = false;
};
