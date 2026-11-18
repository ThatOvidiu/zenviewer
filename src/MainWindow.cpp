#include "MainWindow.h"

#include "Filmstrip.h"
#include "ImageLoader.h"
#include "ImageView.h"
#include "OsdWidgets.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QImageReader>
#include <QKeyEvent>
#include <QLocale>
#include <QMimeData>
#include <QSettings>
#include <QTimer>
#include <QVariantAnimation>

#include <algorithm>

namespace {
const QColor ViewBackground(16, 16, 16);
constexpr int OsdHideDelayMs = 1900;
constexpr int StripKeyHoldMs = 1200; // how long the strip stays after the last left / right press
constexpr int OsdMargin = 26;
constexpr int TopBarHeight = 110;
constexpr qreal MinImageTextScale = 0.5; // keeps the file details readable in a very small window
constexpr qreal DetailTextShare = 0.75; // file name and info panel, relative to the other image text
const QList<int> SlideIntervalsMs = {1000, 1500, 2000, 5000, 10000, 20000};

// The listed interval closest to ms, so a value saved by an older version still fits
int nearestSlideInterval(int ms)
{
    return *std::min_element(SlideIntervalsMs.begin(), SlideIntervalsMs.end(),
                             [ms](int a, int b) { return qAbs(a - ms) < qAbs(b - ms); });
}

// "1.5", "2"…, with the decimal point of the user's language
QString secondsText(int ms)
{
    return QLocale().toString(ms / 1000.0, 'g', 4);
}

enum { SettingsIntervalGroup, SettingsEffectGroup };

// Case and accents don't count when sorting
QString sortKey(const QString &name)
{
    QString key;
    for (const QChar c : name.normalized(QString::NormalizationForm_D))
        if (c.category() != QChar::Mark_NonSpacing)
            key += c.toCaseFolded();
    return key;
}

// Runs of digits compare as whole numbers: img2 before img10
bool naturalLess(const QString &a, const QString &b)
{
    const auto isDigit = [](QChar c) { return c >= u'0' && c <= u'9'; };
    qsizetype i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (!isDigit(a[i]) || !isDigit(b[j])) {
            if (a[i] != b[j])
                return a[i] < b[j];
            ++i, ++j;
            continue;
        }
        // Of two numbers without leading zeros, the longer one is larger
        while (i < a.size() && a[i] == u'0')
            ++i;
        while (j < b.size() && b[j] == u'0')
            ++j;
        const qsizetype startA = i, startB = j;
        while (i < a.size() && isDigit(a[i]))
            ++i;
        while (j < b.size() && isDigit(b[j]))
            ++j;
        if (i - startA != j - startB)
            return i - startA < j - startB;
        const int order = QStringView(a).sliced(startA, i - startA).compare(QStringView(b).sliced(startB, j - startB));
        if (order != 0)
            return order < 0;
    }
    return a.size() - i < b.size() - j;
}

QStringList imageNameFilters()
{
    QStringList filters;
    for (const QByteArray &format : QImageReader::supportedImageFormats())
        if (imageFormats().contains(format))
            filters << QStringLiteral("*.") + QString::fromLatin1(format);
    return filters;
}

void fadeTo(QVariantAnimation *fade, qreal from, bool shown)
{
    fade->stop();
    {
        // A finished animation reports its new end value right away, which would flash the widget
        const QSignalBlocker blocker(fade);
        fade->setDuration(shown ? 150 : 350);
        fade->setStartValue(from);
        fade->setEndValue(shown ? 1.0 : 0.0);
    }
    fade->start();
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_view(new ImageView(this))
    , m_loader(new ImageLoader(this))
    , m_transition(new TransitionOverlay(m_view->viewport()))
    , m_osdFade(new QVariantAnimation(this))
    , m_osdHideTimer(new QTimer(this))
    , m_barFade(new QVariantAnimation(this))
    , m_stripFade(new QVariantAnimation(this))
    , m_stripKeyTimer(new QTimer(this))
    , m_slideTimer(new QTimer(this))
{
    QSettings settings;
    m_slideIntervalMs = nearestSlideInterval(settings.value("slideshow/interval", m_slideIntervalMs).toInt());
    const auto effect = TransitionOverlay::Effect(settings.value("slideshow/effect", int(m_effect)).toInt());
    if (TransitionOverlay::effects().contains(effect))
        m_effect = effect;

    setCentralWidget(m_view);
    setMinimumSize(640, 420);
    m_view->setBackgroundBrush(ViewBackground);
    m_view->viewport()->setMouseTracking(true);

    createActions();
    createOsd();

    m_slideTimer->setSingleShot(true);
    connect(m_slideTimer, &QTimer::timeout, this, &MainWindow::advanceSlideshow);
    connect(m_transition, &TransitionOverlay::finished, this, &MainWindow::scheduleNextSlide);

    m_osdHideTimer->setSingleShot(true);
    m_osdHideTimer->setInterval(OsdHideDelayMs);
    connect(m_osdHideTimer, &QTimer::timeout, this, [this] { hideOsd(); });
    connect(m_osdFade, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        applyOsdOpacity(v.toReal());
    });
    m_stripKeyTimer->setSingleShot(true);
    m_stripKeyTimer->setInterval(StripKeyHoldMs);
    connect(m_stripKeyTimer, &QTimer::timeout, this, &MainWindow::updatePanels);
    connect(m_barFade, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_barOpacity = v.toReal();
        m_controlBar->setOpacity(m_barOpacity);
        applyArrowOpacity();
    });
    connect(m_stripFade, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_stripOpacity = v.toReal();
        applyStripOpacity();
    });

    connect(m_filmstrip, &Filmstrip::activated, this, &MainWindow::showIndex);
    connect(m_loader, &ImageLoader::loaded, this, &MainWindow::onImageLoaded);
    connect(m_loader, &ImageLoader::fullLoaded, this, [this](const QString &path, const QImage &image) {
        if (path != currentPath())
            return;
        m_fullImagePending = false;
        updateSpinner();
        if (!image.isNull())
            m_view->setFullImage(image);
    });
    connect(m_view, &ImageView::detailNeeded, this, [this] {
        m_fullImagePending = m_loader->requestFull(currentPath());
        updateSpinner();
    });
    connect(m_view, &ImageView::zoomChanged, this, [this](qreal zoom) {
        toast(QStringLiteral("%1%").arg(qRound(zoom * 100)), OsdIcon::ZoomIn, 900);
    });
    connect(m_view, &ImageView::doubleClicked, this, &MainWindow::toggleFullScreen);
    connect(m_view, &ImageView::navigateRequested, this, [this](int step) {
        m_stripKeyTimer->start(); // same as the left / right keys
        navigate(step);
        updatePanels();
    });
    connect(m_view, &ImageView::imageRectChanged, this, &MainWindow::layoutOsd);

    // Drops land on the window, not the graphics view
    m_view->setAcceptDrops(false);
    m_view->viewport()->setAcceptDrops(false);
    setAcceptDrops(true);

    // Any mouse activity inside the window brings the OSD back
    qApp->installEventFilter(this);

    if (!restoreGeometry(settings.value("window/geometry").toByteArray()))
        resize(1100, 750);

    updateActions();
    updatePlaybackUi();
    updateCursor();
}

void MainWindow::createActions()
{
    auto add = [this](const QString &text, const QList<QKeySequence> &keys, auto slot) {
        auto *action = new QAction(text, this);
        action->setShortcuts(keys);
        connect(action, &QAction::triggered, this, slot);
        addAction(action); // window-wide shortcuts
        return action;
    };
    auto help = [this](QAction *action, OsdIcon icon = OsdIcon::None) {
        m_helpActions.append({action, icon});
        return action;
    };

    // The bar's play button does both: start it, then pause it. On the keyboard they are two keys.
    m_playAct = add(tr("Start slideshow"), {}, [this] { playPause(); });
    m_slideshowAct = help(add(tr("Start / stop slideshow"), {Qt::Key_P, Qt::Key_F5}, [this] {
        m_slideshow ? stopSlideshow() : playPause();
    }), OsdIcon::Play);
    m_pauseAct = help(add(tr("Pause / resume slideshow"), {Qt::Key_Space}, [this] {
        if (m_slideshow)
            playPause();
    }), OsdIcon::Pause);
    m_stopAct = add(tr("Stop slideshow"), {}, [this] { stopSlideshow(); });
    m_prevAct = help(add(tr("Previous image"), {Qt::Key_Left, Qt::Key_PageUp}, [this] { navigate(-1); }),
                     OsdIcon::Previous);
    m_nextAct = help(add(tr("Next image"), {Qt::Key_Right, Qt::Key_PageDown}, [this] { navigate(+1); }),
                     OsdIcon::Next);
    help(add(tr("First image"), {Qt::Key_Home}, [this] { showIndex(0); }));
    help(add(tr("Last image"), {Qt::Key_End}, [this] { showIndex(m_files.size() - 1); }));
    help(add(tr("Shorter slide interval"), {Qt::Key_BracketLeft}, [this] { stepSlideInterval(-1); }));
    help(add(tr("Longer slide interval"), {Qt::Key_BracketRight}, [this] { stepSlideInterval(+1); }));
    auto *settingsAct = help(add(tr("Slideshow settings"), {Qt::Key_S}, [this] {
        setHelpVisible(false);
        m_settings->open();
        showOsd(); // keeps the cursor visible
    }), OsdIcon::Settings);

    m_imageActions = {
        help(add(tr("Zoom in"), {Qt::Key_Plus}, [this] { m_view->zoomIn(); }), OsdIcon::ZoomIn),
        help(add(tr("Zoom out"), {Qt::Key_Minus}, [this] { m_view->zoomOut(); }), OsdIcon::ZoomOut),
        help(add(tr("Fit to window"), {Qt::Key_0}, [this] { m_view->fitToWindow(); }), OsdIcon::FitWindow),
        help(add(tr("Actual size / fit"), {Qt::Key_1}, [this] { m_view->toggleActualSize(); }), OsdIcon::ActualSize),
        add(tr("Trash (press twice)"), {QKeySequence::Delete}, [this] { deleteCurrent(); }),
    };
    QAction *zoomInAct = m_imageActions[0], *zoomOutAct = m_imageActions[1], *fitAct = m_imageActions[2],
            *actualAct = m_imageActions[3];

    m_infoAct = help(add(tr("Info panel"), {Qt::Key_I}, [] {}), OsdIcon::Info);
    m_infoAct->setCheckable(true);
    m_filmstripAct = help(add(tr("Thumbnails on / off"), {Qt::Key_T}, [] {}), OsdIcon::Filmstrip);
    m_filmstripAct->setCheckable(true);
    m_filmstripAct->setChecked(QSettings().value("osd/filmstrip", true).toBool());
    m_loopAct = help(add(tr("Loop: last image leads to first"), {Qt::Key_L}, [] {}));
    m_loopAct->setCheckable(true);
    m_loopAct->setChecked(QSettings().value("view/loop", false).toBool());
    connect(m_loopAct, &QAction::toggled, this, [this](bool on) {
        QSettings().setValue("view/loop", on);
        toast(on ? tr("Loop on") : tr("Loop off"), OsdIcon::None);
        updateActions();
    });
    // Only for now: the left edge always brings it back, so it can't be left off by accident
    help(add(tr("Show / hide icon bar"), {Qt::Key_M}, [this] {
        if (cardOpen())
            return;
        if (m_barShown) {
            m_controlBar->endKeyboardNavigation();
            m_revealBar = m_mouseAtLeft = false;
        } else {
            m_controlBar->selectDefault();
        }
        updatePanels();
    }));
    const auto selectInDock = [this](int step) {
        if (!cardOpen()) {
            m_controlBar->moveSelection(step);
            updatePanels();
        }
    };
    help(add(tr("Select in icon bar"), {Qt::Key_Up}, [selectInDock] { selectInDock(-1); }));
    add(tr("Select next in icon bar"), {Qt::Key_Down}, [selectInDock] { selectInDock(+1); });
    help(add(tr("Press icon · show / hide controls"), {Qt::Key_Return, Qt::Key_Enter}, [this] {
        if (m_controlBar->isKeyboardNavigating())
            m_controlBar->activateSelection();
        else if (m_revealBar || m_revealStrip)
            hideOsd(true);
        else {
            m_revealBar = m_revealStrip = true;
            showOsd();
        }
    }));
    m_fullScreenAct = help(add(tr("Full screen"), {Qt::Key_F}, [this] { toggleFullScreen(); }),
                           OsdIcon::Fullscreen);
    m_openAct = help(add(tr("Open image or folder"), {Qt::Key_O}, [this] { openFileDialog(); }), OsdIcon::Open);
    auto *helpAct = help(add(tr("Help"), {Qt::Key_H, Qt::Key_F1}, [this] { setHelpVisible(!m_help->isVisible()); }),
                         OsdIcon::Help);
    m_aboutAct = help(add(tr("About"), {Qt::Key_A}, [this] { setAboutVisible(!m_about->isVisible()); }),
                      OsdIcon::More);
    help(add(tr("Close · stop · exit full screen"), {Qt::Key_Escape}, [this] {
        if (m_about->isVisible())
            setAboutVisible(false);
        else if (m_help->isVisible())
            setHelpVisible(false);
        else if (m_controlBar->isKeyboardNavigating()) {
            m_controlBar->endKeyboardNavigation();
            updatePanels();
        } else if (m_slideshow)
            stopSlideshow();
        else if (isFullScreen())
            toggleFullScreen();
    }));
    help(add(tr("Quit (asks first)"), {Qt::Key_Q, Qt::Key_X},
             [this] { confirmQuit(); }));

    m_controlBar = new ControlBar(m_view);
    m_controlBar->addButton(m_openAct, OsdIcon::Open);
    m_controlBar->addSeparator();
    m_controlBar->addButton(m_prevAct, OsdIcon::Previous);
    m_controlBar->addButton(m_playAct, OsdIcon::Play, true);
    m_controlBar->addButton(m_nextAct, OsdIcon::Next);
    m_controlBar->addButton(m_stopAct, OsdIcon::Stop);
    m_controlBar->addSeparator();
    m_controlBar->addButton(zoomInAct, OsdIcon::ZoomIn);
    m_controlBar->addButton(zoomOutAct, OsdIcon::ZoomOut);
    m_controlBar->addButton(actualAct, OsdIcon::ActualSize);
    m_controlBar->addButton(fitAct, OsdIcon::FitWindow);
    m_controlBar->addSeparator();
    m_controlBar->addButton(m_infoAct, OsdIcon::Info);
    m_controlBar->addButton(m_fullScreenAct, OsdIcon::Fullscreen);
    m_controlBar->addButton(settingsAct, OsdIcon::Settings);
    m_controlBar->addButton(m_filmstripAct, OsdIcon::Filmstrip);
    m_controlBar->addButton(helpAct, OsdIcon::Help);
    m_controlBar->addSeparator();
    m_controlBar->addButton(m_aboutAct, OsdIcon::More);
    m_controlBar->setKeysFrom(m_stopAct, m_slideshowAct);
}

void MainWindow::createOsd()
{
    m_topBar = new TopBar(m_view);

    m_filmstrip = new Filmstrip(m_view);

    m_prevButton = new OsdButton(m_prevAct, OsdIcon::ChevronLeft, m_view);
    m_nextButton = new OsdButton(m_nextAct, OsdIcon::ChevronRight, m_view);

    m_infoPanel = new InfoPanel(m_view);
    m_infoPanel->hide();
    m_controlBar->raise();
    connect(m_infoAct, &QAction::toggled, this, [this](bool on) {
        m_infoPanel->setVisible(on);
        applyOsdOpacity(m_osdOpacity); // the info panel replaces the top bar title
        layoutOsd();
    });
    connect(m_filmstripAct, &QAction::toggled, this, [this](bool on) {
        QSettings().setValue("osd/filmstrip", on);
        toast(on ? tr("Thumbnails on") : tr("Thumbnails off"));
        applyStripOpacity();
        m_revealStrip = on;
        showOsd();
    });

    m_spinner = new LoadingSpinner(m_view);
    m_flash = new ActionFlash(m_view);
    m_pauseIndicator = new PauseIndicator(m_view);
    m_toast = new OsdToast(m_view);

    m_emptyHint = new EmptyHint(m_view);
    connect(m_emptyHint, &EmptyHint::clicked, this, &MainWindow::openFileDialog);

    m_open = new OpenOverlay(m_view);
    connect(m_open, &OpenOverlay::chosen, this, &MainWindow::openPath);
    connect(m_open, &OpenOverlay::closed, this, [this] {
        m_view->setFocus();
        showOsd(); // restart the hide timer
    });

    m_settings = new SettingsOverlay(m_view);
    SettingsOverlay::Group intervals{tr("Interval"), {}, m_slideIntervalMs};
    for (int ms : SlideIntervalsMs)
        intervals.options.append({ms == 1000 ? tr("1 second") : tr("%1 seconds").arg(secondsText(ms)), ms});
    SettingsOverlay::Group effects{tr("Transition"), {}, int(m_effect)};
    for (TransitionOverlay::Effect effect : TransitionOverlay::effects())
        effects.options.append({TransitionOverlay::effectName(effect), int(effect)});
    m_settings->setGroups({intervals, effects});
    connect(m_settings, &SettingsOverlay::chosen, this, [this](int group, int value) {
        if (group == SettingsIntervalGroup)
            setSlideInterval(value);
        else
            setSlideEffect(TransitionOverlay::Effect(value));
    });
    connect(m_settings, &SettingsOverlay::closed, this, [this] {
        m_view->setFocus();
        showOsd(); // restart the hide timer
    });

    m_help = new HelpOverlay(m_view);
    connect(m_help, &HelpOverlay::closeRequested, this, [this] { setHelpVisible(false); });

    m_about = new AboutOverlay(m_view);
    connect(m_about, &AboutOverlay::closeRequested, this, [this] { setAboutVisible(false); });

    m_confirm = new ConfirmOverlay(m_view);
    connect(m_confirm, &ConfirmOverlay::accepted, this, &QWidget::close);
    connect(m_confirm, &ConfirmOverlay::rejected, this, [this] {
        m_view->setFocus();
        showOsd(); // restart the hide timer
    });

    QList<HelpOverlay::Entry> keyboard;
    for (const auto &[action, icon] : std::as_const(m_helpActions)) {
        QStringList keys;
        for (const QKeySequence &key : action->shortcuts()) {
            // Page Up / Down still turn pages (presentation clickers send them), but the arrows say it
            if (key == QKeySequence(Qt::Key_PageUp) || key == QKeySequence(Qt::Key_PageDown))
                continue;
            if (!keys.contains(keyLabel(key)))
                keys << keyLabel(key);
        }
        if (action->shortcut() == QKeySequence(Qt::Key_Return))
            keys = {QStringLiteral("Enter")}; // the main key and the keypad one, under its usual name
        if (action->shortcut() == QKeySequence(Qt::Key_Up))
            keys = {keyLabel(Qt::Key_Up), keyLabel(Qt::Key_Down)};
        keyboard.append({keys, icon, action->text()});
    }
    const QList<HelpOverlay::Entry> mouse = {
        {{tr("Move")}, OsdIcon::None, tr("Show title and arrows")},
        {{tr("Left edge")}, OsdIcon::None, tr("Show icon bar")},
        {{tr("Bottom edge")}, OsdIcon::Filmstrip, tr("Show thumbnails")},
        {{tr("Wheel")}, OsdIcon::Next, tr("Previous / next image")},
        {{tr("Ctrl+Wheel")}, OsdIcon::ZoomIn, tr("Zoom at cursor")},
        {{tr("Drag")}, OsdIcon::None, tr("Pan the image")},
        {{tr("Double-click")}, OsdIcon::Fullscreen, tr("Full screen")},
        {{tr("Middle-click")}, OsdIcon::ActualSize, tr("Fit / actual size")},
        {{tr("Back"), tr("Forward")}, OsdIcon::Next, tr("Previous / next image")},
        {{tr("Thumbnail")}, OsdIcon::Filmstrip, tr("Jump to image")},
        {{tr("Edge arrows")}, OsdIcon::ChevronRight, tr("Previous / next image")},
        {{tr("Drop file")}, OsdIcon::Open, tr("Open it")},
    };
    m_help->setEntries(keyboard, mouse);

    applyOsdOpacity(1.0);
    applyStripOpacity();
}

qreal MainWindow::imageTextScale() const
{
    const qreal fullScreenScale = osdScale(screen()->size().height());
    if (isFullScreen())
        return fullScreenScale;
    // Follows the fitted image's shorter side, as a share of the screen. So a landscape and a
    // portrait picture get about the same text in the same window, and zooming doesn't change it.
    const QSizeF image = m_view->fittedSize();
    const qreal share = qMin(image.width(), image.height()) / screen()->size().height();
    return qMax(MinImageTextScale, fullScreenScale * qMin(share, 1.0));
}

void MainWindow::layoutOsd()
{
    if (!m_controlBar)
        return;
    const QRect r = m_view->rect();


    const qreal scale = osdScale(m_view);
    const int margin = qRound(OsdMargin * scale);

    m_controlBar->scaleToHeight(r.height());
    const QSize bar = m_controlBar->sizeHint();
    m_controlBar->setGeometry(0, (r.height() - bar.height()) / 2, bar.width(), bar.height());

    // Equal insets keep the strip centred in the window, clear of the dock
    const int dock = m_controlBar->panelWidth() + margin;
    m_filmstrip->setScale(scale);
    m_filmstrip->setGeometry(dock, r.height() - m_filmstrip->height(), r.width() - 2 * dock, m_filmstrip->height());

    const QSize nav = m_prevButton->sizeHint();
    const int navY = (r.height() - nav.height()) / 2;
    m_prevButton->setGeometry(qMax(dock, margin), navY, nav.width(), nav.height());
    m_nextButton->setGeometry(r.width() - margin - nav.width(), navY, nav.width(), nav.height());

    // Top bar, info and pause indicator stick to the image itself, not the window
    const QRect image = m_view->hasImage() ? m_view->imageRect() : r;
    const qreal imageScale = imageTextScale();
    // File name and info panel at three quarters of that, but never below the readable minimum
    const qreal detailScale = qMax(MinImageTextScale, DetailTextShare * imageScale);
    m_topBar->setScale(detailScale);
    m_topBar->setGeometry(image.left(), image.top(), image.width(), qRound(TopBarHeight * detailScale));
    m_topBar->setTextIndent(dock - image.left()); // wide images run under the dock
    m_infoPanel->setScale(detailScale);
    m_infoPanel->setMaximumTextWidth(qMax(qRound(200 * detailScale), image.width() - qRound(120 * detailScale)));
    QSize info = m_infoPanel->sizeHint();
    info.setWidth(qMin(info.width(), qMax(qRound(200 * detailScale), image.width())));
    m_infoPanel->setGeometry(QRect(image.topLeft(), info));
    const int pause = qRound(76 * scale);
    m_pauseIndicator->setGeometry(image.right() - margin - pause, image.top() + margin, pause, pause);
    m_toast->setScale(imageScale);
    m_toast->setMaximumTextWidth(r.width() - 2 * margin);
    const QSize toast = m_toast->sizeHint();
    const QPoint toastTopLeft = image.center() - QPoint(toast.width(), toast.height()) / 2;
    m_toast->setGeometry(QRect(QPoint(qBound(margin, toastTopLeft.x(), r.width() - margin - toast.width()), toastTopLeft.y()), toast));

    const int spinner = qRound(72 * scale);
    m_spinner->setGeometry((r.width() - spinner) / 2, (r.height() - spinner) / 2, spinner, spinner);
    const int flash = qRound(180 * scale);
    m_flash->setGeometry((r.width() - flash) / 2, (r.height() - flash) / 2, flash, flash);

    const QSize hint = m_emptyHint->sizeHint();
    m_emptyHint->setGeometry(QRect(QPoint((r.width() - hint.width()) / 2, (r.height() - hint.height()) / 2), hint));
    m_help->setGeometry(r);
    m_about->setGeometry(r);
    m_help->setReservedLeft(0); // the icon bar hides while the help is up
    m_open->setGeometry(r);
    m_settings->setGeometry(r);
    m_confirm->setGeometry(r);
}

void MainWindow::showOsd()
{
    m_osdHideTimer->start();
    updatePanels();
    if (m_osdVisible)
        return;
    m_osdVisible = true;
    fadeTo(m_osdFade, m_osdOpacity, true);
    updateCursor();
}

void MainWindow::hideOsd(bool force)
{
    if (!m_view->hasImage())
        return; // keep the controls up while there is nothing to look at
    if (!force && (osdHovered() || cardOpen() || m_confirm->isVisible() || m_open->isVisible()
                   || m_settings->isVisible())) {
        m_osdHideTimer->start();
        return;
    }
    m_osdHideTimer->stop();
    m_revealBar = m_revealStrip = false;
    if (force) { // gone even with the cursor resting on them, until it moves again
        m_controlBar->endKeyboardNavigation();
        m_mouseAtLeft = m_mouseAtBottom = false;
        m_stripKeyTimer->stop();
    }
    updatePanels();
    if (!m_osdVisible)
        return;
    m_osdVisible = false;
    fadeTo(m_osdFade, m_osdOpacity, false);
    updateCursor();
}

void MainWindow::applyOsdOpacity(qreal opacity)
{
    m_osdOpacity = opacity;
    applyArrowOpacity();
    m_topBar->setOpacity(m_infoAct->isChecked() ? 0.0 : opacity);
}

void MainWindow::applyArrowOpacity()
{
    // The arrows step aside while the icon bar is in use, and fade in again with it gone
    const qreal opacity = qMin(m_osdOpacity, 1.0 - m_barOpacity);
    for (OsdWidget *widget : std::initializer_list<OsdWidget *>{m_prevButton, m_nextButton})
        widget->setOpacity(opacity);
}

void MainWindow::applyStripOpacity()
{
    m_filmstrip->setOpacity(m_stripOpacity);
    m_filmstrip->setVisible(m_stripOpacity > 0.01 && m_filmstripAct->isChecked() && m_files.size() > 1);
}

void MainWindow::updatePanels()
{
    const bool nothingToSee = !m_view->hasImage();
    const bool mouseFree = !overlayOpen();
    // The help and About cards have the screen to themselves
    const bool bar = !cardOpen()
        && (nothingToSee || m_controlBar->isKeyboardNavigating() || m_revealBar || (m_mouseAtLeft && mouseFree));
    const bool strip = m_filmstripAct->isChecked()
        && (nothingToSee || m_revealStrip || m_stripKeyTimer->isActive() || (m_mouseAtBottom && mouseFree));
    if (bar != m_barShown)
        fadeTo(m_barFade, m_barOpacity, m_barShown = bar);
    if (strip != m_stripShown)
        fadeTo(m_stripFade, m_stripOpacity, m_stripShown = strip);
}

bool MainWindow::cardOpen() const
{
    return m_help->isVisible() || m_about->isVisible();
}

void MainWindow::setAboutVisible(bool visible)
{
    m_about->setVisible(visible);
    if (visible) {
        m_help->hide();
        m_about->raise();
        m_controlBar->endKeyboardNavigation();
        m_revealBar = false;
        showOsd();
    }
    updatePanels();
}

bool MainWindow::overlayOpen() const
{
    return m_open->isVisible() || m_settings->isVisible() || m_confirm->isVisible();
}

bool MainWindow::osdHovered() const
{
    return m_mouseAtLeft || m_mouseAtBottom || m_controlBar->isKeyboardNavigating() || m_prevButton->underMouse()
        || m_nextButton->underMouse();
}

void MainWindow::updateCursor()
{
    QWidget *viewport = m_view->viewport();
    if (!m_osdVisible && m_view->hasImage())
        viewport->setCursor(Qt::BlankCursor);
    else if (m_slideshow)
        viewport->setCursor(Qt::ArrowCursor);
    else
        viewport->setCursor(Qt::OpenHandCursor);
}

void MainWindow::toast(const QString &text, OsdIcon icon, int durationMs)
{
    m_toast->showMessage(text, icon, durationMs);
    layoutOsd();
}

void MainWindow::setHelpVisible(bool visible)
{
    m_help->setVisible(visible);
    if (visible) {
        m_about->hide();
        m_help->raise();
        m_controlBar->endKeyboardNavigation();
        m_revealBar = false;
        showOsd();
    }
    updatePanels();
}

void MainWindow::openPath(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        toast(tr("Not found: %1").arg(path), OsdIcon::Open, 3000);
        return;
    }
    if (info.isDir())
        loadFolder(info.absoluteFilePath(), {});
    else
        loadFolder(info.absolutePath(), info.absoluteFilePath());
}

void MainWindow::loadFolder(const QString &dir, const QString &selectFile)
{
    QList<std::pair<QString, QString>> sorted; // sort key, path
    const QFileInfoList entries = QDir(dir).entryInfoList(imageNameFilters(), QDir::Files | QDir::Readable);
    for (const QFileInfo &entry : entries)
        sorted.append({sortKey(entry.fileName()), entry.absoluteFilePath()});
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
        if (naturalLess(a.first, b.first))
            return true;
        return !naturalLess(b.first, a.first) && a.second < b.second; // same to the eye: any fixed order
    });
    QStringList files;
    for (const auto &[key, path] : std::as_const(sorted))
        files << path;

    // Opened explicitly but with an unknown extension: still try to show it
    if (!selectFile.isEmpty() && !files.contains(selectFile))
        files.prepend(selectFile);

    m_files = files;
    m_index = -1;
    m_filmstrip->setFiles(m_files);
    applyStripOpacity();
    layoutOsd();

    if (m_files.isEmpty()) {
        m_imagePending = m_fullImagePending = false;
        updateSpinner();
        m_view->clear();
        updateTitle();
        updateActions();
        toast(tr("No images here"), OsdIcon::Open, 3000);
        return;
    }
    showIndex(selectFile.isEmpty() ? 0 : m_files.indexOf(selectFile));
}

void MainWindow::showIndex(int index)
{
    if (index < 0 || index >= m_files.size() || index == m_index)
        return;
    // Switching faster than a transition can play: no transition, and none left running
    m_quickNavigation = m_transition->isRunning()
        || (m_lastNavigation.isValid() && !m_lastNavigation.hasExpired(TransitionOverlay::DurationMs));
    m_lastNavigation.start();
    if (m_quickNavigation)
        m_transition->stop();

    m_index = index;
    m_deleteArmed.invalidate();
    m_filmstrip->setCurrentIndex(index);
    m_slideTimer->stop(); // restarted once the new image is on screen

    updateTitle();
    m_imagePending = true;
    m_fullImagePending = false;
    updateSpinner();
    m_loader->releaseFull();
    m_loader->request(currentPath());

    for (int offset : {1, -1, 2})
        if (int i = index + offset; i >= 0 && i < m_files.size())
            m_loader->prefetch(m_files.at(i));
    if (wrapsAround()) {
        const int n = int(m_files.size());
        m_loader->prefetch(m_files.at((index + 1) % n));
        m_loader->prefetch(m_files.at((index - 1 + n) % n));
    }

    updateActions();
}

bool MainWindow::wrapsAround() const
{
    return m_slideshow || m_loopAct->isChecked();
}

void MainWindow::navigate(int delta)
{
    if (m_files.isEmpty())
        return;
    const int n = int(m_files.size());
    showIndex(wrapsAround() ? ((m_index + delta) % n + n) % n : m_index + delta);
}

void MainWindow::onImageLoaded(const QString &path, const QImage &image, const QSize &fullSize, const QString &error)
{
    if (path != currentPath())
        return; // a prefetch, or the user already moved on

    m_imagePending = false;
    updateSpinner();
    if (image.isNull()) {
        m_transition->stop();
        m_view->clear();
        toast(tr("Cannot open: %1").arg(error), OsdIcon::Info, 3000);
        scheduleNextSlide();
    } else {
        // Snapshot the old frame (including a transition still in flight) before swapping
        const bool animate = m_slideshow && m_view->hasImage() && !m_quickNavigation;
        const QPixmap from = animate ? m_view->viewport()->grab() : QPixmap();
        m_transition->stop();
        m_view->setImage(image, fullSize);
        if (animate)
            m_transition->start(from, m_view->viewport()->grab(), m_effect); // schedules the next slide when done
        else
            scheduleNextSlide();
    }
    updateInfo(image.isNull() ? QSize() : fullSize);
    updateActions();
    updateCursor();
    if (m_osdVisible)
        m_osdHideTimer->start();
}

void MainWindow::updateSpinner()
{
    m_spinner->setActive(m_imagePending || m_fullImagePending);
}

void MainWindow::openFileDialog()
{
    const QString filter = tr("Images (%1);;All files (*)").arg(imageNameFilters().join(' '));
    const QString startDir = m_files.isEmpty() ? QDir::homePath() : QFileInfo(currentPath()).absolutePath();
    setHelpVisible(false);
    m_open->ask(tr("Open Image or Folder"), startDir, filter);
    showOsd(); // keeps the cursor visible
}

void MainWindow::deleteCurrent()
{
    const QString path = currentPath();
    if (path.isEmpty())
        return;

    if (!m_deleteArmed.isValid() || m_deleteArmed.hasExpired(3000)) {
        m_deleteArmed.start();
        toast(tr("Press Delete again to move to trash"), OsdIcon::Delete, 3000);
        return;
    }
    m_deleteArmed.invalidate();

    if (!QFile::moveToTrash(path)) {
        toast(tr("Could not move to trash"), OsdIcon::Delete, 3000);
        return;
    }
    toast(tr("Moved to trash"), OsdIcon::Delete);

    m_loader->forget(path);
    const int removed = m_index;
    m_files.removeAt(removed);
    m_filmstrip->removeAt(removed);
    m_index = -1;
    layoutOsd();

    if (m_files.isEmpty()) {
        m_imagePending = m_fullImagePending = false;
        updateSpinner();
        stopSlideshow();
        m_view->clear();
        updateTitle();
        updateInfo({});
        updateActions();
        applyStripOpacity();
        showOsd();
        return;
    }
    showIndex(qMin(removed, m_files.size() - 1));
}

void MainWindow::toggleFullScreen()
{
    if (isFullScreen())
        showNormal();
    else
        showFullScreen();
}

void MainWindow::setSlideInterval(int ms)
{
    m_slideIntervalMs = ms;
    QSettings().setValue("slideshow/interval", ms);
    m_settings->setCurrent(SettingsIntervalGroup, ms);
    if (m_slideTimer->isActive())
        m_slideTimer->start(m_slideIntervalMs);
    if (!m_settings->isVisible()) // the panel shows the choice itself, and a message would cover it
        toast(tr("%1 s per slide").arg(secondsText(ms)), OsdIcon::Play);
}

void MainWindow::setSlideEffect(TransitionOverlay::Effect effect)
{
    m_effect = effect;
    QSettings().setValue("slideshow/effect", int(effect));
    m_settings->setCurrent(SettingsEffectGroup, int(effect));
    if (!m_settings->isVisible())
        toast(TransitionOverlay::effectName(effect), OsdIcon::Settings);
}

void MainWindow::stepSlideInterval(int direction)
{
    const int index = SlideIntervalsMs.indexOf(nearestSlideInterval(m_slideIntervalMs));
    setSlideInterval(SlideIntervalsMs[qBound(0, index + direction, int(SlideIntervalsMs.size()) - 1)]);
}

void MainWindow::playPause()
{
    if (!m_slideshow)
        startSlideshow();
    else
        toggleSlideshowPause();
    if (m_slideshow)
        m_flash->flash(m_slideshowPaused ? OsdIcon::Pause : OsdIcon::Play);
}

void MainWindow::startSlideshow()
{
    if (m_slideshow || m_files.isEmpty())
        return;
    m_slideshow = true;
    m_slideshowPaused = false;

    m_view->setBackgroundBrush(Qt::black);
    m_view->setDragMode(QGraphicsView::NoDrag);
    updatePlaybackUi();
    updateActions();
    hideOsd(true);
    updateCursor();
    scheduleNextSlide();
}

void MainWindow::stopSlideshow()
{
    if (!m_slideshow)
        return;
    m_slideshow = false;
    m_slideshowPaused = false;
    m_slideTimer->stop();
    m_transition->stop();

    m_view->setBackgroundBrush(ViewBackground);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    updatePlaybackUi();
    updateActions();
    showOsd();
    updateCursor();
}

void MainWindow::toggleSlideshowPause()
{
    if (!m_slideshow)
        return;
    m_slideshowPaused = !m_slideshowPaused;
    if (m_slideshowPaused)
        m_slideTimer->stop();
    else
        scheduleNextSlide();
    updatePlaybackUi();
}

void MainWindow::advanceSlideshow()
{
    if (m_slideshow && m_files.size() > 1)
        navigate(+1);
}

void MainWindow::scheduleNextSlide()
{
    if (m_slideshow && !m_slideshowPaused)
        m_slideTimer->start(m_slideIntervalMs);
}

void MainWindow::updatePlaybackUi()
{
    const bool playing = m_slideshow && !m_slideshowPaused;
    m_controlBar->setIcon(m_playAct, playing ? OsdIcon::Pause : OsdIcon::Play);
    m_playAct->setText(!m_slideshow ? tr("Start slideshow") : (playing ? tr("Pause slideshow") : tr("Resume slideshow")));
    m_controlBar->setKeysFrom(m_playAct, m_slideshow ? m_pauseAct : m_slideshowAct);
    m_pauseIndicator->setActive(m_slideshow && m_slideshowPaused);
    m_stopAct->setEnabled(m_slideshow);
    updateTitle();
}

void MainWindow::updateActions()
{
    const bool hasImage = m_view->hasImage();
    for (QAction *action : std::as_const(m_imageActions))
        action->setEnabled(hasImage);
    m_playAct->setEnabled(!m_files.isEmpty());
    m_slideshowAct->setEnabled(!m_files.isEmpty());
    m_filmstrip->setWrapAround(wrapsAround());
    m_prevAct->setEnabled(m_index > 0 || (wrapsAround() && m_files.size() > 1));
    m_nextAct->setEnabled((m_index >= 0 && m_index < m_files.size() - 1) || (wrapsAround() && m_files.size() > 1));
    m_emptyHint->setVisible(m_files.isEmpty());
    updatePanels();
}

void MainWindow::updateTitle()
{
    const QString name = QFileInfo(currentPath()).fileName();
    // Qt adds " — ZenViewer <version>" (the application display name)
    QString title = name;
    if (m_slideshow && !name.isEmpty())
        title += m_slideshowPaused ? tr(" [slideshow paused]") : tr(" [slideshow mode]");
    setWindowTitle(title);
    m_topBar->setTexts(name, m_index >= 0 ? QStringLiteral("%1 / %2").arg(m_index + 1).arg(m_files.size()) : QString());
}

void MainWindow::updateInfo(const QSize &imageSize)
{
    const QString path = currentPath();
    if (path.isEmpty()) {
        m_infoPanel->setContent({}, {});
        return;
    }
    const QFileInfo info(path);
    const QLocale locale;
    QStringList facts = {QStringLiteral("%1 / %2").arg(m_index + 1).arg(m_files.size())};
    if (imageSize.isValid())
        facts << tr("%1 × %2").arg(imageSize.width()).arg(imageSize.height());
    facts << locale.formattedDataSize(info.size())
          << QString::fromLatin1(QImageReader::imageFormat(path)).toUpper();
    m_infoPanel->setContent(info.completeBaseName(), {
        facts.join(QStringLiteral("   ·   ")),
        locale.toString(info.lastModified().date(), QLocale::LongFormat) + QStringLiteral("   ·   ")
            + locale.toString(info.lastModified().time(), QLocale::ShortFormat),
    });
    layoutOsd();
}

void MainWindow::confirmQuit()
{
    setHelpVisible(false);
    m_confirm->ask(tr("Quit ZenViewer?"), tr("Yes"), tr("No"));
    showOsd(); // keeps the cursor visible
}

QString MainWindow::helpText() const
{
    return m_help->plainText();
}

QString MainWindow::currentPath() const
{
    return (m_index >= 0 && m_index < m_files.size()) ? m_files.at(m_index) : QString();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::Resize:
        if (watched == m_view)
            layoutOsd();
        break;
    case QEvent::Leave:
        if (watched == this) {
            m_mouseAtLeft = m_mouseAtBottom = false;
            m_lastMousePos = QPointF(-1, -1); // coming back to the same spot still counts as a move
            updatePanels();
        }
        break;
    case QEvent::ShortcutOverride: // every key press, before shortcuts consume it
        // The info panel comes and goes on its own, without the controls
        if (m_infoAct->shortcuts().contains(static_cast<QKeyEvent *>(event)->keyCombination()))
            break;
        [[fallthrough]];
    case QEvent::MouseMove:
    case QEvent::Wheel:
    case QEvent::MouseButtonPress: {
        auto *widget = qobject_cast<QWidget *>(watched);
        if (!widget || widget->window() != this)
            break;
        if (event->type() == QEvent::ShortcutOverride && !overlayOpen()) {
            const QKeyCombination key = static_cast<QKeyEvent *>(event)->keyCombination();
            if (m_prevAct->shortcuts().contains(key) || m_nextAct->shortcuts().contains(key))
                m_stripKeyTimer->start();
        }
        if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
            // Qt also sends synthetic moves when widgets change under a resting cursor
            const QPointF pos = static_cast<QMouseEvent *>(event)->globalPosition();
            if (pos != m_lastMousePos || event->type() == QEvent::MouseButtonPress) {
                m_controlBar->endKeyboardNavigation();
                m_revealBar = m_revealStrip = false;
                const QPoint at = m_view->mapFromGlobal(pos).toPoint();
                // A magnified icon sticks out of the column and still counts as on the bar
                m_mouseAtLeft = at.x() < m_controlBar->panelWidth()
                    || (m_controlBar->isVisible() && m_controlBar->mask().contains(m_controlBar->mapFromGlobal(pos).toPoint()));
                m_mouseAtBottom = at.y() >= m_view->height() - m_filmstrip->height();
            }
            m_lastMousePos = pos;
        }
        showOsd();
        if (event->type() == QEvent::MouseButtonPress && !m_open->isVisible() && !m_settings->isVisible()) {
            const Qt::MouseButton button = static_cast<QMouseEvent *>(event)->button();
            if (button == Qt::BackButton || button == Qt::ForwardButton) {
                navigate(button == Qt::BackButton ? -1 : 1);
                return true;
            }
        }
        break;
    }
    default:
        break;
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange && m_controlBar) {
        // The button says what it will do: in full screen it leads back to a window
        m_controlBar->setIcon(m_fullScreenAct, isFullScreen() ? OsdIcon::Windowed : OsdIcon::Fullscreen);
        m_fullScreenAct->setText(isFullScreen() ? tr("Window mode") : tr("Full screen"));
        layoutOsd();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    stopSlideshow();
    if (isFullScreen())
        showNormal();
    QSettings().setValue("window/geometry", saveGeometry());
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty() && urls.first().isLocalFile())
        openPath(urls.first().toLocalFile());
}
