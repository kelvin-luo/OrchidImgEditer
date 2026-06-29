cmake_minimum_required(VERSION 3.20)

project(kImgEdit
    VERSION 1.0.0
    DESCRIPTION "kImgEdit - a lightweight image annotation/editing tool"
    LANGUAGES CXX
)

# Output layout (relative to project root, one level above code/)
get_filename_component(KIMG_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${KIMG_PROJECT_ROOT}/msvc_release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${KIMG_PROJECT_ROOT}/msvc_release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${KIMG_PROJECT_ROOT}/build_msvc")

# ============ Global compile settings ============
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(MSVC)
    add_compile_options(/utf-8 /W3 /permissive- /MP)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX UNICODE _UNICODE)
endif()

# ============ Qt6 ============
# Allow override via -DQt6_DIR=... or CMAKE_PREFIX_PATH on command line
if(NOT DEFINED Qt6_DIR AND NOT DEFINED ENV{Qt6_DIR})
    set(_default_qt "D:/Qt6/6.9.1/msvc2022_64")
    if(EXISTS "${_default_qt}/lib/cmake/Qt6/Qt6Config.cmake")
        list(APPEND CMAKE_PREFIX_PATH "${_default_qt}")
        message(STATUS "kImgEdit: using default Qt6 at ${_default_qt}")
    endif()
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Svg)

# ============ OpenCV ============
if(NOT DEFINED OpenCV_DIR AND NOT DEFINED ENV{OpenCV_DIR})
    set(_default_ocv "D:/win10/opencv500/build")
    if(EXISTS "${_default_ocv}/OpenCVConfig.cmake")
        set(OpenCV_DIR "${_default_ocv}")
        message(STATUS "kImgEdit: using default OpenCV at ${_default_ocv}")
    endif()
endif()

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

# ============ Sources ============
set(KIMG_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/ImageCanvas.cpp
    src/ImageCanvas.h
    src/Tools.h
    src/OcrService.cpp
    src/OcrService.h
)

set(KIMG_RESOURCES
    resources/resources.qrc
)

# Windows: bundle app icon via .rc file (only when an .ico exists)
set(KIMG_WIN_RC "")
if(WIN32)
    set(_ico_file "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.ico")
    if(EXISTS "${_ico_file}")
        set(_rc_file "${CMAKE_CURRENT_BINARY_DIR}/kimgedit_app.rc")
        # Forward slashes for RC paths
        string(REPLACE "\\" "/" _ico_norm "${_ico_file}")
        file(WRITE "${_rc_file}" "IDI_ICON1 ICON DISCARDABLE \"${_ico_norm}\"\n")
        set(KIMG_WIN_RC "${_rc_file}")
        message(STATUS "kImgEdit: embedding app icon ${_ico_file}")
    else()
        message(STATUS "kImgEdit: no app.ico found; window icon comes from SVG only")
    endif()
endif()

add_executable(kImgEdit WIN32
    ${KIMG_SOURCES}
    ${KIMG_RESOURCES}
    ${KIMG_WIN_RC}
)

target_include_directories(kImgEdit PRIVATE src)

target_link_libraries(kImgEdit PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    ${OpenCV_LIBS}
)

# ============ Install / Deploy ============
# Use windeployqt to copy Qt runtime DLLs next to the exe after build
if(WIN32)
    set(_windeployqt "${Qt6_DIR}/../../../bin/windeployqt.exe")
    if(NOT EXISTS "${_windeployqt}")
        # Fallback: try to derive from Qt6Core target location
        get_target_property(_qt_core_loc Qt6::Core LOCATION)
        get_filename_component(_qt_bin_dir "${_qt_core_loc}" DIRECTORY)
        set(_windeployqt "${_qt_bin_dir}/windeployqt.exe")
    endif()

    if(EXISTS "${_windeployqt}")
        add_custom_command(TARGET kImgEdit POST_BUILD
            COMMAND "${_windeployqt}"
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    --no-quick-import
                    "$<TARGET_FILE:kImgEdit>"
            COMMENT "Running windeployqt for kImgEdit"
        )
    else()
        message(WARNING "kImgEdit: windeployqt not found at ${_windeployqt}; Qt DLLs must be on PATH at runtime")
    endif()

    # Copy required OpenCV DLLs next to the exe
    set(_opencv_bin_candidates
        "${OpenCV_DIR}/x64/vc17/bin"
        "${OpenCV_DIR}/x64/vc16/bin"
        "${OpenCV_DIR}/bin"
    )
    set(_opencv_bin_dir "")
    foreach(_candidate IN LISTS _opencv_bin_candidates)
        if(EXISTS "${_candidate}")
            set(_opencv_bin_dir "${_candidate}")
            break()
        endif()
    endforeach()
    if(_opencv_bin_dir)
        file(GLOB _opencv_world_dlls "${_opencv_bin_dir}/opencv_world*.dll")
        foreach(_dll IN LISTS _opencv_world_dlls)
            add_custom_command(TARGET kImgEdit POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll}" "$<TARGET_FILE_DIR:kImgEdit>")
        endforeach()
        message(STATUS "kImgEdit: OpenCV runtime DLLs from ${_opencv_bin_dir}")
    else()
        message(WARNING "kImgEdit: OpenCV bin dir not found under ${OpenCV_DIR}; add OpenCV DLLs to PATH at runtime")
    endif()
endif()

# ============ Status ============
message(STATUS "kImgEdit ===========================================")
message(STATUS "  Qt6 version : ${Qt6_VERSION}")
message(STATUS "  OpenCV ver. : ${OpenCV_VERSION}")
message(STATUS "  OpenCV libs : ${OpenCV_LIBS}")
message(STATUS "  Output dir  : ${CMAKE_BINARY_DIR}")
message(STATUS "====================================================")
