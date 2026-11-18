#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QLoggingCategory>
#include <QTextStream>

#ifdef __GLIBC__
#include <malloc.h>
#endif
#ifdef _WIN32
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#endif

int main(int argc, char *argv[])
{
#ifdef __GLIBC__
    // Image buffers are big and short-lived. With a fixed threshold every
    // allocation over 1 MB is mmap'd and returned to the system when freed;
    // glibc's adaptive default keeps them in the heap, where they fragment
    // it and memory use creeps up over a session.
    mallopt(M_MMAP_THRESHOLD, 1024 * 1024);
#endif
#ifdef _WIN32
    // A GUI app on Windows has no console, so -h and --version would print
    // nowhere; when started from a terminal, write to that one instead.
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE *console = nullptr;
        freopen_s(&console, "CONOUT$", "w", stdout);
        freopen_s(&console, "CONOUT$", "w", stderr);
        // Qt writes UTF-8, but cmd's console reads its own code page (437, 852…), which
        // garbles ← → ↑ ↓ and ·. UTF-8 while the app runs, the old one back when it exits.
        static const UINT previousCodePage = GetConsoleOutputCP();
        SetConsoleOutputCP(CP_UTF8);
        // cmd doesn't wait for a windowed app, so its prompt comes before our text and
        // nothing follows it. After -h or --version, one Enter brings a fresh prompt.
        static bool printOnly = false;
        for (int i = 1; i < argc && qstrcmp(argv[i], "--") != 0; ++i)
            for (const char *option : {"-h", "--help", "-v", "--version"})
                printOnly = printOnly || qstrcmp(argv[i], option) == 0;
        std::atexit([] {
            std::fflush(stdout);
            SetConsoleOutputCP(previousCodePage);
            if (!printOnly)
                return; // after a session, an Enter would send whatever was typed meanwhile
            const HANDLE input = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                             nullptr, OPEN_EXISTING, 0, nullptr);
            if (input == INVALID_HANDLE_VALUE)
                return;
            INPUT_RECORD keys[2] = {};
            for (int k = 0; k < 2; ++k) {
                keys[k].EventType = KEY_EVENT;
                keys[k].Event.KeyEvent.bKeyDown = k == 0;
                keys[k].Event.KeyEvent.wRepeatCount = 1;
                keys[k].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
                keys[k].Event.KeyEvent.wVirtualScanCode = WORD(MapVirtualKeyW(VK_RETURN, MAPVK_VK_TO_VSC));
                keys[k].Event.KeyEvent.uChar.UnicodeChar = L'\r';
            }
            DWORD written = 0;
            WriteConsoleInputW(input, keys, 2, &written);
            CloseHandle(input);
        });
    }
#endif
#ifndef _WIN32
    // -h lists the shortcuts the real window sets up, but must work without a display
    // (over SSH, say), and so must --version. Windows always has one, and its
    // package has no offscreen plugin.
    for (int i = 1; i < argc && qstrcmp(argv[i], "--") != 0; ++i) {
        const char *arg = argv[i];
        if (qstrcmp(arg, "-h") == 0 || qstrcmp(arg, "--help") == 0 || qstrcmp(arg, "-v") == 0
            || qstrcmp(arg, "--version") == 0) {
            qputenv("QT_QPA_PLATFORM", "offscreen");
            qputenv("QT_QPA_PLATFORMTHEME", "");
        }
    }
#endif
#ifdef _WIN32
    // The Windows package keeps Qt's DLLs and plugins in a dll folder beside the
    // exe (see windows/windows.cmake); Qt finds the plugins there with this, before
    // QApplication loads the first one
    wchar_t exePath[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        const QString dllDir =
            QFileInfo(QString::fromWCharArray(exePath, int(length))).absolutePath() + QStringLiteral("/dll");
        if (QFileInfo::exists(dllDir))
            QCoreApplication::addLibraryPath(dllDir);
    }
#elif defined(__linux__)
    // The same for the Linux bundle (see linux/bundle), whose lib folder beside the
    // binary holds Qt's libraries and its plugin folders (lib/platforms…)
    const QString libDir =
        QFileInfo(QFileInfo(QStringLiteral("/proc/self/exe")).symLinkTarget()).absolutePath() + QStringLiteral("/lib");
    if (QFileInfo::exists(libDir + QStringLiteral("/platforms")))
        QCoreApplication::addLibraryPath(libDir);
    // In a Flatpak (see linux/flatpak) XDG_DATA_HOME is the app's private folder, and
    // Qt looks for the trash there and finds none. The real one is the host's.
    if (qEnvironmentVariableIsSet("FLATPAK_ID") && qEnvironmentVariableIsSet("HOST_XDG_DATA_HOME"))
        qputenv("XDG_DATA_HOME", qgetenv("HOST_XDG_DATA_HOME"));
#endif
    // Wayland app-id / X11 WM_CLASS, so WM window rules can match "zenviewer".
    // Set before QApplication so the portal registration below uses it.
    // A Flatpak names its desktop file after its id (see linux/flatpak).
    QGuiApplication::setDesktopFileName(qEnvironmentVariable("FLATPAK_ID", QStringLiteral("zenviewer")));
    // Qt registers the app ID with xdg-desktop-portal, but the portal only
    // accepts that as the first call on the connection; when it has already
    // derived an ID from the systemd scope it refuses with a harmless warning.
    QLoggingCategory::setFilterRules("qt.qpa.services.warning=false");
    QApplication app(argc, argv);
    QApplication::setApplicationName("zenviewer");
    QApplication::setApplicationDisplayName(QStringLiteral("ZenViewer " ZENVIEWER_VERSION)); // shown in the title bar
    QApplication::setApplicationVersion(QStringLiteral(ZENVIEWER_VERSION)); // from CMakeLists.txt
    // Qt refuses images over 256 MB by default; a 100 MP photo needs ~400 MB.
    // Only full-resolution zooms and thumbnail decodes allocate this much.
    QImageReader::setAllocationLimit(2048);

    // The app icon, in the sizes made by src/icons/make-icons.sh (window, taskbar, About)
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256})
        icon.addFile(QStringLiteral(":/icons/zenviewer-%1.png").arg(size), QSize(size, size));
    QApplication::setWindowIcon(icon);

    QCommandLineParser parser;
    parser.setApplicationDescription("A lightweight image viewer");
    const QCommandLineOption helpOption({"h", "help"}, "Show this help and exit.");
    parser.addOption(helpOption);
    parser.addVersionOption();
    parser.addPositionalArgument("path", "Image file or folder to open");
    parser.process(app);

    MainWindow window;
    if (parser.isSet(helpOption)) {
        QTextStream(stdout) << parser.helpText() << '\n' << window.helpText();
        return 0;
    }
    window.show();

    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty())
        window.openPath(args.first());

    return app.exec();
}
