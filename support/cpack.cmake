qt_generate_deploy_qml_app_script(
    TARGET appgav
    OUTPUT_SCRIPT deploy_script
    MACOS_BUNDLE_POST_BUILD
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)

message("deploy script name: ${deploy_script}")
message("qt_deploy_support: ${QT_DEPLOY_SUPPORT}")
install(SCRIPT ${deploy_script})

# Enable support for packing using CPack
if(UNIX AND NOT APPLE) # Linux
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
elseif(APPLE) # macOS
    set(CPACK_GENERATOR "TGZ;DragNDrop")
elseif (WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
endif ()

# CPack settings
set(CPACK_PRE_BUILD_SCRIPTS ${QT_DEPLOY_SUPPORT})
set(CPACK_PACKAGE_NAME "GAV")
set(CPACK_PACKAGE_VENDOR "Akshay Raj Gollahalli")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GAV - A simple audio and video player")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "GAV")
set(CPACK_PACKAGE_CONTACT "Akshay Raj Gollahalli <akshay@gollahalli.com>")
SET(CPACK_OUTPUT_FILE_PREFIX packages)
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_COMPONENTS_GROUPING IGNORE)

## License file
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)

# NSIS settings for Windows
if(WIN32)
    set(CPACK_NSIS_DISPLAY_NAME "GAV - Audio Video Player")
    set(CPACK_NSIS_PACKAGE_NAME "GAV")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
    set(CPACK_NSIS_MUI_EXECUTABLES "gav.exe;GAV - Audio Video Player")
    
    # Create proper shortcuts and register for Start Menu search
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "
        CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\GAV.lnk' '$INSTDIR\\\\bin\\\\gav.exe' '' '$INSTDIR\\\\bin\\\\gav.exe' 0
        CreateShortCut '$DESKTOP\\\\GAV.lnk' '$INSTDIR\\\\bin\\\\gav.exe' '' '$INSTDIR\\\\bin\\\\gav.exe' 0
        WriteRegStr HKLM 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\gav.exe' '' '$INSTDIR\\\\bin\\\\gav.exe'
        WriteRegStr HKLM 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\gav.exe' 'Path' '$INSTDIR\\\\bin'
    ")
    
    set(CPACK_NSIS_DELETE_ICONS_EXTRA "
        Delete '$SMPROGRAMS\\\\$MUI_TEMP\\\\GAV.lnk'
        Delete '$DESKTOP\\\\GAV.lnk'
        DeleteRegKey HKLM 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\gav.exe'
    ")
    set(CPACK_NSIS_MODIFY_PATH ON)
    
    # Branding - Convert paths to Windows format
    file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/assets/images/logo.ico" LOGO_ICO_PATH)
    file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/assets/images/logo.bmp" LOGO_BMP_PATH)
    
    set(CPACK_NSIS_MUI_ICON "${LOGO_ICO_PATH}")
    set(CPACK_NSIS_MUI_UNIICON "${LOGO_ICO_PATH}")
    set(CPACK_NSIS_MUI_HEADERIMAGE_BITMAP "${LOGO_BMP_PATH}")
    
    # License and website
    set(CPACK_NSIS_MENU_LINKS "https://www.gollahalli.com" "GAV Website")
    
    # Standard installation settings
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH ON)
    
    # Set default installation directory (Program Files - will prompt for admin)
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
endif()

## Installer settings
# DEB settings
set(CPACK_DEBIAN_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}.deb")

# RPM settings
set(CPACK_RPM_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}.rpm")

# DMG settings
set(CPACK_DMG_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}.dmg")

# TGZ settings
set(CPACK_ARCHIVE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

include(CPack)
