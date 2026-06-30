qt_generate_deploy_qml_app_script(
    TARGET appgav
    OUTPUT_SCRIPT deploy_script
    MACOS_BUNDLE_POST_BUILD
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)

message("deploy script name: ${deploy_script}")
message("qt_deploy_support: ${QT_DEPLOY_SUPPORT}")

# Deploy Qt dependencies
install(SCRIPT ${deploy_script})

# When GAV_QTMULTIMEDIA_PLUGIN is provided, replace Qt's bundled ffmpegmediaplugin in the install tree with a custom
# build (statically linked against vcpkg's newer FFmpeg). Runs AFTER the Qt deploy script so we override its output.
if(GAV_QTMULTIMEDIA_PLUGIN)
    if(NOT EXISTS "${GAV_QTMULTIMEDIA_PLUGIN}")
        message(FATAL_ERROR "GAV_QTMULTIMEDIA_PLUGIN points to non-existent file: ${GAV_QTMULTIMEDIA_PLUGIN}")
    endif()
    install(CODE "
        file(GLOB_RECURSE _existing_plugin LIST_DIRECTORIES false
            \"\${CMAKE_INSTALL_PREFIX}/*ffmpegmediaplugin*\")
        list(FILTER _existing_plugin EXCLUDE REGEX \"\\\\.(dSYM|pdb|debug)$\")
        if(_existing_plugin)
            list(GET _existing_plugin 0 _existing_plugin)
            get_filename_component(_dest_dir \"\${_existing_plugin}\" DIRECTORY)
            get_filename_component(_existing_name \"\${_existing_plugin}\" NAME)
            message(STATUS \"GAV: replacing bundled FFmpeg plugin at \${_existing_plugin}\")
            file(REMOVE \"\${_existing_plugin}\")
            file(COPY \"${GAV_QTMULTIMEDIA_PLUGIN}\" DESTINATION \"\${_dest_dir}\")
            # Rename if the source basename differs from what Qt deployed (e.g. shared- vs static-build naming).
            get_filename_component(_new_name \"${GAV_QTMULTIMEDIA_PLUGIN}\" NAME)
            if(NOT _new_name STREQUAL _existing_name)
                file(RENAME \"\${_dest_dir}/\${_new_name}\" \"\${_dest_dir}/\${_existing_name}\")
            endif()
        else()
            message(WARNING \"GAV_QTMULTIMEDIA_PLUGIN set but no ffmpegmediaplugin found in install tree to override\")
        endif()
    ")
endif()

# Enable support for packing using CPack
if(UNIX AND NOT APPLE) # Linux
    set(CPACK_GENERATOR "TGZ;DEB;RPM;AppImage")
    # The AppImage generator expects CPACK_PACKAGE_ICON to be a bare filename that StringStartsWith the desktop file's
    # `Icon=` value; it then locates the file in the staged install tree via FindFile. gav.desktop has `Icon=gav`, and
    # the install rule places the PNG at share/icons/hicolor/256x256/apps/gav.png.
    set(CPACK_PACKAGE_ICON "gav.png")
elseif(APPLE) # macOS
    set(CPACK_GENERATOR "TGZ;DragNDrop")
elseif (WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
endif ()

# CPack settings
set(CPACK_PRE_BUILD_SCRIPTS ${QT_DEPLOY_SUPPORT})
set(CPACK_PACKAGE_NAME "GAV Player")
set(GAV_PACKAGE_FILENAME "GAV-Player")
set(CPACK_PACKAGE_VENDOR "Akshay Raj Gollahalli")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GAV Player - A simple audio and video player")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "GAV Player")
set(CPACK_PACKAGE_CONTACT "Akshay Raj Gollahalli <akshay@gollahalli.com>")
SET(CPACK_OUTPUT_FILE_PREFIX packages)
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_COMPONENTS_GROUPING IGNORE)

## License file
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)

# NSIS settings for Windows
if(WIN32)
    set(CPACK_NSIS_DISPLAY_NAME "GAV Player")
    set(CPACK_NSIS_PACKAGE_NAME "GAV Player")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")

    # Create proper shortcuts and register for Start Menu search
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "
        CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\GAV Player.lnk' '$INSTDIR\\\\bin\\\\gav.exe' '' '$INSTDIR\\\\bin\\\\gav.exe' 0
        CreateShortCut '$DESKTOP\\\\GAV Player.lnk' '$INSTDIR\\\\bin\\\\gav.exe' '' '$INSTDIR\\\\bin\\\\gav.exe' 0
        WriteRegStr HKLM 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\gav.exe' '' '$INSTDIR\\\\bin\\\\gav.exe'
        WriteRegStr HKLM 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\gav.exe' 'Path' '$INSTDIR\\\\bin'
        WriteRegStr HKLM 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\GAVPlayer' 'DisplayVersion' '${CPACK_PACKAGE_VERSION}'
        WriteRegStr HKLM 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\GAVPlayer' 'DisplayName' 'GAV Player'
        WriteRegStr HKLM 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\GAVPlayer' 'Publisher' '${CPACK_PACKAGE_VENDOR}'
    ")

    set(CPACK_NSIS_DELETE_ICONS_EXTRA "
        Delete '$SMPROGRAMS\\\\$MUI_TEMP\\\\GAV Player.lnk'
        Delete '$DESKTOP\\\\GAV Player.lnk'
        DeleteRegKey HKLM 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\gav.exe'
        DeleteRegKey HKLM 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\GAVPlayer'
    ")
    set(CPACK_NSIS_MODIFY_PATH ON)
    
    # Branding - Convert paths to Windows format
    file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/assets/images/logo.ico" LOGO_ICO_PATH)
    file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/assets/images/logo.bmp" LOGO_BMP_PATH)
    
    set(CPACK_NSIS_MUI_ICON "${LOGO_ICO_PATH}")
    set(CPACK_NSIS_MUI_UNIICON "${LOGO_ICO_PATH}")
    set(CPACK_NSIS_MUI_HEADERIMAGE_BITMAP "${LOGO_BMP_PATH}")
    
    # License and website
    set(CPACK_NSIS_MENU_LINKS "https://gav.gollahalli.com" "GAV Player Website")
    
    # Standard installation settings
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH ON)
    
    # Set default installation directory (Program Files - will prompt for admin)
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
endif()

## Installer settings
# DEB settings
set(CPACK_DEBIAN_PACKAGE_NAME "gav-player")
set(CPACK_DEBIAN_FILE_NAME "${GAV_PACKAGE_FILENAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}.deb")

# RPM settings
set(CPACK_RPM_PACKAGE_NAME "gav-player")
set(CPACK_RPM_FILE_NAME "${GAV_PACKAGE_FILENAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}.rpm")

# DMG settings
set(CPACK_DMG_FILE_NAME "${GAV_PACKAGE_FILENAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}.dmg")

# TGZ settings
set(CPACK_ARCHIVE_FILE_NAME "${GAV_PACKAGE_FILENAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

# Set the package file name for all generators (including NSIS and ZIP)
set(CPACK_PACKAGE_FILE_NAME "${GAV_PACKAGE_FILENAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

include(CPack)
