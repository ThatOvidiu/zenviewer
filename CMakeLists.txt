cmake_minimum_required(VERSION 3.21)

project(zenviewer VERSION 0.6.0 LANGUAGES CXX)

# Shown in the About panel and the Windows file properties
set(ZENVIEWER_AUTHOR "Ovidiu Albu")
set(ZENVIEWER_CONTACT "ovi.albu@gmail.com")
set(ZENVIEWER_COPYRIGHT_SINCE 2026)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(zenviewer
    src/main.cpp
    src/MainWindow.h src/MainWindow.cpp
    src/ImageView.h src/ImageView.cpp
    src/ImageLoader.h src/ImageLoader.cpp
    src/ImageOrientation.h src/ImageOrientation.cpp
    src/Filmstrip.h src/Filmstrip.cpp
    src/TransitionOverlay.h src/TransitionOverlay.cpp
    src/OsdIcons.h src/OsdIcons.cpp
    src/OsdWidgets.h src/OsdWidgets.cpp
)

target_link_libraries(zenviewer PRIVATE Qt6::Widgets)
# The version and the details above live only here; the code gets them from here
target_compile_definitions(zenviewer PRIVATE
    ZENVIEWER_VERSION="${PROJECT_VERSION}"
    ZENVIEWER_AUTHOR="${ZENVIEWER_AUTHOR}"
    ZENVIEWER_CONTACT="${ZENVIEWER_CONTACT}"
    ZENVIEWER_COPYRIGHT_SINCE="${ZENVIEWER_COPYRIGHT_SINCE}")

# The app icon, for the window and the About panel (src/icons/make-icons.sh makes it)
qt_add_resources(zenviewer "icons" PREFIX "/" BASE src FILES
    src/icons/zenviewer-16.png src/icons/zenviewer-24.png src/icons/zenviewer-32.png
    src/icons/zenviewer-48.png src/icons/zenviewer-64.png src/icons/zenviewer-128.png
    src/icons/zenviewer-256.png)

if(NOT MSVC)
    target_compile_options(zenviewer PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(ZENVIEWER_LTO "Link-time optimization in optimized builds" ON)

if(ZENVIEWER_LTO AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error LANGUAGES CXX)
    if(ipo_supported)
        set_property(TARGET zenviewer PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
    else()
        message(STATUS "LTO not supported: ${ipo_error}")
    endif()
endif()

set_target_properties(zenviewer PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

# Installing and anything else that differs per platform lives in the
# platform's own folder.
if(WIN32)
    include(windows/windows.cmake)
elseif(APPLE)
    install(TARGETS zenviewer BUNDLE DESTINATION .)
else()
    include(linux/linux.cmake)
endif()
