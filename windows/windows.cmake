# Windows part of the build; included from the top-level CMakeLists.txt.
# `cmake --install` produces a folder that is the whole app: zenviewer.exe, a
# dll folder with the Qt and Microsoft runtime DLLs and Qt's plugins, and the
# licences. Needs Qt >= 6.5.

if(MSVC)
    # The sources are UTF-8 (strings such as "·" and "×"); without /utf-8 MSVC
    # reads them in the system code page and garbles them.
    target_compile_options(zenviewer PRIVATE /utf-8 /W3 /permissive-)
endif()

# Icon and version details of zenviewer.exe (Explorer, taskbar, Properties > Details)
enable_language(RC)
set(ZENVIEWER_ICO "${PROJECT_SOURCE_DIR}/src/icons/zenviewer.ico")
configure_file(${CMAKE_CURRENT_LIST_DIR}/zenviewer.rc.in ${CMAKE_BINARY_DIR}/zenviewer.rc @ONLY)
target_sources(zenviewer PRIVATE ${CMAKE_BINARY_DIR}/zenviewer.rc)

install(TARGETS zenviewer RUNTIME DESTINATION .)

# Windows only looks for an exe's DLLs beside it, not in a subfolder, unless that
# subfolder is a "private assembly": a folder whose manifest (dll/dll.manifest,
# written below) lists its DLLs, named in the exe's own manifest. The name "dll"
# must match the folder, the line here and the manifest written further down.
if(MSVC)
    target_link_options(zenviewer PRIVATE
        "/MANIFESTDEPENDENCY:type='win32' name='dll' version='1.0.0.0' processorArchitecture='amd64'")
endif()


# Qt's DLLs and plugins, via windeployqt. Leave out what a Widgets app doesn't
# use: the network stack (only the touch-input plugin pulls it in), OpenGL /
# Direct3D shader compilers, Qt's translations (the app loads none) and the
# VC++ installer (the runtime DLLs are copied below instead).
qt_generate_deploy_app_script(TARGET zenviewer
    OUTPUT_SCRIPT deploy_script
    NO_TRANSLATIONS
    DEPLOY_TOOL_OPTIONS --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler
        --no-system-dxc-compiler --skip-plugin-types generic,tls,networkinformation)
# DLLs and plugin folders (platforms, imageformats…) all in dll; main.cpp tells
# Qt to look for its plugins there, so no qt.conf is needed
install(CODE "set(QT_DEPLOY_BIN_DIR dll)\nset(QT_DEPLOY_PLUGINS_DIR dll)")
install(SCRIPT ${deploy_script})

# Microsoft's C++ runtime DLLs (vcruntime140.dll, msvcp140.dll…), which Microsoft
# allows, so other PCs don't need the VC++ Redistributable.
if(MSVC)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION dll)
    include(InstallRequiredSystemLibraries)
endif()

# dll/dll.manifest, listing every DLL that ended up in dll (see the top of this
# file), and no qt.conf, which the Qt deploy step writes but nothing needs
install(CODE [[
    set(dir "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/dll")
    file(REMOVE "${dir}/qt.conf" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/qt.conf")
    file(GLOB dlls RELATIVE "${dir}" "${dir}/*.dll")
    list(SORT dlls)
    set(text "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n")
    string(APPEND text "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\n")
    string(APPEND text "  <assemblyIdentity type=\"win32\" name=\"dll\" version=\"1.0.0.0\" processorArchitecture=\"amd64\"/>\n")
    foreach(dll IN LISTS dlls)
        string(APPEND text "  <file name=\"${dll}\"/>\n")
    endforeach()
    string(APPEND text "</assembly>\n")
    file(WRITE "${dir}/dll.manifest" "${text}")
    message(STATUS "Writing: ${dir}/dll.manifest")
]])

# Licences (Qt is LGPL; see windows/notices)
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION . RENAME LICENSE.txt)
install(FILES ${CMAKE_CURRENT_LIST_DIR}/notices/THIRD-PARTY-NOTICES.txt DESTINATION .)
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/notices/licenses DESTINATION .)
# TinyEXIF, built into the exe (third_party/TinyEXIF)
install(FILES ${PROJECT_SOURCE_DIR}/third_party/TinyEXIF/LICENSE DESTINATION licenses RENAME TinyEXIF-LICENSE.txt)
install(FILES ${PROJECT_SOURCE_DIR}/third_party/TinyEXIF/LICENSE.easyexif DESTINATION licenses RENAME TinyEXIF-easyexif-LICENSE.txt)
