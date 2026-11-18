# Linux part of the build; included from the top-level CMakeLists.txt.

# Qt's CMake targets add libraries the app never calls, such as OpenGL
target_link_options(zenviewer PRIVATE -Wl,--as-needed)

include(GNUInstallDirs)
install(TARGETS zenviewer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_LIST_DIR}/zenviewer.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# The app icon, where desktops look for the .desktop file's Icon=zenviewer
foreach(size 16 24 32 48 64 128 256)
    install(FILES ${PROJECT_SOURCE_DIR}/src/icons/zenviewer-${size}.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps RENAME zenviewer.png)
endforeach()
install(FILES ${PROJECT_SOURCE_DIR}/src/icons/zenviewer.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
