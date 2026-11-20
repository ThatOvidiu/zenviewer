#pragma once

#include <QColor>
#include <QIcon>

class QPainter;
class QPointF;
class QRectF;

// Vector icons for the on-screen display, drawn on a 24x24 grid so they
// look the same on every platform and scale cleanly.
enum class OsdIcon {
    None,
    Play,
    Pause,
    Stop,
    Previous,
    Next,
    ChevronLeft,
    ChevronRight,
    ZoomIn,
    ZoomOut,
    FitWindow,
    ActualSize,
    Delete,
    Open,
    Info,
    Fullscreen,
    Windowed, // a window with a title bar: leave full screen
    Filmstrip,
    Settings,
    Help,
    Camera, // EXIF details
    Print,
    More, // three dots: About
    Keyboard,
    Mouse,
};

void drawOsdIcon(QPainter &painter, OsdIcon icon, const QRectF &rect, const QColor &color);

// The icon in white, greyed out when disabled, for menus
QIcon osdIconSet(OsdIcon icon);

// "Still loading" circle. Turns with the clock, so callers only need to keep repainting.
void drawLoadingSpinner(QPainter &painter, const QPointF &center, qreal radius);
