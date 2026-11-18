#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QPixmap>
#include <QSet>
#include <QThreadPool>
#include <QTimer>
#include <QWidget>

#include <atomic>

class QVariantAnimation;

// Full-width thumbnail strip centred on the current image, showing it and
// five neighbours on each side. Thumbnails shrink exponentially with distance
// from the centre, all stand on a common baseline, and both ends of the strip
// fade to black. Only thumbnails near the centre are decoded and kept; one still
// decoding shows a spinner. Selections in quick succession jump instead of gliding.
class Filmstrip : public QWidget
{
    Q_OBJECT

public:
    explicit Filmstrip(QWidget *parent = nullptr);
    ~Filmstrip() override;

    void setFiles(const QStringList &files);
    void setCurrentIndex(int index); // does not emit activated()
    void removeAt(int index);
    void setOpacity(qreal opacity);
    void setScale(qreal scale); // 1.0 at a 1080 pixel high view
    // The strip becomes a circle: the first images follow the last. Folders too
    // small to fill the strip without repeats stay a line.
    void setWrapAround(bool wrap);

signals:
    void activated(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    // Positions along the strip keep counting past the ends when it wraps around;
    // fileIndex() maps them to m_files
    bool wraps() const;
    int fileIndex(int position) const;
    qreal normalizedPos(qreal pos) const;
    int distance(int a, int b) const;

    struct Layout
    {
        int first = 0;       // position of rects[0]
        QList<QRectF> rects; // thumbnail rectangles around the centre
    };
    Layout layout() const;
    QSizeF baseSize(int position) const;
    int indexAt(const QPointF &pos) const;
    void requestThumbnails();
    void onThumbnailReady(quint64 generation, const QString &path, const QImage &thumb, bool skipped);
    void updateSpinnerTimer();

    QThreadPool m_pool;
    QStringList m_files;
    QHash<QString, QPixmap> m_thumbs;
    QSet<QString> m_requested;
    QVariantAnimation *m_scroll;
    QElapsedTimer m_lastSelection;
    QTimer m_spinnerTimer;
    qreal m_opacity = 1.0;
    qreal m_scale = 1.0;
    qreal m_pos = 0.0; // position shown at the centre; fractional while scrolling
    int m_current = -1;
    int m_hover = -1;
    bool m_jumpToNext = true; // first selection after setFiles() doesn't animate
    bool m_wrapAround = false;
    quint64 m_generation = 0;
    std::atomic<int> m_center = 0; // read by thumbnail workers to skip stale requests
};
