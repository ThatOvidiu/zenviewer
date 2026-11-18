#pragma once

#include "OsdIcons.h"

#include <QTimer>
#include <QWidget>

class QAbstractButton;
class QAction;
class QFileDialog;
class QVariantAnimation;

// Size of the on-screen display relative to its nominal size at a 1080 pixel
// high view; everything in it follows this, so it looks the same at any resolution.
qreal osdScale(const QWidget *view);
qreal osdScale(int height);
// How a shortcut is written on screen
QString keyLabel(const QKeySequence &sequence);

// Translucent overlay element. Opacity is driven by the OSD fade; at zero
// the widget hides so mouse events reach the image underneath.
class OsdWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OsdWidget(QWidget *parent);

    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal opacity);

protected:
    qreal m_opacity = 1.0;
};

// Name of the hovered dock button, shown beside it.
class DockHint : public OsdWidget
{
    Q_OBJECT

public:
    explicit DockHint(QWidget *parent);
    void setText(const QString &text);
    void setScale(qreal scale);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_text;
    qreal m_scale = 1.0;
};

// Column of icon buttons stuck to the left window edge, without a visible panel;
// each button triggers a QAction.
// Buttons near the cursor magnify smoothly and push their neighbours apart.
// The keyboard can drive it too: the selected button behaves as if hovered.
class ControlBar : public OsdWidget
{
    Q_OBJECT

public:
    explicit ControlBar(QWidget *parent);

    void addButton(QAction *action, OsdIcon icon, bool primary = false);
    void addSeparator();
    void setIcon(QAction *action, OsdIcon icon);
    // The hint of action names the first shortcut of keys, for a button with none of its own
    void setKeysFrom(QAction *action, QAction *keys);
    // Scales the whole dock so it keeps the same proportion of the window height
    void scaleToHeight(int windowHeight);
    // Selects the primary button, or the first enabled one if it is disabled
    void selectDefault();
    // Selects the next enabled button in the given direction, wrapping around;
    // with nothing selected yet, the default one
    void moveSelection(int step);
    void activateSelection();
    void endKeyboardNavigation();
    bool isKeyboardNavigating() const { return m_keyboard; }
    // Width of the button column at rest
    int panelWidth() const;
    // Includes room for the dock at full magnification
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    struct Item
    {
        QAction *action = nullptr; // nullptr for a separator
        OsdIcon icon = OsdIcon::None;
        bool primary = false;
        QRectF rest; // position when not magnified
        QRectF rect; // current position
        QAction *keysFrom = nullptr; // whose shortcut the hint names, if not the action's own
    };

    void relayout();
    void updateMagnification();
    void animateMagnification(qreal target);
    void updateHint();
    void repaintBehind(const QRegion &region);
    void select(int index);
    int itemAt(const QPointF &pos) const;
    void setHover(int index);

    qreal naturalHeight() const;

    QList<Item> m_items;
    DockHint *m_hint;
    QVariantAnimation *m_magnifyAnim;
    QVariantAnimation *m_followAnim; // glides the magnification to the selected button
    qreal m_scale = 1.0;
    qreal m_magnify = 0.0; // 0 at rest, 1 fully magnified
    qreal m_mouseY = 0.0;
    qreal m_restTop = 0.0;
    qreal m_restBottom = 0.0;
    qreal m_panelTop = 0.0;
    qreal m_panelBottom = 0.0;
    int m_hover = -1;
    int m_pressed = -1;
    bool m_keyboard = false;
};

// Round single-icon button (side navigation arrows). Invisible while its
// action is disabled.
class OsdButton : public OsdWidget
{
    Q_OBJECT

public:
    OsdButton(QAction *action, OsdIcon icon, QWidget *parent);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QAction *m_action;
    OsdIcon m_icon;
    bool m_hover = false;
};

// Gradient along the top edge of the image with file name and position.
class TopBar : public OsdWidget
{
    Q_OBJECT

public:
    explicit TopBar(QWidget *parent);
    void setTexts(const QString &title, const QString &counter);
    // Keeps the text clear of what covers the left part of the bar
    void setTextIndent(int indent);
    void setScale(qreal scale);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
    QString m_counter;
    int m_textIndent = 30;
    qreal m_scale = 1.0;
};

// Large title plus metadata lines on a translucent black backdrop, anchored
// to the image's top-left corner.
class InfoPanel : public OsdWidget
{
    Q_OBJECT

public:
    explicit InfoPanel(QWidget *parent);
    void setContent(const QString &title, const QStringList &lines);
    void setMaximumTextWidth(int width);
    void setScale(qreal scale);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
    QStringList m_lines;
    int m_maxWidth = 900;
    qreal m_scale = 1.0;
};

// Short-lived message in large type (zoom level, effect name, ...), meant for the
// centre of the image.
class OsdToast : public OsdWidget
{
    Q_OBJECT

public:
    explicit OsdToast(QWidget *parent);
    void showMessage(const QString &text, OsdIcon icon, int durationMs);
    void setScale(qreal scale);
    // A longer message shrinks to fit, and past a point gets elided
    void setMaximumTextWidth(int width);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QFont textFont(qreal scale) const;
    qreal naturalWidth(qreal scale) const;
    qreal fittedScale() const;
    qreal textLeft(qreal scale) const;
    // The message as it fits, in the room left beside the icon. May reach into the
    // right padding: font metrics round, and that mustn't cut a message that fits.
    QRectF textRect(qreal scale) const;
    QString visibleText(qreal scale) const;

    QString m_text;
    OsdIcon m_icon = OsdIcon::None;
    qreal m_scale = 1.0;
    int m_maxWidth = 1800;
    QTimer m_timer;
    QVariantAnimation *m_fade;
};

// Two blinking bars on a translucent black square, shown while the
// slideshow is paused.
class PauseIndicator : public QWidget
{
    Q_OBJECT

public:
    explicit PauseIndicator(QWidget *parent);
    void setActive(bool active);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVariantAnimation *m_blink;
    qreal m_level = 1.0;
};

// Big icon in the middle of the view that fades in and out very quickly to
// acknowledge play or pause.
class ActionFlash : public QWidget
{
    Q_OBJECT

public:
    explicit ActionFlash(QWidget *parent);
    void flash(OsdIcon icon);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVariantAnimation *m_fade;
    OsdIcon m_icon = OsdIcon::Play;
    qreal m_level = 0.0;
};

// Shown while the image on screen is still being decoded. Waits a moment
// before appearing, so images that load quickly don't make it flash.
class LoadingSpinner : public QWidget
{
    Q_OBJECT

public:
    explicit LoadingSpinner(QWidget *parent);
    void setActive(bool active);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QTimer m_delay;
    QTimer m_tick;
};

// Full-window key/mouse reference.
class HelpOverlay : public QWidget
{
    Q_OBJECT

public:
    struct Entry
    {
        QStringList keys;
        OsdIcon icon = OsdIcon::None;
        QString text;
    };

    explicit HelpOverlay(QWidget *parent);
    void setEntries(const QList<Entry> &keyboard, const QList<Entry> &mouse);
    // The card and its text stay right of this, however large they get
    void setReservedLeft(int width);
    // The same reference as text, for the command line
    QString plainText() const;

signals:
    void closeRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Column
    {
        OsdIcon header = OsdIcon::None;
        QList<Entry> entries;
        qreal keysWidth = 0;
        qreal textWidth = 0;
    };
    struct Metrics
    {
        qreal scale = 1.0;
        QList<Column> columns;
        qsizetype mouseStart = 0; // first of the mouse columns, after the keyboard ones
        QSizeF card;
    };

    QRectF availableRect() const;
    void addColumns(Metrics &metrics, OsdIcon header, const QList<Entry> &entries, int maxRows) const;
    bool computeMetrics(qreal scale, Metrics &metrics) const;
    void updateMetrics();
    void paintColumn(QPainter &p, const QRectF &rect, const Column &column) const;

    QList<Entry> m_keyboard;
    QList<Entry> m_mouse;
    Metrics m_metrics;
    int m_reservedLeft = 0;
};

// File browser inside the window rather than a separate one that a full-screen
// viewer could cover. Takes an image or a whole folder. While shown it swallows
// every shortcut; clicking outside the browser cancels.
class OpenOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit OpenOverlay(QWidget *parent);
    void ask(const QString &title, const QString &startDir, const QString &nameFilters);

signals:
    void chosen(const QString &path);
    void closed();

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void placeDialog();
    void fitSidebar();
    void applyColorScheme();

    QFileDialog *m_dialog = nullptr;
    QAbstractButton *m_close = nullptr; // the round × above the dialog's top-right corner
};

// Full-window yes/no question with a large prompt. Defaults to No; works with
// the keyboard (←/→/Tab, Enter/Space, Y/N, Esc) and the mouse. While shown it
// takes focus and swallows every shortcut.
class ConfirmOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit ConfirmOverlay(QWidget *parent);
    void ask(const QString &question, const QString &yesText, const QString &noText);

signals:
    void accepted();
    void rejected();

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool focusNextPrevChild(bool next) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRectF cardRect() const;
    QRectF buttonRect(bool yes) const;
    void finish(bool yes);

    QString m_question;
    QString m_yesText;
    QString m_noText;
    bool m_yesSelected = false;
};

// Choices in side-by-side groups, in the type and on the backdrop of the info
// panel, centred over the image, with a Done button under all of them. A choice
// applies at once and the panel stays up. Works with the keyboard (arrows, Enter,
// and S or Esc to close; past either end of any group is Done) and the mouse
// (clicking outside closes). While shown it takes focus and swallows every shortcut.
class SettingsOverlay : public QWidget
{
    Q_OBJECT

public:
    struct Option
    {
        QString text;
        int value = 0;
    };
    struct Group
    {
        QString title;
        QList<Option> options;
        int current = 0; // value of the chosen option
    };

    explicit SettingsOverlay(QWidget *parent);
    void setGroups(const QList<Group> &groups);
    void setCurrent(int group, int value);
    void open();

signals:
    void chosen(int group, int value);
    void closed();

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool focusNextPrevChild(bool next) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    struct Layout
    {
        qreal scale = 1.0;
        QRectF panel;
        QList<QRectF> columns; // below the group titles
        QRectF done;
    };

    Layout computeLayout() const;
    QRectF rowRect(const Layout &layout, int group, int row) const;
    bool rowAt(const QPointF &pos, int &group, int &row) const;
    void select(int group, int row);
    void selectDone();
    void close();

    QList<Group> m_groups;
    int m_group = 0;
    int m_row = 0;
    bool m_doneSelected = false;
};

// Help > About: the app's name, icon, version, author and licence on a card.
// Any click closes it.
class AboutOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit AboutOverlay(QWidget *parent);

signals:
    void closeRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
};

// Shown when nothing is open; click to open a file.
class EmptyHint : public QWidget
{
    Q_OBJECT

public:
    explicit EmptyHint(QWidget *parent);
    QSize sizeHint() const override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    bool m_hover = false;
};
