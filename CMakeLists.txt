cmake_minimum_required(VERSION 3.16)

project(gav VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check if in CLion
if ($ENV{CLION_IDE})
    set(QT_VERSION "6.9.*")
    message(STATUS "CLion IDE detected. Setting Qt version to ${QT_VERSION}")

    if(WIN32)
        set(USER_HOME_DIR "C:")
    else()
        set(USER_HOME_DIR $ENV{HOME})
    endif()

    file(GLOB QT_DIRS "${USER_HOME_DIR}/Qt/${QT_VERSION}")
    list(LENGTH QT_DIRS QT_DIRS_COUNT)
    if (QT_DIRS_COUNT GREATER 0)
        message(STATUS "Found ${QT_DIRS_COUNT} Qt directories starting with ${QT_VERSION}")
        list(GET QT_DIRS 0 QT_DIR)
        message(STATUS "Using Qt directory: ${QT_DIR}")
        list(APPEND CMAKE_PREFIX_PATH ${QT_DIR} ${QT_DIR}/gcc_64)
        # List all dirs in QT_DIRS/gcc_64/lib/cmake
        file(GLOB QT_CMAKE_DIRS "${QT_DIR}/gcc_64/lib/cmake/*")
        message(STATUS "CMake directories in ${QT_DIR}/gcc_64/lib/cmake:")
        foreach(dir ${QT_CMAKE_DIRS})
            message(STATUS "  - ${dir}")
            # Get the last part of the path
            get_filename_component(dir_name ${dir} NAME)
            message(STATUS "    - Directory name: ${dir_name}")
            set(${dir_name}_DIR ${dir})
        endforeach()
    else()
        message(FATAL_ERROR "No Qt directory starting with ${QT_VERSION} found in /home/akshay/Qt/")
    endif()
endif ()

find_package(Qt6 REQUIRED COMPONENTS Core Quick Multimedia Widgets)

qt_standard_project_setup(REQUIRES 6.9)

qt_add_executable(appgav
    main.cpp
)

target_compile_definitions(appgav PRIVATE APP_VERSION="${PROJECT_VERSION}")

qt_add_resources(appgav "assets"
    PREFIX "/"
    FILES
        support/installerscript.qs
        qtquickcontrols2.conf
        assets/fonts/MaterialSymbolsOutlined.ttf
        assets/images/logo.png
        assets/images/logo-bw.png
        assets/images/icon.png
        assets/images/icon-bw.png
)

qt_add_qml_module(appgav
    URI gavqml
    VERSION 1.0
    QML_FILES
        Main.qml
        MediaComponent.qml
        Debug.qml
        MediaControlsComponent.qml
        PlayListComponent.qml
        SOURCES
        custommediaplayer.h custommediaplayer.cpp
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appgav PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.gollahalli.appgav
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

set_target_properties(appgav PROPERTIES OUTPUT_NAME "gav")

target_link_libraries(appgav
    PRIVATE Qt6::Core Qt6::Quick Qt6::Multimedia Qt6::Widgets
)

include(InstallRequiredSystemLibraries)
include(GNUInstallDirs)

install(TARGETS appgav
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(UNIX)
    # For DEB package
    install(FILES assets/images/icon.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
            RENAME gav.png)
    install(FILES support/gav.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

#     # For IFW installer
#     install(FILES assets/images/icon.png DESTINATION .)
#     install(FILES support/gav.desktop DESTINATION .)
endif()

include(support/cpack.cmake)
