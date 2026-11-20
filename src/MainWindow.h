#pragma once

#include "ImagePrinter.h"
#include "OsdIcons.h"
#include "TransitionOverlay.h"

#include <QElapsedTimer>
#include <QMainWindow>

#include <functional>
#include <memory>

class AboutOverlay;
class ConfirmOverlay;
class ControlBar;
class EmptyHint;
class ExifOverlay;
class ActionFlash;
class Filmstrip;
class HelpOverlay;
class ImageLoader;
class ImageView;
class InfoPanel;
class OsdButton;
class LoadingSpinner;
class OpenOverlay;
class OsdToast;
class PauseIndicator;
class SettingsOverlay;
class TopBar;
class QAction;
class QTimer;
class QVariantAnimation;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void openPath(const QString &path);
    QString helpText() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void createActions();
    void createOsd();
    void layoutOsd();
    qreal imageTextScale() const;
    void showOsd();
    void hideOsd(bool force = false);
    void applyOsdOpacity(qreal opacity);
    void applyArrowOpacity();
    void applyStripOpacity();
    void updatePanels();
    bool osdHovered() const;
    bool overlayOpen() const;
    // The help or the About card is up
    bool cardOpen() const;
    void updateCursor();
    void updateSpinner();
    bool wrapsAround() const;
    void toast(const QString &text, OsdIcon icon = OsdIcon::None, int durationMs = 1200);
    void setHelpVisible(bool visible);
    void setAboutVisible(bool visible);
    void showExif();
    void printCurrent();
    void updateExif();
    void confirmQuit();
    // The right-click menu: every command, with its key, as at the moment it opens
    void showContextMenu(const QPoint &globalPos);
    // The File menu: copy, move or delete the image and its raw file. Deleting from
    // the menu asks first, rather than waiting for a second press.
    static QString fileNames(const QStringList &files);
    void confirmDelete(const QStringList &files);
    void trashFiles(const QStringList &files);
    void askTransfer(const QStringList &files, bool move);
    void transferTo(const QString &folder);
    void dropFromList(const QString &path);

    void loadFolder(const QString &dir, const QString &selectFile);
    void showIndex(int index);
    void navigate(int delta);
    void onImageLoaded(const QString &path, const QImage &image, const QSize &fullSize, const QString &error);
    void openFileDialog();
    void deleteCurrent();
    void toggleFullScreen();

    void setSlideInterval(int ms);
    void setSlideEffect(TransitionOverlay::Effect effect);
    void stepSlideInterval(int direction);
    void playPause();
    void startSlideshow();
    void stopSlideshow();
    void toggleSlideshowPause();
    void advanceSlideshow();
    void scheduleNextSlide();
    void updatePlaybackUi();

    void updateActions();
    void updateTitle();
    void updateInfo(const QSize &imageSize);
    QString currentPath() const;

    ImageView *m_view = nullptr;
    Filmstrip *m_filmstrip = nullptr;
    ImageLoader *m_loader = nullptr;
    TransitionOverlay *m_transition = nullptr;

    // On-screen display
    ControlBar *m_controlBar = nullptr;
    OsdButton *m_prevButton = nullptr;
    OsdButton *m_nextButton = nullptr;
    TopBar *m_topBar = nullptr;
    InfoPanel *m_infoPanel = nullptr;
    OsdToast *m_toast = nullptr;
    PauseIndicator *m_pauseIndicator = nullptr;
    HelpOverlay *m_help = nullptr;
    AboutOverlay *m_about = nullptr;
    ConfirmOverlay *m_confirm = nullptr;
    OpenOverlay *m_open = nullptr;
    SettingsOverlay *m_settings = nullptr;
    ExifOverlay *m_exif = nullptr;
    LoadingSpinner *m_spinner = nullptr;
    ActionFlash *m_flash = nullptr;
    EmptyHint *m_emptyHint = nullptr;
    QVariantAnimation *m_osdFade = nullptr;
    QTimer *m_osdHideTimer = nullptr;
    qreal m_osdOpacity = 1.0;
    bool m_osdVisible = true;
    // The icon bar and the strip come and go with the cursor at their window edge
    QVariantAnimation *m_barFade = nullptr;
    QVariantAnimation *m_stripFade = nullptr;
    QTimer *m_stripKeyTimer = nullptr; // running: left / right was pressed a moment ago
    qreal m_barOpacity = 1.0;
    qreal m_stripOpacity = 1.0;
    bool m_barShown = true;
    bool m_stripShown = true;
    bool m_mouseAtLeft = false;
    bool m_mouseAtBottom = false;
    // Called up from the keyboard: until the OSD times out or the mouse moves
    bool m_revealBar = false;
    bool m_revealStrip = false;

    QAction *m_openAct = nullptr;
    QAction *m_prevAct = nullptr;
    QAction *m_nextAct = nullptr;
    QAction *m_playAct = nullptr;
    QAction *m_stopAct = nullptr;
    QAction *m_pauseAct = nullptr;
    QAction *m_slideshowAct = nullptr;
    QAction *m_infoAct = nullptr;
    QAction *m_exifAct = nullptr;
    QAction *m_printAct = nullptr;
    QAction *m_firstAct = nullptr;
    QAction *m_lastAct = nullptr;
    QAction *m_shorterAct = nullptr;
    QAction *m_longerAct = nullptr;
    QAction *m_settingsAct = nullptr;
    QAction *m_iconBarAct = nullptr;
    QAction *m_helpAct = nullptr;
    QAction *m_quitAct = nullptr;
    // What the confirmation's Yes does: quit, or move the image to the trash
    std::function<void()> m_confirmYes;
    // Waiting for the folder to copy or move them to
    QStringList m_transferFiles;
    bool m_transferMove = false;
    // Where the last copy or move went; only for as long as the app runs
    QString m_lastTransferFolder;
    std::unique_ptr<ImagePrinter> m_printer;
    QAction *m_aboutAct = nullptr;
    QAction *m_filmstripAct = nullptr;
    QAction *m_loopAct = nullptr;
    QAction *m_fullScreenAct = nullptr;
    QList<QAction *> m_imageActions; // enabled only while an image is shown
    QList<std::pair<QAction *, OsdIcon>> m_helpActions;

    QStringList m_files;
    int m_index = -1;
    QPointF m_lastMousePos;
    QElapsedTimer m_lastNavigation;
    bool m_quickNavigation = false; // too soon after the last switch for a transition
    bool m_imagePending = false;
    bool m_fullImagePending = false;
    QElapsedTimer m_deleteArmed; // second Delete press within a few seconds confirms

    QTimer *m_slideTimer = nullptr;
    TransitionOverlay::Effect m_effect = TransitionOverlay::Effect::Random;
    int m_slideIntervalMs = 2000;
    bool m_slideshow = false;
    bool m_slideshowPaused = false;
};
