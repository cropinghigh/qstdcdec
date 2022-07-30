cmake_minimum_required(VERSION 3.5)

project(qstdcdec VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check https://doc.qt.io/qt/deployment-android.html for more information.
# They need to be set before the find_package( ...) calls below.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()

find_package(QT NAMES Qt5 COMPONENTS Widgets Network REQUIRED)
find_package(Qt5 COMPONENTS Widgets Network REQUIRED)

set(PROJECT_SOURCES
        main.cpp
        qstdcdec.cpp
        qstdcdec.h
        json.hpp
        qstdcdec.ui
)

if(ANDROID)
    add_library(qstdcdec SHARED
        ${PROJECT_SOURCES}
    )
else()
    add_executable(qstdcdec
        ${PROJECT_SOURCES}
    )
endif()

target_link_libraries(qstdcdec PRIVATE Qt5::Widgets Qt5::Network PUBLIC inmarsatc_parser)

set_target_properties(qstdcdec PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(qstdcdec)
endif()
