#include "OsdWidgets.h"

#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QAction>
#include <QApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QLayout>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSplitter>
#include <QStyleHints>
#include <QSysInfo>
#include <QTimer>
#include <QTreeView>
#include <QVariantAnimation>
#include <QtMath>

namespace {
const QColor PanelColor(18, 18, 20, 200);
const QColor OutlineColor(255, 255, 255, 30); // keeps panels visible on black letterboxing
const QColor HoverColor(255, 255, 255, 40);
const QColor CheckedColor(255, 255, 255, 55);
const QColor TextColor(255, 255, 255);
const QColor DimTextColor(255, 255, 255, 160);
const QColor DisabledColor(255, 255, 255, 70);

constexpr qreal BarPadding = 9;
constexpr qreal ButtonSize = 44;
constexpr qreal PrimarySize = 53;
constexpr qreal SeparatorWidth = 14;
constexpr qreal ButtonGap = 4;

constexpr qreal DockReferenceHeight = 1080; // window height at which the dock has its nominal size
constexpr qreal MaxMagnification = 1.9;
constexpr qreal MagnifyRange = 150; // distance from the cursor where magnification ends
constexpr qreal MagnifyGrowth = (MaxMagnification - 1) * MagnifyRange; // extra dock height

const QColor DockColor(0, 0, 0, 128);
const QColor DockPrimaryColor(40, 160, 95);

QFont pixelFont(const QFont &base, int pixelSize, int weight = QFont::Normal)
{
    QFont font(base);
    // Open letter shapes and a little air between them read best from across a room, like TV captions.
    // Windows has Tahoma (and the wider Verdana) everywhere; Linux mostly has Noto Sans.
#ifdef Q_OS_WIN
    font.setFamilies({"Tahoma", "Verdana", "Arial"});
    font.setLetterSpacing(QFont::PercentageSpacing, 96); // Tahoma's own spacing is already generous
#else
    font.setFamilies({"Noto Sans", "Liberation Sans", "Arial"});
    font.setLetterSpacing(QFont::PercentageSpacing, 102);
#endif
    font.setPixelSize(pixelSize);
    font.setWeight(QFont::Weight(weight));
    return font;
}

QFont hintFont(const QFont &base, qreal scale) { return pixelFont(base, qMax(1, qRound(20 * scale)), QFont::DemiBold); }

// With a fractional display scale (125 %, 150 %…) an edge at an arbitrary
// logical pixel falls inside a screen pixel. Masks and repaints with such edges
// leave the half-covered screen pixels stale on Windows, which shows as thin
// lines. Every gridStep() logical pixels is a whole number of screen pixels:
// 4 at 125 %, 2 at 150 %, 1 at 100 % and 200 %.
int gridStep(const QWidget *widget)
{
    const qreal ratio = widget->devicePixelRatioF();
    for (int step = 1; step <= 8; ++step)
        if (qAbs(step * ratio - qRound(step * ratio)) < 0.01)
            return step;
    return 1;
}

int floorToStep(int value, int step)
{
    return value >= 0 ? value / step * step : -((-value + step - 1) / step * step);
}

// Grows region, in widget's coordinates, outward until its edges fall on whole
// screen pixels. The grid starts at the window's corner, which is on one.
QRegion snapToScreenPixels(const QRegion &region, const QWidget *widget)
{
    const int step = gridStep(widget);
    if (step == 1)
        return region;
    const QPoint origin = widget->mapTo(widget->window(), QPoint(0, 0));
    QRegion snapped;
    for (const QRect &r : region) {
        const QRect w = r.translated(origin);
        const int left = floorToStep(w.left(), step);
        const int top = floorToStep(w.top(), step);
        const int right = -floorToStep(-(w.right() + 1), step); // exclusive, rounded up
        const int bottom = -floorToStep(-(w.bottom() + 1), step);
        snapped += QRect(left, top, right - left, bottom - top).translated(-origin);
    }
    return snapped;
}

}

qreal osdScale(int height)
{
    return qMax(1, height) / DockReferenceHeight;
}

qreal osdScale(const QWidget *view)
{
    return osdScale(view->height());
}

QString keyLabel(const QKeySequence &sequence)
{
    static const QHash<QString, QString> arrows = {
        {"Left", "←"}, {"Right", "→"}, {"Up", "↑"}, {"Down", "↓"}};
    const QString text = sequence.toString(QKeySequence::NativeText);
    return arrows.value(text, text);
}

// ---------------------------------------------------------------- OsdWidget

OsdWidget::OsdWidget(QWidget *parent)
    : QWidget(parent)
{
}

void OsdWidget::setOpacity(qreal opacity)
{
    m_opacity = opacity;
    setVisible(opacity > 0.01);
    update();
}

// ----------------------------------------------------------------- DockHint

DockHint::DockHint(QWidget *parent)
    : OsdWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setOpacity(0);
}

void DockHint::setText(const QString &text)
{
    m_text = text;
    resize(sizeHint());
    update();
}

void DockHint::setScale(qreal scale)
{
    m_scale = scale;
    resize(sizeHint());
    update();
}

QSize DockHint::sizeHint() const
{
    const int textWidth = QFontMetrics(hintFont(font(), m_scale)).horizontalAdvance(m_text);
    return {textWidth + qRound(35 * m_scale), qRound(42 * m_scale)};
}

void DockHint::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_opacity);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(DockColor);
    p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
    p.setPen(TextColor);
    p.setFont(hintFont(font(), m_scale));
    p.drawText(r, Qt::AlignCenter, m_text);
}

// --------------------------------------------------------------- ControlBar

ControlBar::ControlBar(QWidget *parent)
    : OsdWidget(parent)
    , m_hint(new DockHint(parent))
    , m_magnifyAnim(new QVariantAnimation(this))
    , m_followAnim(new QVariantAnimation(this))
{
    setMouseTracking(true);
    m_followAnim->setDuration(160);
    m_followAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_followAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_mouseY = v.toReal();
        updateMagnification();
    });
    m_magnifyAnim->setDuration(180);
    m_magnifyAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_magnifyAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_magnify = v.toReal();
        updateMagnification();
    });
}

void ControlBar::addButton(QAction *action, OsdIcon icon, bool primary)
{
    m_items.append(Item{action, icon, primary, {}, {}});
    connect(action, &QAction::changed, this, [this] {
        updateHint();
        update();
    });
    relayout();
}

void ControlBar::addSeparator()
{
    m_items.append(Item{});
    relayout();
}

void ControlBar::setKeysFrom(QAction *action, QAction *keys)
{
    for (Item &item : m_items)
        if (item.action == action)
            item.keysFrom = keys;
    updateHint();
}

void ControlBar::setIcon(QAction *action, OsdIcon icon)
{
    for (Item &item : m_items)
        if (item.action == action)
            item.icon = icon;
    update();
}

qreal ControlBar::naturalHeight() const
{
    qreal height = 2 * BarPadding;
    for (const Item &item : m_items)
        height += (item.action ? (item.primary ? PrimarySize : ButtonSize) : SeparatorWidth) + ButtonGap;
    return height - ButtonGap;
}

int ControlBar::panelWidth() const
{
    return qRound((PrimarySize + 2 * BarPadding) * m_scale);
}

QSize ControlBar::sizeHint() const
{
    const qreal width = BarPadding + PrimarySize * MaxMagnification;
    const qreal height = naturalHeight() + MagnifyGrowth;
    return {qCeil(width * m_scale), qCeil(height * m_scale)};
}

void ControlBar::scaleToHeight(int windowHeight)
{
    const qreal scale = qMax(1, windowHeight) / DockReferenceHeight;
    if (!qFuzzyCompare(scale, m_scale)) {
        m_scale = scale;
        relayout();
    }
}

void ControlBar::relayout()
{
    const qreal panel = panelWidth();
    m_restTop = (sizeHint().height() - naturalHeight() * m_scale) / 2;
    m_restBottom = m_restTop + naturalHeight() * m_scale;
    qreal y = m_restTop + BarPadding * m_scale;
    for (Item &item : m_items) {
        const qreal size = (item.action ? (item.primary ? PrimarySize : ButtonSize) : SeparatorWidth) * m_scale;
        item.rest = item.action ? QRectF((panel - size) / 2, y, size, size) : QRectF(0, y, panel, size);
        y += size + ButtonGap * m_scale;
    }
    m_hint->setScale(m_scale);
    if (m_keyboard && m_hover >= 0) {
        m_followAnim->stop();
        m_mouseY = m_items[m_hover].rest.center().y();
    }
    updateGeometry();
    updateMagnification();
}

void ControlBar::updateMagnification()
{
    // Stretches the dock around the cursor: the magnification at distance d is
    // 1 + amount * cos²(π d / 2 range), and this is its integral, so everything
    // moves continuously and the point under the cursor stays where it is.
    const qreal amount = (MaxMagnification - 1) * m_magnify;
    const qreal range = MagnifyRange * m_scale;
    const auto warp = [&](qreal y) {
        const qreal d = qBound(-range, y - m_mouseY, range);
        return y + amount * (d / 2 + range / (2 * M_PI) * qSin(M_PI * d / range));
    };

    m_panelTop = warp(m_restTop);
    m_panelBottom = warp(m_restBottom);

    // The column keeps catching the mouse between buttons, so moving along it doesn't flicker
    QRegion mask = QRectF(0, m_panelTop, panelWidth(), m_panelBottom - m_panelTop).toAlignedRect();
    for (Item &item : m_items) {
        const qreal top = warp(item.rest.top());
        const qreal size = warp(item.rest.bottom()) - top;
        if (!item.action) {
            item.rect = QRectF(item.rest.left(), top, item.rest.width(), size);
            continue;
        }
        item.rect = QRectF(item.rest.left(), top, size, size);
        if (size > item.rest.height() + 0.5)
            mask += item.rect.toAlignedRect();
    }
    // Keeps the empty parts of the widget from swallowing clicks on the image.
    // The image is repainted under the old and the new outline, not just where
    // the bar shrank: on Windows with a fractional display scale the outline's
    // edges fall inside screen pixels and would otherwise keep stale lines.
    mask = snapToScreenPixels(mask, this);
    const QRegion oldMask = this->mask();
    if (mask != oldMask) {
        setMask(mask);
        repaintBehind((oldMask | mask).translated(pos()));
    }
    updateHint();
    update();
}

void ControlBar::animateMagnification(qreal target)
{
    m_magnifyAnim->stop();
    m_magnifyAnim->setStartValue(m_magnify);
    m_magnifyAnim->setEndValue(target);
    m_magnifyAnim->start();
}

void ControlBar::repaintBehind(const QRegion &region)
{
    // The image under a changing mask or a moved hint isn't repainted by itself,
    // which would leave slivers of them on it. region is in parent coordinates.
    // It grows by 2 px and then to whole screen pixels (see snapToScreenPixels).
    auto *view = qobject_cast<QAbstractScrollArea *>(parentWidget());
    if (!view)
        return;
    QWidget *viewport = view->viewport();
    QRegion grown;
    for (const QRect &r : region)
        grown += r.adjusted(-2, -2, 2, 2);
    viewport->update(snapToScreenPixels(grown.translated(-viewport->pos()), viewport));
}

void ControlBar::updateHint()
{
    const QRect oldHint = m_hint->isVisible() ? m_hint->geometry() : QRect();
    if (m_hover < 0) {
        m_hint->setOpacity(0);
        repaintBehind(oldHint);
        return;
    }
    const QRectF r = m_items[m_hover].rect;
    const Item &item = m_items[m_hover];
    const QKeySequence key = (item.keysFrom ? item.keysFrom : item.action)->shortcut();
    m_hint->setText(key.isEmpty() ? item.action->text() : QStringLiteral("%1 [%2]").arg(item.action->text(), keyLabel(key)));
    m_hint->move(mapToParent(QPointF(r.right() + 12 * m_scale, r.center().y()).toPoint()) - QPoint(0, m_hint->height() / 2));
    // On whole screen pixels too, so its own repaints leave no lines either
    m_hint->setGeometry(snapToScreenPixels(QRegion(m_hint->geometry()), parentWidget()).boundingRect());
    m_hint->setOpacity(1);
    m_hint->raise();
    repaintBehind(QRegion(oldHint) | m_hint->geometry());
}

int ControlBar::itemAt(const QPointF &pos) const
{
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items[i].action && m_items[i].rect.contains(pos))
            return i;
    return -1;
}

void ControlBar::setHover(int index)
{
    if (index == m_hover)
        return;
    m_hover = index;
    const bool clickable = index >= 0 && m_items[index].action->isEnabled();
    setCursor(clickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    updateHint();
    update();
}

void ControlBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_opacity);
    p.setPen(Qt::NoPen);

    for (int i = 0; i < m_items.size(); ++i) {
        const Item &item = m_items[i];
        if (!item.action)
            continue;

        const bool enabled = item.action->isEnabled();
        const bool hover = enabled && i == m_hover;
        const bool pressed = hover && i == m_pressed;
        const qreal size = item.rect.height();

        if (item.primary) {
            QColor fill = DockPrimaryColor;
            if (hover)
                fill = fill.lighter(125);
            fill.setAlpha(enabled ? 255 : 90);
            const qreal shrink = pressed ? 2 * m_scale : 0;
            p.setBrush(fill);
            p.drawEllipse(item.rect.adjusted(shrink, shrink, -shrink, -shrink));
        } else {
            p.setBrush(DockColor);
            p.drawEllipse(item.rect);
            if (item.action->isChecked() || hover) {
                p.setBrush(pressed ? CheckedColor.lighter() : (item.action->isChecked() ? CheckedColor : HoverColor));
                p.drawEllipse(item.rect);
            }
        }

        const qreal inset = size * (item.primary ? 14.5 / PrimarySize : 10 / ButtonSize);
        drawOsdIcon(p, item.icon, item.rect.adjusted(inset, inset, -inset, -inset), enabled ? TextColor : DisabledColor);
    }
}

void ControlBar::selectDefault()
{
    int index = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items[i].action || !m_items[i].action->isEnabled())
            continue;
        if (index < 0 || m_items[i].primary)
            index = i;
        if (m_items[i].primary)
            break;
    }
    if (index >= 0)
        select(index);
}

void ControlBar::moveSelection(int step)
{
    if (m_hover < 0) {
        selectDefault();
        return;
    }
    const int count = int(m_items.size());
    int index = m_hover;
    for (int tries = 0; tries < count; ++tries) {
        index = (index + step + count) % count;
        if (m_items[index].action && m_items[index].action->isEnabled()) {
            select(index);
            return;
        }
    }
}

void ControlBar::select(int index)
{
    const qreal target = m_items[index].rest.center().y();
    const bool magnified = m_magnify > 0.0;
    m_keyboard = true;
    setHover(index);
    m_followAnim->stop();
    if (magnified) {
        m_followAnim->setStartValue(m_mouseY);
        m_followAnim->setEndValue(target);
        m_followAnim->start();
    } else {
        m_mouseY = target; // nothing to glide from: grow in place
    }
    animateMagnification(1.0);
}

void ControlBar::activateSelection()
{
    if (m_hover < 0 || !m_items[m_hover].action->isEnabled())
        return;
    // Same press feedback as a mouse click
    m_pressed = m_hover;
    update();
    QTimer::singleShot(120, this, [this] {
        m_pressed = -1;
        update();
    });
    m_items[m_hover].action->trigger();
}

void ControlBar::endKeyboardNavigation()
{
    if (!m_keyboard)
        return;
    m_keyboard = false;
    m_followAnim->stop();
    if (!underMouse()) { // otherwise the mouse takes over from where it is
        setHover(-1);
        animateMagnification(0.0);
    }
}

void ControlBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_keyboard)
        return;
    m_mouseY = event->position().y();
    updateMagnification();
    setHover(itemAt(event->position()));
}

void ControlBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = itemAt(event->position());
        update();
    }
}

void ControlBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const int index = itemAt(event->position());
    const int pressed = std::exchange(m_pressed, -1);
    update();
    if (index >= 0 && index == pressed && m_items[index].action->isEnabled())
        m_items[index].action->trigger();
}

void ControlBar::enterEvent(QEnterEvent *event)
{
    if (m_keyboard)
        return;
    m_mouseY = event->position().y();
    animateMagnification(1.0);
}

void ControlBar::leaveEvent(QEvent *)
{
    if (m_keyboard)
        return;
    setHover(-1);
    animateMagnification(0.0);
}

void ControlBar::hideEvent(QHideEvent *)
{
    m_magnifyAnim->stop();
    m_followAnim->stop();
    m_keyboard = false;
    m_magnify = 0.0;
    m_hover = -1;
    updateMagnification();
}

// ---------------------------------------------------------------- OsdButton

OsdButton::OsdButton(QAction *action, OsdIcon icon, QWidget *parent)
    : OsdWidget(parent)
    , m_action(action)
    , m_icon(icon)
{
    resize(sizeHint());
    setCursor(Qt::PointingHandCursor);
    connect(action, &QAction::changed, this, qOverload<>(&QWidget::update));
}

QSize OsdButton::sizeHint() const
{
    const int size = qRound(62 * osdScale(parentWidget()));
    return {size, size};
}

void OsdButton::paintEvent(QPaintEvent *)
{
    if (!m_action->isEnabled())
        return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_opacity);
    p.setPen(QPen(OutlineColor, 1));
    p.setBrush(m_hover ? QColor(40, 40, 44, 220) : QColor(18, 18, 20, 150));
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    p.drawEllipse(r);
    const qreal inset = r.width() * 15 / 60;
    drawOsdIcon(p, m_icon, r.adjusted(inset, inset, -inset, -inset), TextColor);
}

void OsdButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()) && m_action->isEnabled())
        m_action->trigger();
}

void OsdButton::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void OsdButton::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

namespace {
constexpr int InfoPadding = 30;
constexpr int InfoPaddingY = 20;
constexpr int InfoTitleHeight = 48;
constexpr int InfoLineHeight = 30;
const QColor BackdropColor(0, 0, 0, 128); // under the info panel and pause symbol

QFont infoTitleFont(const QFont &base, qreal scale) { return pixelFont(base, qMax(1, qRound(38 * scale)), QFont::Bold); }
QFont infoLineFont(const QFont &base, qreal scale) { return pixelFont(base, qMax(1, qRound(20 * scale)), QFont::Medium); }

void drawShadowedText(QPainter &p, const QRectF &rect, const QString &text, const QColor &color)
{
    p.setPen(QColor(0, 0, 0, 160));
    p.drawText(rect.translated(1, 2), Qt::AlignLeft | Qt::AlignVCenter, text);
    p.setPen(color);
    p.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, text);
}
}

// ------------------------------------------------------------------- TopBar

TopBar::TopBar(QWidget *parent)
    : OsdWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void TopBar::setTexts(const QString &title, const QString &counter)
{
    m_title = title;
    m_counter = counter;
    update();
}

void TopBar::setTextIndent(int indent)
{
    m_textIndent = indent;
    update();
}

void TopBar::setScale(qreal scale)
{
    m_scale = scale;
    update();
}

void TopBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setOpacity(m_opacity);

    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0, QColor(0, 0, 0, 170));
    gradient.setColorAt(1, QColor(0, 0, 0, 0));
    p.fillRect(rect(), gradient);

    // Same type sizes as the info panel: title, then the counter on its baseline
    const qreal s = m_scale;
    const int margin = qRound(30 * s);
    const int gap = qRound(24 * s);
    const int left = qMax(margin, m_textIndent);
    const QFont titleFont = infoTitleFont(font(), s);
    const QFont counterFont = infoLineFont(font(), s);
    const int counterWidth = QFontMetrics(counterFont).horizontalAdvance(m_counter) + gap;
    const qreal baseline = 18 * s + QFontMetrics(titleFont).ascent();

    p.setFont(titleFont);
    p.setPen(TextColor);
    const QString title = p.fontMetrics().elidedText(m_title, Qt::ElideMiddle, width() - left - margin - counterWidth);
    p.drawText(QPointF(left, baseline), title);

    p.setFont(counterFont);
    p.setPen(QColor(255, 255, 255, 215));
    p.drawText(QPointF(left + QFontMetrics(titleFont).horizontalAdvance(title) + gap, baseline), m_counter);
}

// ---------------------------------------------------------------- InfoPanel

InfoPanel::InfoPanel(QWidget *parent)
    : OsdWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void InfoPanel::setContent(const QString &title, const QStringList &lines)
{
    m_title = title;
    m_lines = lines;
    updateGeometry();
    update();
}

void InfoPanel::setMaximumTextWidth(int width)
{
    m_maxWidth = width;
}

void InfoPanel::setScale(qreal scale)
{
    m_scale = scale;
    update();
}

QSize InfoPanel::sizeHint() const
{
    const qreal s = m_scale;
    int textWidth = QFontMetrics(infoTitleFont(font(), s)).horizontalAdvance(m_title);
    const QFontMetrics lineMetrics(infoLineFont(font(), s));
    for (const QString &line : m_lines)
        textWidth = qMax(textWidth, lineMetrics.horizontalAdvance(line));
    // A few px of slack: integer advances can round below the drawn width and elide the text
    const int width = qMin(textWidth + 4, m_maxWidth) + qRound(2 * InfoPadding * s);
    const int height = qRound((2 * InfoPaddingY + InfoTitleHeight + int(m_lines.size()) * InfoLineHeight) * s);
    return {width, height};
}

void InfoPanel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_opacity);

    // Flush with the image corner; only the inner (bottom-right) corner is rounded
    const qreal s = m_scale;
    const qreal radius = 14 * s;
    p.setPen(Qt::NoPen);
    p.setBrush(BackdropColor);
    p.drawRoundedRect(QRectF(-radius, -radius, width() + radius, height() + radius), radius, radius);

    const int padding = qRound(InfoPadding * s);
    const int titleHeight = qRound(InfoTitleHeight * s);
    const int lineHeight = qRound(InfoLineHeight * s);
    const int textWidth = width() - 2 * padding;
    int y = qRound(InfoPaddingY * s);
    p.setFont(infoTitleFont(font(), s));
    drawShadowedText(p, QRect(padding, y, textWidth, titleHeight),
                     p.fontMetrics().elidedText(m_title, Qt::ElideMiddle, textWidth), TextColor);
    y += titleHeight;

    p.setFont(infoLineFont(font(), s));
    for (const QString &line : m_lines) {
        drawShadowedText(p, QRect(padding, y, textWidth, lineHeight),
                         p.fontMetrics().elidedText(line, Qt::ElideRight, textWidth), QColor(255, 255, 255, 215));
        y += lineHeight;
    }
}

// ----------------------------------------------------------------- OsdToast

namespace {
constexpr qreal ToastFontSize = 54;
constexpr qreal ToastHeight = 128;
constexpr qreal ToastIconSize = 72;
constexpr qreal ToastIconGap = 26;
constexpr qreal ToastPadding = 56;
constexpr qreal ToastMinFit = 0.4; // how far a long message may shrink before it gets elided
}

OsdToast::OsdToast(QWidget *parent)
    : OsdWidget(parent)
    , m_fade(new QVariantAnimation(this))
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setOpacity(0);

    m_timer.setSingleShot(true);
    m_fade->setDuration(300);
    m_fade->setStartValue(1.0);
    m_fade->setEndValue(0.0);
    connect(&m_timer, &QTimer::timeout, m_fade, [this] { m_fade->start(); });
    connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) { setOpacity(v.toReal()); });
}

void OsdToast::showMessage(const QString &text, OsdIcon icon, int durationMs)
{
    m_text = text;
    m_icon = icon;
    m_fade->stop();
    resize(sizeHint());
    setOpacity(1.0);
    raise();
    m_timer.start(durationMs);
}

void OsdToast::setScale(qreal scale)
{
    m_scale = scale;
    update();
}

void OsdToast::setMaximumTextWidth(int width)
{
    m_maxWidth = width;
    update();
}

QFont OsdToast::textFont(qreal scale) const
{
    return pixelFont(font(), qMax(1, qRound(ToastFontSize * scale)), QFont::DemiBold);
}

qreal OsdToast::naturalWidth(qreal scale) const
{
    const qreal icon = m_icon == OsdIcon::None ? 0 : (ToastIconSize + ToastIconGap) * scale;
    return QFontMetricsF(textFont(scale)).horizontalAdvance(m_text) + icon + 2 * ToastPadding * scale;
}

qreal OsdToast::fittedScale() const
{
    const qreal natural = naturalWidth(m_scale);
    if (natural <= m_maxWidth)
        return m_scale;
    // Font sizes round, so leave a little room rather than elide a message that fits
    return m_scale * qMax(ToastMinFit, 0.98 * m_maxWidth / natural);
}

qreal OsdToast::textLeft(qreal scale) const
{
    return (ToastPadding + (m_icon == OsdIcon::None ? 0 : ToastIconSize + ToastIconGap)) * scale;
}

QRectF OsdToast::textRect(qreal scale) const
{
    const qreal left = textLeft(scale);
    return QRectF(left, 0, width() - left - ToastPadding / 2 * scale, height());
}

QString OsdToast::visibleText(qreal scale) const
{
    return QFontMetrics(textFont(scale)).elidedText(m_text, Qt::ElideMiddle, qFloor(textRect(scale).width()));
}

QSize OsdToast::sizeHint() const
{
    const qreal s = fittedScale();
    return {qMin(m_maxWidth, qCeil(naturalWidth(s))), qRound(ToastHeight * s)};
}

void OsdToast::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_opacity);
    p.setPen(QPen(OutlineColor, 1));
    p.setBrush(PanelColor);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.drawRoundedRect(r, r.height() / 2, r.height() / 2);

    const qreal s = fittedScale();
    if (m_icon != OsdIcon::None) {
        const qreal icon = ToastIconSize * s;
        drawOsdIcon(p, m_icon, QRectF(ToastPadding * s, (r.height() - icon) / 2, icon, icon), TextColor);
    }
    p.setFont(textFont(s));
    p.setPen(TextColor);
    p.drawText(textRect(s), Qt::AlignVCenter | Qt::AlignLeft, visibleText(s));
}

// ----------------------------------------------------------- PauseIndicator

PauseIndicator::PauseIndicator(QWidget *parent)
    : QWidget(parent)
    , m_blink(new QVariantAnimation(this))
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hide();

    m_blink->setDuration(1920); // one calm pulse, bright to dim and back
    m_blink->setStartValue(1.0);
    m_blink->setKeyValueAt(0.5, 0.15);
    m_blink->setEndValue(1.0);
    m_blink->setEasingCurve(QEasingCurve::InOutSine);
    m_blink->setLoopCount(-1);
    connect(m_blink, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_level = v.toReal();
        update();
    });
}

void PauseIndicator::setActive(bool active)
{
    setVisible(active);
    if (active) {
        raise();
        m_blink->start();
    } else {
        m_blink->stop();
    }
}

void PauseIndicator::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(width() / 76.0, height() / 76.0); // drawn on a 76 x 76 grid
    p.setPen(Qt::NoPen);
    p.setBrush(BackdropColor); // steady; only the bars blink
    p.drawRoundedRect(QRectF(0, 0, 76, 76), 14, 14);

    p.setOpacity(m_level);
    const QRectF left(22, 18, 11, 40);
    const QRectF right(43, 18, 11, 40);
    p.setBrush(Qt::white);
    p.drawRoundedRect(left, 2.5, 2.5);
    p.drawRoundedRect(right, 2.5, 2.5);
}

// -------------------------------------------------------------- ActionFlash

ActionFlash::ActionFlash(QWidget *parent)
    : QWidget(parent)
    , m_fade(new QVariantAnimation(this))
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hide();

    m_fade->setDuration(320);
    m_fade->setStartValue(0.0);
    m_fade->setKeyValueAt(0.3, 1.0);
    m_fade->setEndValue(0.0);
    connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_level = v.toReal();
        update();
    });
    connect(m_fade, &QVariantAnimation::finished, this, &QWidget::hide);
}

void ActionFlash::flash(OsdIcon icon)
{
    m_icon = icon;
    m_fade->stop();
    m_level = 0.0;
    show();
    raise();
    m_fade->start();
}

void ActionFlash::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_level);
    p.setPen(Qt::NoPen);
    p.setBrush(DockColor);
    p.drawEllipse(QRectF(rect()));
    const qreal inset = width() * 0.2;
    drawOsdIcon(p, m_icon, QRectF(rect()).adjusted(inset, inset, -inset, -inset), TextColor);
}

// ----------------------------------------------------------- LoadingSpinner

LoadingSpinner::LoadingSpinner(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hide();
    m_delay.setSingleShot(true);
    m_delay.setInterval(120);
    m_tick.setInterval(33);
    connect(&m_delay, &QTimer::timeout, this, [this] {
        show();
        m_tick.start();
    });
    connect(&m_tick, &QTimer::timeout, this, qOverload<>(&QWidget::update));
}

void LoadingSpinner::setActive(bool active)
{
    if (!active) {
        m_delay.stop();
        m_tick.stop();
        hide();
    } else if (!isVisible() && !m_delay.isActive()) {
        m_delay.start();
    }
}

void LoadingSpinner::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(DockColor);
    p.drawEllipse(QRectF(rect()));
    drawLoadingSpinner(p, QRectF(rect()).center(), width() * 0.28);
}

// -------------------------------------------------------------- HelpOverlay

HelpOverlay::HelpOverlay(QWidget *parent)
    : QWidget(parent)
{
    hide();
}

namespace {
// Help layout at scale 1; the overlay finds the largest scale that fits
constexpr qreal HelpMargin = 20;
constexpr qreal HelpPad = 28; // around the card's contents, and between columns
constexpr qreal HelpSidePad = 52; // left and right inside the card
constexpr qreal HelpDividerGap = 72; // between the keyboard and the mouse columns, line in the middle
constexpr qreal HelpTypeShare = 0.95; // text and icons a little under the largest size that fits
constexpr qreal HelpHeader = 46;
constexpr qreal HelpRow = 40;
constexpr qreal HelpCapHeight = 28;
constexpr qreal HelpCapPadding = 16;
constexpr qreal HelpCapGap = 6;
constexpr qreal HelpKeysGap = 18;
constexpr qreal HelpIconSlot = 34;

QFont helpKeyFont(const QFont &base, qreal scale) { return pixelFont(base, qMax(1, qRound(16 * scale)), QFont::DemiBold); }
QFont helpTextFont(const QFont &base, qreal scale) { return pixelFont(base, qMax(1, qRound(18 * scale))); }
}

void HelpOverlay::setEntries(const QList<Entry> &keyboard, const QList<Entry> &mouse)
{
    m_keyboard = keyboard;
    m_mouse = mouse;
    updateMetrics();
}

void HelpOverlay::setReservedLeft(int width)
{
    if (width != m_reservedLeft) {
        m_reservedLeft = width;
        updateMetrics();
    }
}

void HelpOverlay::resizeEvent(QResizeEvent *)
{
    updateMetrics();
}

void HelpOverlay::mousePressEvent(QMouseEvent *)
{
    emit closeRequested();
}

QString HelpOverlay::plainText() const
{
    const auto keysOf = [](const Entry &entry) { return entry.keys.join(QStringLiteral(", ")); };
    int keysWidth = 0;
    for (const QList<Entry> *entries : {&m_keyboard, &m_mouse})
        for (const Entry &entry : *entries)
            keysWidth = qMax(keysWidth, int(keysOf(entry).size()));

    QString text;
    for (const auto &[title, entries] : {std::pair{tr("Keyboard:"), &m_keyboard}, std::pair{tr("Mouse:"), &m_mouse}}) {
        text += title + '\n';
        for (const Entry &entry : *entries)
            text += QStringLiteral("  %1  %2\n").arg(keysOf(entry), -keysWidth).arg(entry.text);
        text += '\n';
    }
    return text.trimmed() + '\n';
}

QRectF HelpOverlay::availableRect() const
{
    return QRectF(m_reservedLeft, 0, qMax(0, width() - m_reservedLeft), height())
        .adjusted(HelpMargin, HelpMargin, -HelpMargin, -HelpMargin);
}

void HelpOverlay::addColumns(Metrics &metrics, OsdIcon header, const QList<Entry> &entries, int maxRows) const
{
    const QFontMetricsF keyMetrics(helpKeyFont(font(), metrics.scale));
    const QFontMetricsF textMetrics(helpTextFont(font(), metrics.scale));
    // As few columns as fit, sharing the entries evenly; each is only as wide as it needs
    const int count = int((entries.size() + maxRows - 1) / maxRows);
    const int rows = int((entries.size() + count - 1) / qMax(1, count));
    for (qsizetype first = 0; first < entries.size(); first += rows) {
        Column column;
        column.header = first == 0 ? header : OsdIcon::None;
        column.entries = entries.mid(first, rows);
        for (const Entry &entry : std::as_const(column.entries)) {
            qreal keys = 0;
            for (const QString &key : entry.keys)
                keys += keyMetrics.horizontalAdvance(key) + (HelpCapPadding + HelpCapGap) * metrics.scale;
            column.keysWidth = qMax(column.keysWidth, keys);
            column.textWidth = qMax(column.textWidth, textMetrics.horizontalAdvance(entry.text) + 2);
        }
        metrics.columns.append(column);
    }
}

bool HelpOverlay::computeMetrics(qreal scale, Metrics &metrics) const
{
    const QRectF available = availableRect();
    const int maxRows = int((available.height() - (2 * HelpPad + HelpHeader) * scale) / (HelpRow * scale));
    if (maxRows < 1)
        return false;

    metrics = {};
    metrics.scale = scale;
    addColumns(metrics, OsdIcon::Keyboard, m_keyboard, maxRows);
    metrics.mouseStart = metrics.columns.size();
    addColumns(metrics, OsdIcon::Mouse, m_mouse, maxRows);

    qreal width = 2 * HelpSidePad * scale;
    qsizetype rows = 0;
    for (qsizetype i = 0; i < metrics.columns.size(); ++i) {
        const Column &column = metrics.columns[i];
        if (i > 0)
            width += (i == metrics.mouseStart ? HelpDividerGap : HelpPad) * scale;
        width += column.keysWidth + (HelpKeysGap + HelpIconSlot) * scale + column.textWidth;
        rows = qMax(rows, column.entries.size());
    }
    metrics.card = QSizeF(width, (2 * HelpPad + HelpHeader + rows * HelpRow) * scale);
    return width <= available.width();
}

void HelpOverlay::updateMetrics()
{
    // Larger type only ever needs more room, so the largest fit can be bisected
    qreal low = 0.3;
    qreal high = 8.0;
    computeMetrics(low, m_metrics);
    for (int i = 0; i < 14; ++i) {
        const qreal middle = (low + high) / 2;
        Metrics candidate;
        if (computeMetrics(middle, candidate)) {
            m_metrics = candidate;
            low = middle;
        } else {
            high = middle;
        }
    }
    computeMetrics(m_metrics.scale * HelpTypeShare, m_metrics);
    update();
}

void HelpOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(0, 0, 0, 170));

    const qreal s = m_metrics.scale;
    QRectF card(QPointF(0, 0), m_metrics.card);
    card.moveCenter(availableRect().center());

    p.setPen(QPen(OutlineColor, 1));
    p.setBrush(QColor(22, 22, 25, 235));
    p.drawRoundedRect(card, 18 * s, 18 * s);

    const QRectF inner = card.adjusted(HelpSidePad * s, HelpPad * s, -HelpSidePad * s, -HelpPad * s);
    qreal x = inner.left();
    for (qsizetype i = 0; i < m_metrics.columns.size(); ++i) {
        const Column &column = m_metrics.columns[i];
        if (i > 0 && i == m_metrics.mouseStart) {
            // Keyboard on the left, mouse on the right, with a clear line between them
            const qreal lineX = x + HelpDividerGap * s / 2;
            p.setPen(QPen(QColor(255, 255, 255, 120), qMax(1.0, 2 * s)));
            p.drawLine(QPointF(lineX, inner.top()), QPointF(lineX, inner.bottom()));
            x += HelpDividerGap * s;
        } else if (i > 0) {
            x += HelpPad * s;
        }
        const qreal columnWidth = column.keysWidth + (HelpKeysGap + HelpIconSlot) * s + column.textWidth;
        paintColumn(p, QRectF(x, inner.top(), columnWidth, inner.height()), column);
        x += columnWidth;
    }
}

void HelpOverlay::paintColumn(QPainter &p, const QRectF &rect, const Column &column) const
{
    const qreal s = m_metrics.scale;
    drawOsdIcon(p, column.header, QRectF(rect.left(), rect.top(), 34 * s, 34 * s), TextColor);

    const QFont keyFont = helpKeyFont(font(), s);
    const QFont textFont = helpTextFont(font(), s);
    const qreal capHeight = HelpCapHeight * s;
    const qreal rowHeight = HelpRow * s;
    qreal y = rect.top() + HelpHeader * s;

    for (const Entry &entry : column.entries) {
        const qreal mid = y + rowHeight / 2;

        // Key caps
        p.setFont(keyFont);
        qreal x = rect.left();
        for (const QString &key : entry.keys) {
            const qreal w = QFontMetricsF(keyFont).horizontalAdvance(key) + HelpCapPadding * s;
            const QRectF cap(x, mid - capHeight / 2, w, capHeight);
            p.setPen(QPen(QColor(255, 255, 255, 110), 1));
            p.setBrush(QColor(255, 255, 255, 22));
            p.drawRoundedRect(cap, 5 * s, 5 * s);
            p.setPen(TextColor);
            p.drawText(cap, Qt::AlignCenter, key);
            x += w + HelpCapGap * s;
        }

        x = rect.left() + column.keysWidth + HelpKeysGap * s;
        if (entry.icon != OsdIcon::None)
            drawOsdIcon(p, entry.icon, QRectF(x, mid - 12 * s, 24 * s, 24 * s), DimTextColor);
        x += HelpIconSlot * s;

        p.setFont(textFont);
        p.setPen(TextColor);
        const QRectF textRect(x, y, rect.right() - x, rowHeight);
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(entry.text, Qt::ElideRight, int(textRect.width())));
        y += rowHeight;
    }
}

// ----------------------------------------------------------- ConfirmOverlay

namespace {
constexpr int ConfirmQuestionSize = 44;
constexpr QSizeF ConfirmButtonSize(190, 64);
constexpr qreal ConfirmButtonGap = 24;
}

ConfirmOverlay::ConfirmOverlay(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    hide();
}

void ConfirmOverlay::ask(const QString &question, const QString &yesText, const QString &noText)
{
    m_question = question;
    m_yesText = yesText;
    m_noText = noText;
    m_yesSelected = false; // the safe choice is the default
    show();
    raise();
    setFocus(Qt::OtherFocusReason);
    update();
}

void ConfirmOverlay::finish(bool yes)
{
    hide();
    emit yes ? accepted() : rejected();
}

// The card and its buttons are laid out at nominal size; osdScale() is applied when painting
QRectF ConfirmOverlay::cardRect() const
{
    const qreal s = osdScale(this);
    const QFontMetrics metrics(pixelFont(font(), ConfirmQuestionSize, QFont::Bold));
    const qreal buttonsWidth = 2 * ConfirmButtonSize.width() + ConfirmButtonGap;
    const qreal w = qMin(width() / s - 48.0, qMax<qreal>(metrics.horizontalAdvance(m_question), buttonsWidth) + 2 * 56);
    QRectF card(0, 0, w, 56 + metrics.height() + 40 + ConfirmButtonSize.height() + 30 + 20 + 32);
    card.moveCenter(QPointF(width(), height()) / (2 * s));
    return card;
}

QRectF ConfirmOverlay::buttonRect(bool yes) const
{
    const QRectF card = cardRect();
    const qreal top = card.bottom() - 32 - 20 - 30 - ConfirmButtonSize.height();
    // No on the left, Yes on the right
    const qreal left = card.center().x() + (yes ? ConfirmButtonGap / 2 : -ConfirmButtonGap / 2 - ConfirmButtonSize.width());
    return QRectF(QPointF(left, top), ConfirmButtonSize);
}

bool ConfirmOverlay::event(QEvent *event)
{
    // Claim every key so window shortcuts (arrows, Space, Enter, ...) don't fire
    if (event->type() == QEvent::ShortcutOverride) {
        event->accept();
        return true;
    }
    return QWidget::event(event);
}

bool ConfirmOverlay::focusNextPrevChild(bool)
{
    return false; // Tab moves between the buttons, and reaches keyPressEvent only this way
}

void ConfirmOverlay::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        m_yesSelected = !m_yesSelected;
        update();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        finish(m_yesSelected);
        break;
    case Qt::Key_Y:
        finish(true);
        break;
    case Qt::Key_N:
    case Qt::Key_Escape:
        finish(false);
        break;
    default:
        break;
    }
}

void ConfirmOverlay::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF pos = event->position() / osdScale(this);
    for (bool yes : {false, true}) {
        if (buttonRect(yes).contains(pos) && m_yesSelected != yes) {
            m_yesSelected = yes;
            update();
        }
    }
}

void ConfirmOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const QPointF pos = event->position() / osdScale(this);
    if (buttonRect(true).contains(pos))
        finish(true);
    else if (buttonRect(false).contains(pos) || !cardRect().contains(pos))
        finish(false); // clicking outside the card cancels
}

void ConfirmOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(0, 0, 0, 170));
    p.scale(osdScale(this), osdScale(this));

    const QRectF card = cardRect();
    p.setPen(QPen(OutlineColor, 1));
    p.setBrush(QColor(22, 22, 25, 235));
    p.drawRoundedRect(card, 22, 22);

    const QFont questionFont = pixelFont(font(), ConfirmQuestionSize, QFont::Bold);
    p.setFont(questionFont);
    p.setPen(TextColor);
    const QRectF questionRect(card.left() + 24, card.top() + 56, card.width() - 48, QFontMetrics(questionFont).height());
    p.drawText(questionRect, Qt::AlignCenter,
               p.fontMetrics().elidedText(m_question, Qt::ElideRight, int(questionRect.width())));

    p.setFont(pixelFont(font(), 24, QFont::DemiBold));
    for (bool yes : {false, true}) {
        const QRectF button = buttonRect(yes);
        const bool selected = yes == m_yesSelected;
        p.setPen(selected ? Qt::NoPen : QPen(QColor(255, 255, 255, 110), 1.5));
        p.setBrush(selected ? TextColor : QColor(255, 255, 255, 22));
        p.drawRoundedRect(button, button.height() / 2, button.height() / 2);
        p.setPen(selected ? QColor(18, 18, 20) : TextColor);
        p.drawText(button, Qt::AlignCenter, yes ? m_yesText : m_noText);
    }

    p.setFont(pixelFont(font(), 14));
    p.setPen(DimTextColor);
    p.drawText(QRectF(card.left(), card.bottom() - 32 - 20, card.width(), 20), Qt::AlignCenter,
               tr("←  →  choose   ·   Enter  confirm   ·   Y / N   ·   Esc  cancel"));
}

// -------------------------------------------------------------- OpenOverlay

namespace {
// A QFileDialog that also accepts folders: Open with a folder selected, or the
// extra button for the folder being shown. Entering a folder is a double-click.
class FileOrFolderDialog : public QFileDialog
{
public:
    using QFileDialog::QFileDialog;

    void addFolderButton(const QString &text)
    {
        auto *buttons = findChild<QDialogButtonBox *>();
        QPushButton *button = buttons->addButton(text, QDialogButtonBox::ActionRole);
        button->setAutoDefault(false);
        connect(button, &QPushButton::clicked, this, [this] {
            m_folder = directory().absolutePath();
            QDialog::accept();
        });
    }

    QString chosenPath() const
    {
        if (!m_folder.isEmpty())
            return m_folder;
        const QStringList files = selectedFiles();
        return files.isEmpty() ? QString() : files.first();
    }

protected:
    void accept() override
    {
        const QStringList files = selectedFiles();
        if (files.size() == 1 && QFileInfo(files.first()).isDir()) {
            m_folder = files.first();
            QDialog::accept();
            return;
        }
        QFileDialog::accept();
    }

private:
    QString m_folder;
};

// Round × that closes the open dialog, for the mouse
class DialogCloseButton : public QAbstractButton
{
public:
    explicit DialogCloseButton(QWidget *parent)
        : QAbstractButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setToolTip(QFileDialog::tr("Cancel"));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setPen(QPen(OutlineColor, 1));
        p.setBrush(isDown() ? QColor(255, 255, 255, 90) : underMouse() ? QColor(255, 255, 255, 55) : DockColor);
        p.drawEllipse(r);
        p.setPen(QPen(TextColor, qMax(1.5, width() / 14.0), Qt::SolidLine, Qt::RoundCap));
        const QRectF x = r.adjusted(r.width() * 0.33, r.height() * 0.33, -r.width() * 0.33, -r.height() * 0.33);
        p.drawLine(x.topLeft(), x.bottomRight());
        p.drawLine(x.topRight(), x.bottomLeft());
    }
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }
};

// The open dialog's colours: the system's light or dark mode where Qt can tell
// (Qt 6.5 and newer, on most systems), else a soft neutral grey
QPalette openDialogPalette(QPalette palette)
{
    enum { Dark, Light, Neutral } scheme = Neutral;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    switch (QGuiApplication::styleHints()->colorScheme()) {
    case Qt::ColorScheme::Dark: scheme = Dark; break;
    case Qt::ColorScheme::Light: scheme = Light; break;
    default: break;
    }
#endif
    struct Colors { QColor window, base, alternate, text, placeholder, button, disabled; };
    const Colors c = scheme == Dark ? Colors{{32, 32, 35}, {20, 20, 22}, {28, 28, 31}, TextColor, DimTextColor,
                                             {46, 46, 50}, DisabledColor}
                   : scheme == Light ? Colors{{243, 243, 243}, {255, 255, 255}, {247, 247, 247}, {27, 27, 27},
                                              {112, 112, 112}, {229, 229, 229}, {165, 165, 165}}
                                     : Colors{{214, 214, 217}, {232, 232, 234}, {224, 224, 227}, {32, 32, 34},
                                              {100, 100, 104}, {200, 200, 204}, {150, 150, 154}};
    palette.setColor(QPalette::Window, c.window);
    palette.setColor(QPalette::WindowText, c.text);
    palette.setColor(QPalette::Base, c.base);
    palette.setColor(QPalette::AlternateBase, c.alternate);
    palette.setColor(QPalette::Text, c.text);
    palette.setColor(QPalette::PlaceholderText, c.placeholder);
    palette.setColor(QPalette::Button, c.button);
    palette.setColor(QPalette::ButtonText, c.text);
    palette.setColor(QPalette::Highlight, DockPrimaryColor);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, c.disabled);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, c.disabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, c.disabled);
    return palette;
}
}

// The file list's Type column, which the dialog leaves out: name, size and date say enough
constexpr int TypeColumn = 2;

OpenOverlay::OpenOverlay(QWidget *parent)
    : QWidget(parent)
    , m_close(new DialogCloseButton(this))
{
    setFocusPolicy(Qt::StrongFocus);
    connect(m_close, &QAbstractButton::clicked, this, [this] {
        if (m_dialog)
            m_dialog->reject();
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Follows the system switching between light and dark while the dialog is open
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &OpenOverlay::applyColorScheme);
#endif
    hide();
}

void OpenOverlay::applyColorScheme()
{
    if (m_dialog)
        m_dialog->setPalette(openDialogPalette(m_dialog->palette()));
}

void OpenOverlay::ask(const QString &title, const QString &startDir, const QString &nameFilters)
{
    if (m_dialog)
        return;
    auto *dialog = new FileOrFolderDialog(this, title, startDir, nameFilters);
    m_dialog = dialog;
    dialog->setWindowFlags(Qt::Widget); // embedded, not a window of its own
    dialog->setOption(QFileDialog::DontUseNativeDialog);
    dialog->setFileMode(QFileDialog::ExistingFile);
    dialog->setViewMode(QFileDialog::Detail);
    dialog->setAutoFillBackground(true);
    dialog->addFolderButton(tr("Open this folder"));

    applyColorScheme();

    connect(dialog, &QDialog::finished, this, [this, dialog](int result) {
        const QString path = result == QDialog::Accepted ? dialog->chosenPath() : QString();
        // Qt saves the columns for every app's file dialog, so leave them as they were found
        if (auto *list = dialog->findChild<QTreeView *>("treeView"))
            list->header()->setSectionHidden(TypeColumn, false);
        hide();
        m_dialog = nullptr;
        dialog->deleteLater();
        emit closed();
        if (!path.isEmpty())
            emit chosen(path);
    });

    show();
    raise();
    placeDialog();
    dialog->show();
    fitSidebar();
    // The list, so the arrow keys and Enter browse straight away
    if (auto *list = dialog->findChild<QTreeView *>("treeView")) {
        list->header()->setSectionResizeMode(QHeaderView::ResizeToContents); // no truncated sizes and dates
        list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        list->setFocus(Qt::OtherFocusReason);
    }
}

void OpenOverlay::placeDialog()
{
    if (!m_dialog)
        return;
    // Follows the window like the rest of the OSD
    m_dialog->setFont(pixelFont(font(), qMax(11, qRound(19.2 * height() / DockReferenceHeight))));
    const QSize size(width() * 11 / 20, height() * 3 / 4);
    const QRect dialog(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
    m_dialog->setGeometry(dialog);
    // Room between the dialog's edge and its contents, and between the file list and the
    // file name, type and buttons below it; both grow with the font
    const int em = m_dialog->font().pixelSize();
    if (QLayout *layout = m_dialog->layout())
        layout->setContentsMargins(em, em, em, em);
    if (auto *splitter = m_dialog->findChild<QSplitter *>("splitter"))
        splitter->setContentsMargins(0, 0, 0, em * 3 / 4);
    // The × sits on the dimmed backdrop just above the dialog's top-right corner
    const int button = qMax(28, qRound(2.4 * m_dialog->font().pixelSize()));
    const int gap = qMax(6, button / 4);
    m_close->setGeometry(dialog.right() + 1 - button, qMax(0, dialog.top() - gap - button), button, button);
    m_close->raise();
    fitSidebar();
}

void OpenOverlay::fitSidebar()
{
    // The splitter keeps its width whatever the font, which would cut the names in the sidebar
    auto *splitter = m_dialog->findChild<QSplitter *>("splitter");
    auto *sidebar = m_dialog->findChild<QListView *>("sidebar");
    if (!splitter || !sidebar)
        return;
    m_dialog->layout()->activate();
    const int sidebarWidth = sidebar->sizeHintForColumn(0) + 2 * sidebar->frameWidth() + sidebar->spacing() + 4;
    splitter->setSizes({sidebarWidth, splitter->width() - sidebarWidth});
    if (auto *list = m_dialog->findChild<QTreeView *>("treeView"))
        list->header()->setSectionHidden(TypeColumn, true);
}

bool OpenOverlay::event(QEvent *event)
{
    // Claim every key so window shortcuts don't fire while browsing or typing
    if (event->type() == QEvent::ShortcutOverride) {
        event->accept();
        return true;
    }
    return QWidget::event(event);
}

void OpenOverlay::resizeEvent(QResizeEvent *)
{
    placeDialog();
}

void OpenOverlay::mousePressEvent(QMouseEvent *)
{
    if (m_dialog)
        m_dialog->reject();
}

void OpenOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 170));
}

// ---------------------------------------------------------- SettingsOverlay

namespace {
constexpr qreal SettingsMargin = 20; // from the window, when the panel has to shrink
constexpr qreal SettingsColumnGap = 48;
constexpr qreal SettingsRowHeight = 38;
constexpr qreal SettingsMarkerSlot = 30;
constexpr qreal SettingsTitleGap = 8;
constexpr qreal SettingsDoneGap = 16;
constexpr qreal SettingsDoneHeight = 58;
constexpr qreal SettingsFooterHeight = 44;
}

SettingsOverlay::SettingsOverlay(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    hide();
}

void SettingsOverlay::setGroups(const QList<Group> &groups)
{
    m_groups = groups;
    update();
}

void SettingsOverlay::setCurrent(int group, int value)
{
    m_groups[group].current = value;
    update();
}

void SettingsOverlay::open()
{
    m_group = 0;
    select(0, 0);
    for (int row = 0; row < m_groups[0].options.size(); ++row)
        if (m_groups[0].options[row].value == m_groups[0].current)
            select(0, row);
    show();
    raise();
    setFocus(Qt::OtherFocusReason);
}

void SettingsOverlay::close()
{
    hide();
    emit closed();
}

void SettingsOverlay::select(int group, int row)
{
    m_group = group;
    m_row = row;
    m_doneSelected = false;
    update();
}

void SettingsOverlay::selectDone()
{
    m_doneSelected = true;
    update();
}

SettingsOverlay::Layout SettingsOverlay::computeLayout() const
{
    const QFontMetricsF titleMetrics(infoTitleFont(font(), 1.0));
    const QFontMetricsF lineMetrics(infoLineFont(font(), 1.0));

    // Size at nominal scale, in the info panel's type
    QList<qreal> widths;
    qsizetype rows = 0;
    qreal width = 2 * InfoPadding - SettingsColumnGap;
    for (const Group &group : m_groups) {
        qreal column = titleMetrics.horizontalAdvance(group.title) + 2;
        for (const Option &option : group.options)
            column = qMax(column, SettingsMarkerSlot + lineMetrics.horizontalAdvance(option.text) + 16);
        widths << column;
        width += column + SettingsColumnGap;
        rows = qMax(rows, group.options.size());
    }
    const qreal height = 2 * InfoPaddingY + InfoTitleHeight + SettingsTitleGap + rows * SettingsRowHeight + SettingsDoneGap + SettingsDoneHeight + SettingsFooterHeight;

    // Smaller than the rest of the display only if the window can't hold it
    Layout layout;
    layout.scale = qMin(osdScale(this), qMin((this->width() - 2 * SettingsMargin) / width, (this->height() - 2 * SettingsMargin) / height));
    const qreal s = layout.scale;
    layout.panel = QRectF(0, 0, width * s, height * s);
    layout.panel.moveCenter(QRectF(rect()).center());

    qreal x = layout.panel.left() + InfoPadding * s;
    const qreal top = layout.panel.top() + (InfoPaddingY + InfoTitleHeight + SettingsTitleGap) * s;
    for (qreal column : std::as_const(widths)) {
        layout.columns << QRectF(x, top, column * s, rows * SettingsRowHeight * s);
        x += (column + SettingsColumnGap) * s;
    }
    const QRectF &first = layout.columns.first();
    layout.done = QRectF(first.left(), first.bottom() + SettingsDoneGap * s, layout.columns.last().right() - first.left(), SettingsDoneHeight * s);
    return layout;
}

QRectF SettingsOverlay::rowRect(const Layout &layout, int group, int row) const
{
    const QRectF column = layout.columns.at(group);
    const qreal rowHeight = SettingsRowHeight * layout.scale;
    return QRectF(column.left(), column.top() + row * rowHeight, column.width(), rowHeight);
}

bool SettingsOverlay::rowAt(const QPointF &pos, int &group, int &row) const
{
    const Layout layout = computeLayout();
    for (int g = 0; g < m_groups.size(); ++g) {
        for (int r = 0; r < m_groups[g].options.size(); ++r) {
            if (rowRect(layout, g, r).contains(pos)) {
                group = g;
                row = r;
                return true;
            }
        }
    }
    return false;
}

bool SettingsOverlay::event(QEvent *event)
{
    // Claim every key so window shortcuts (S, arrows, Enter, ...) don't fire
    if (event->type() == QEvent::ShortcutOverride) {
        event->accept();
        return true;
    }
    return QWidget::event(event);
}

bool SettingsOverlay::focusNextPrevChild(bool)
{
    return false; // Tab moves inside the panel, and reaches keyPressEvent only this way
}

void SettingsOverlay::keyPressEvent(QKeyEvent *event)
{
    const int rows = int(m_groups[m_group].options.size());
    switch (event->key()) {
    case Qt::Key_Up:
        if (m_doneSelected)
            select(m_group, rows - 1);
        else if (m_row == 0)
            selectDone();
        else
            select(m_group, m_row - 1);
        break;
    case Qt::Key_Down:
        if (m_doneSelected)
            select(m_group, 0);
        else if (m_row == rows - 1)
            selectDone();
        else
            select(m_group, m_row + 1);
        break;
    case Qt::Key_Left:
    case Qt::Key_Right: {
        if (m_doneSelected)
            break; // it spans the columns
        const int count = int(m_groups.size());
        const int group = (m_group + (event->key() == Qt::Key_Left ? -1 : 1) + count) % count;
        select(group, qMin(m_row, int(m_groups[group].options.size()) - 1));
        break;
    }
    case Qt::Key_Tab:
    case Qt::Key_Backtab: {
        // Every group, then Done
        const int stops = int(m_groups.size()) + 1;
        const int here = m_doneSelected ? stops - 1 : m_group;
        const int next = (here + (event->key() == Qt::Key_Backtab ? -1 : 1) + stops) % stops;
        if (next == stops - 1)
            selectDone();
        else
            select(next, qMin(m_row, int(m_groups[next].options.size()) - 1));
        break;
    }
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (m_doneSelected)
            close();
        else
            emit chosen(m_group, m_groups[m_group].options[m_row].value);
        break;
    case Qt::Key_S:
    case Qt::Key_Escape:
        close();
        break;
    default:
        break;
    }
}

void SettingsOverlay::mouseMoveEvent(QMouseEvent *event)
{
    int group = 0, row = 0;
    const bool overRow = rowAt(event->position(), group, row);
    const bool overDone = computeLayout().done.contains(event->position());
    setCursor(overRow || overDone ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (overDone && !m_doneSelected)
        selectDone();
    else if (overRow && (m_doneSelected || group != m_group || row != m_row))
        select(group, row);
}

void SettingsOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    int group = 0, row = 0;
    const Layout layout = computeLayout();
    if (rowAt(event->position(), group, row))
        emit chosen(group, m_groups[group].options[row].value);
    else if (layout.done.contains(event->position()) || !layout.panel.contains(event->position()))
        close();
}

void SettingsOverlay::paintEvent(QPaintEvent *)
{
    const Layout layout = computeLayout();
    const qreal s = layout.scale;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(BackdropColor);
    p.drawRoundedRect(layout.panel, 14 * s, 14 * s);

    for (int g = 0; g < m_groups.size(); ++g) {
        const Group &group = m_groups[g];
        const QRectF column = layout.columns.at(g);
        p.setFont(infoTitleFont(font(), s));
        drawShadowedText(p, QRectF(column.left(), column.top() - (InfoTitleHeight + SettingsTitleGap) * s, column.width(), InfoTitleHeight * s),
                         group.title, TextColor);

        p.setFont(infoLineFont(font(), s));
        for (int r = 0; r < group.options.size(); ++r) {
            const QRectF row = rowRect(layout, g, r);
            if (!m_doneSelected && g == m_group && r == m_row) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(255, 255, 255, 50));
                p.drawRoundedRect(row, 8 * s, 8 * s);
            }
            if (group.options[r].value == group.current) {
                p.setPen(Qt::NoPen);
                p.setBrush(DockPrimaryColor);
                p.drawEllipse(QPointF(row.left() + SettingsMarkerSlot / 2 * s, row.center().y()), 7 * s, 7 * s);
            }
            drawShadowedText(p, row.adjusted(SettingsMarkerSlot * s, 0, 0, 0), group.options[r].text, QColor(255, 255, 255, 215));
        }
    }

    p.setPen(m_doneSelected ? Qt::NoPen : QPen(QColor(255, 255, 255, 110), 1.5));
    p.setBrush(m_doneSelected ? TextColor : QColor(255, 255, 255, 22));
    p.drawRoundedRect(layout.done, layout.done.height() / 2, layout.done.height() / 2);
    p.setFont(pixelFont(font(), qMax(1, qRound(24 * s)), QFont::DemiBold));
    p.setPen(m_doneSelected ? QColor(18, 18, 20) : TextColor);
    p.drawText(layout.done, Qt::AlignCenter, tr("Done"));

    p.setFont(pixelFont(font(), qMax(1, qRound(15 * s))));
    p.setPen(DimTextColor);
    p.drawText(QRectF(layout.panel.left(), layout.panel.bottom() - SettingsFooterHeight * s, layout.panel.width(), SettingsFooterHeight * s),
               Qt::AlignCenter, tr("↑ ↓ ← →  choose   ·   Enter  set   ·   S / Esc  close"));
}


// ------------------------------------------------------------- AboutOverlay

AboutOverlay::AboutOverlay(QWidget *parent)
    : QWidget(parent)
{
    hide();
}

void AboutOverlay::mousePressEvent(QMouseEvent *)
{
    emit closeRequested();
}

void AboutOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), QColor(0, 0, 0, 170));

    // Drawn on a 600 x 560 grid at 1080 px window height, smaller in smaller windows
    const qreal s = qMin(osdScale(this), 0.9 * qMin(width() / 600.0, height() / 560.0));
    p.translate(rect().center() - QPointF(300 * s, 280 * s));
    p.scale(s, s);

    const QRectF card(0, 0, 600, 560);
    p.setPen(QPen(OutlineColor, 1 / s));
    p.setBrush(QColor(22, 22, 25, 235));
    p.drawRoundedRect(card, 22, 22);

    const QIcon icon = QApplication::windowIcon();
    if (!icon.isNull()) {
        const QPixmap pixmap = icon.pixmap(qRound(120 * s * devicePixelRatioF()));
        p.drawPixmap(QRectF(240, 36, 120, 120), pixmap, QRectF(pixmap.rect()));
    }

    const auto line = [&](qreal top, qreal height, const QString &text, const QFont &font, const QColor &color) {
        p.setFont(font);
        p.setPen(color);
        const QRectF r(30, top, 540, height);
        p.drawText(r, Qt::AlignCenter, p.fontMetrics().elidedText(text, Qt::ElideRight, int(r.width())));
    };
    line(170, 52, QStringLiteral("ZenViewer"), pixelFont(font(), 40, QFont::Bold), TextColor);
    line(224, 30, tr("Version %1").arg(QStringLiteral(ZENVIEWER_VERSION)), pixelFont(font(), 21), DimTextColor);
    line(256, 30, tr("A lightweight image viewer"), pixelFont(font(), 19), DimTextColor);

    line(316, 30,
         tr("© %1, since %2 and for all time")
             .arg(QStringLiteral(ZENVIEWER_AUTHOR), QStringLiteral(ZENVIEWER_COPYRIGHT_SINCE)),
         pixelFont(font(), 20, QFont::DemiBold), TextColor);
    line(348, 30, QStringLiteral(ZENVIEWER_CONTACT), pixelFont(font(), 19), TextColor);
    line(380, 28, tr("Free software under the MIT License"), pixelFont(font(), 17), DimTextColor);

    line(436, 26, tr("Qt %1 (LGPL v3)").arg(QString::fromLatin1(qVersion())), pixelFont(font(), 16), DimTextColor);
    line(462, 26,
         tr("%1 · %2").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture()),
         pixelFont(font(), 16), DimTextColor);

    line(510, 24, tr("A  or  Esc   close"), pixelFont(font(), 14), DisabledColor);
}

// ---------------------------------------------------------------- EmptyHint

EmptyHint::EmptyHint(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    resize(sizeHint());
}

QSize EmptyHint::sizeHint() const
{
    // 2.5 times the 360 x 200 drawing grid, so it reads from across the room
    const qreal s = 2.5 * osdScale(parentWidget());
    return {qRound(360 * s), qRound(200 * s)};
}

void EmptyHint::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(width() / 360.0, height() / 200.0); // drawn on a 360 x 200 grid
    const QColor color(255, 255, 255, m_hover ? 230 : 150);
    drawOsdIcon(p, OsdIcon::Open, QRectF((360 - 72) / 2.0, 20, 72, 72), color);

    p.setPen(color);
    p.setFont(pixelFont(font(), 13));
    p.drawText(QRect(0, 112, 360, 24), Qt::AlignCenter, tr("Press O or drop an image here"));
    p.setPen(QColor(255, 255, 255, 110));
    p.drawText(QRect(0, 138, 360, 24), Qt::AlignCenter, tr("H  ·  keys & mouse"));
}

void EmptyHint::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked();
}

void EmptyHint::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void EmptyHint::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}
