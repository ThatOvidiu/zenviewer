#include "OsdIcons.h"

#include <QElapsedTimer>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>

namespace {

void fillPath(QPainter &p, const QPainterPath &path, const QColor &color)
{
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPath(path);
    p.restore();
}

QPainterPath triangle(QPointF a, QPointF b, QPointF c)
{
    QPainterPath path;
    path.moveTo(a);
    path.lineTo(b);
    path.lineTo(c);
    path.closeSubpath();
    return path;
}

void glyph(QPainter &p, const QString &text, int pixelSize)
{
    QFont font = p.font();
    font.setPixelSize(pixelSize);
    font.setBold(true);
    p.setFont(font);
    p.drawText(QRectF(0, 0, 24, 24), Qt::AlignCenter, text);
}

// Draws in a 24x24 coordinate space with the pen already set up.
void drawShape(QPainter &p, OsdIcon icon, const QColor &color)
{
    auto mirrored = [&](OsdIcon base) {
        p.save();
        p.translate(24, 0);
        p.scale(-1, 1);
        drawShape(p, base, color);
        p.restore();
    };

    switch (icon) {
    case OsdIcon::None:
        break;
    case OsdIcon::Play:
        fillPath(p, triangle({7, 4.5}, {19.5, 12}, {7, 19.5}), color);
        break;
    case OsdIcon::Pause: {
        QPainterPath path;
        path.addRoundedRect(QRectF(6, 5, 4, 14), 1, 1);
        path.addRoundedRect(QRectF(14, 5, 4, 14), 1, 1);
        fillPath(p, path, color);
        break;
    }
    case OsdIcon::Stop: {
        QPainterPath path;
        path.addRoundedRect(QRectF(6, 6, 12, 12), 1.5, 1.5);
        fillPath(p, path, color);
        break;
    }
    case OsdIcon::Previous: {
        QPainterPath path = triangle({19, 5}, {9, 12}, {19, 19});
        path.addRoundedRect(QRectF(5, 5, 2.5, 14), 1, 1);
        fillPath(p, path, color);
        break;
    }
    case OsdIcon::Next:
        mirrored(OsdIcon::Previous);
        break;
    case OsdIcon::ChevronLeft: {
        QPen pen = p.pen();
        pen.setWidthF(2.5);
        p.setPen(pen);
        p.drawPolyline(QPolygonF({{15, 4}, {7, 12}, {15, 20}}));
        break;
    }
    case OsdIcon::ChevronRight:
        mirrored(OsdIcon::ChevronLeft);
        break;
    case OsdIcon::ZoomIn:
        p.drawLine(QPointF(10.5, 7.5), QPointF(10.5, 13.5));
        [[fallthrough]];
    case OsdIcon::ZoomOut:
        p.drawEllipse(QPointF(10.5, 10.5), 6.5, 6.5);
        p.drawLine(QPointF(15.3, 15.3), QPointF(20, 20));
        p.drawLine(QPointF(7.5, 10.5), QPointF(13.5, 10.5));
        break;
    case OsdIcon::FitWindow:
        p.drawRoundedRect(QRectF(3, 5, 18, 14), 2, 2);
        p.drawPolyline(QPolygonF({{3.5, 17}, {9, 11.5}, {13, 15.5}, {16, 12.5}, {20.5, 17}}));
        fillPath(p, [] { QPainterPath s; s.addEllipse(QPointF(16, 9), 1.6, 1.6); return s; }(), color);
        break;
    case OsdIcon::ActualSize:
        glyph(p, QStringLiteral("1:1"), 10);
        break;
    case OsdIcon::Delete:
        p.drawLine(QPointF(4, 6.5), QPointF(20, 6.5));
        p.drawPolyline(QPolygonF({{9, 6.5}, {9, 4}, {15, 4}, {15, 6.5}}));
        p.drawPolyline(QPolygonF({{6, 6.5}, {7, 20}, {17, 20}, {18, 6.5}}));
        p.drawLine(QPointF(10, 10), QPointF(10, 16.5));
        p.drawLine(QPointF(14, 10), QPointF(14, 16.5));
        break;
    case OsdIcon::Open:
        p.drawPolygon(QPolygonF({{3, 6}, {10, 6}, {12, 8.5}, {21, 8.5}, {21, 19}, {3, 19}}));
        break;
    case OsdIcon::Info:
        p.drawEllipse(QPointF(12, 12), 9, 9);
        fillPath(p, [] { QPainterPath s; s.addEllipse(QPointF(12, 7.8), 1.3, 1.3); return s; }(), color);
        p.drawLine(QPointF(12, 11), QPointF(12, 16.5));
        break;
    case OsdIcon::Fullscreen:
        p.drawPolyline(QPolygonF({{4, 9}, {4, 4}, {9, 4}}));
        p.drawPolyline(QPolygonF({{15, 4}, {20, 4}, {20, 9}}));
        p.drawPolyline(QPolygonF({{20, 15}, {20, 20}, {15, 20}}));
        p.drawPolyline(QPolygonF({{9, 20}, {4, 20}, {4, 15}}));
        break;
    case OsdIcon::Windowed:
        p.drawRoundedRect(QRectF(3.5, 5, 17, 14), 2, 2);
        p.drawLine(QPointF(3.5, 9), QPointF(20.5, 9));
        break;
    case OsdIcon::Filmstrip:
        p.drawRoundedRect(QRectF(2.5, 7, 5.5, 10), 1, 1);
        p.drawRoundedRect(QRectF(9.25, 7, 5.5, 10), 1, 1);
        p.drawRoundedRect(QRectF(16, 7, 5.5, 10), 1, 1);
        break;
    case OsdIcon::Settings: {
        p.save();
        p.translate(12, 12);
        p.setPen(QPen(color, 3.2, Qt::SolidLine, Qt::FlatCap));
        for (int i = 0; i < 8; ++i) {
            p.drawLine(QPointF(0, -6.5), QPointF(0, -9.8));
            p.rotate(45);
        }
        p.restore();
        p.drawEllipse(QPointF(12, 12), 6.5, 6.5);
        p.drawEllipse(QPointF(12, 12), 2.5, 2.5);
        break;
    }
    case OsdIcon::Help:
        p.drawEllipse(QPointF(12, 12), 9, 9);
        glyph(p, QStringLiteral("?"), 13);
        break;
    case OsdIcon::More:
        fillPath(p, [] {
            QPainterPath dots;
            for (qreal x : {5.5, 12.0, 18.5})
                dots.addEllipse(QPointF(x, 12), 1.9, 1.9);
            return dots;
        }(), color);
        break;
    case OsdIcon::Keyboard:
        p.drawRoundedRect(QRectF(2.5, 6.5, 19, 11), 2, 2);
        for (qreal x : {6.5, 10.0, 13.5, 17.0})
            p.drawPoint(QPointF(x, 10));
        p.drawLine(QPointF(8, 14), QPointF(16, 14));
        break;
    case OsdIcon::Camera: {
        // Body with the viewfinder bump on top, and the lens
        QPainterPath body;
        body.moveTo(3, 9);
        body.arcTo(QRectF(3, 7, 4, 4), 180, -90);
        body.lineTo(7.5, 7);
        body.lineTo(9.2, 4.5);
        body.lineTo(14.8, 4.5);
        body.lineTo(16.5, 7);
        body.arcTo(QRectF(17, 7, 4, 4), 90, -90);
        body.lineTo(21, 17.5);
        body.arcTo(QRectF(17, 15.5, 4, 4), 0, -90);
        body.lineTo(5, 19.5);
        body.arcTo(QRectF(3, 15.5, 4, 4), 270, -90);
        body.closeSubpath();
        p.drawPath(body);
        p.drawEllipse(QPointF(12, 12.5), 3.6, 3.6);
        break;
    }
    case OsdIcon::Print: {
        // The paper going in at the top, the body, the printed sheet coming out
        p.drawPolyline(QPolygonF({{7, 8}, {7, 3.5}, {17, 3.5}, {17, 8}}));
        QPainterPath body;
        body.moveTo(6.5, 17);
        body.lineTo(5, 17);
        body.arcTo(QRectF(3, 13, 4, 4), 270, -90);
        body.lineTo(3, 10);
        body.arcTo(QRectF(3, 8, 4, 4), 180, -90);
        body.lineTo(19, 8);
        body.arcTo(QRectF(17, 8, 4, 4), 90, -90);
        body.lineTo(21, 15);
        body.arcTo(QRectF(17, 13, 4, 4), 0, -90);
        body.lineTo(17.5, 17);
        p.drawPath(body);
        p.drawRect(QRectF(7, 13.5, 10, 7));
        break;
    }
    case OsdIcon::Mouse:
        p.drawRoundedRect(QRectF(7, 3.5, 10, 17), 5, 5);
        p.drawLine(QPointF(12, 3.5), QPointF(12, 9));
        break;
    }
}

}

void drawOsdIcon(QPainter &painter, OsdIcon icon, const QRectF &rect, const QColor &color)
{
    const qreal size = qMin(rect.width(), rect.height());
    QRectF box(0, 0, size, size);
    box.moveCenter(rect.center());

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(box.topLeft());
    painter.scale(size / 24.0, size / 24.0);
    painter.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    drawShape(painter, icon, color);
    painter.restore();
}

void drawLoadingSpinner(QPainter &painter, const QPointF &center, qreal radius)
{
    static const QElapsedTimer clock = [] {
        QElapsedTimer timer;
        timer.start();
        return timer;
    }();
    const qreal lineWidth = radius * 0.28;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 50), lineWidth));
    painter.drawEllipse(center, radius, radius);

    // Bright head turning clockwise, with a tail fading out behind it
    QConicalGradient comet(center, -360.0 * (clock.elapsed() % 1000) / 1000.0);
    comet.setColorAt(0, Qt::white);
    comet.setColorAt(0.7, QColor(255, 255, 255, 0));
    comet.setColorAt(1, QColor(255, 255, 255, 0));
    painter.setPen(QPen(QBrush(comet), lineWidth));
    painter.drawEllipse(center, radius, radius);
    painter.restore();
}

QIcon osdIconSet(OsdIcon icon)
{
    QIcon set;
    if (icon == OsdIcon::None)
        return set;
    for (const int size : {16, 20, 24, 32, 40, 48}) {
        for (const auto &[mode, color] : {std::pair{QIcon::Normal, QColor(255, 255, 255)},
                                          std::pair{QIcon::Disabled, QColor(255, 255, 255, 70)}}) {
            QPixmap pixmap(size, size);
            pixmap.fill(Qt::transparent);
            QPainter p(&pixmap);
            p.setRenderHint(QPainter::Antialiasing);
            drawOsdIcon(p, icon, QRectF(0, 0, size, size), color);
            p.end();
            set.addPixmap(pixmap, mode);
        }
    }
    return set;
}
