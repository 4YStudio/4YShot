cmake_minimum_required(VERSION 3.20)
project(4yshot VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets DBus Network Concurrent)
find_package(KF6GlobalAccel QUIET)

set(SOURCES
    src/main.cpp
    src/core/config.h
    src/core/config.cpp
    src/core/clipboard_util.h
    src/core/clipboard_util.cpp
    src/core/app.h
    src/core/app.cpp
    src/core/ocr_service.h
    src/core/ocr_service.cpp
    src/core/ocr_text_model.h
    src/core/smart_select.h
    src/core/smart_seed.h
    src/core/ocr_text_model.cpp
    src/core/smart_select.cpp
    src/core/smart_seed.cpp
    src/platform/capture_manager.h
    src/platform/capture_manager.cpp
    src/platform/window_keep_above.h
    src/platform/window_keep_above.cpp
    src/annotation/shapes.h
    src/annotation/shapes.cpp
    src/annotation/annotation_engine.h
    src/annotation/annotation_engine.cpp
    src/ui/selection_model.h
    src/ui/selection_model.cpp
    src/ui/magnifier.h
    src/ui/magnifier.cpp
    src/ui/toolbar.h
    src/ui/toolbar.cpp
    src/ui/overlay_widget.h
    src/ui/overlay_widget.cpp
    src/ui/icon_helper.h
    src/ui/icon_helper.cpp
    src/ui/ocr_dialog.h
    src/ui/ocr_dialog.cpp
    src/ui/long_capture_dialog.h
    src/ui/long_capture_dialog.cpp
    src/ui/text_selection_bubble.h
    src/ui/text_selection_bubble.cpp
    src/pin/pin_window.h
    src/pin/pin_window.cpp
    resources/4yshot.qrc
)

add_executable(4yshot ${SOURCES})

target_include_directories(4yshot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(4yshot PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    Qt6::Network
    Qt6::Concurrent
)

if(KF6GlobalAccel_FOUND)
    message(STATUS "Enabling KDE Plasma 6 KGlobalAccel support")
    target_link_libraries(4yshot PRIVATE KF6::GlobalAccel)
    target_compile_definitions(4yshot PRIVATE HAS_KF6_GLOBALACCEL)
endif()

# PP-OCR engine (ONNX Runtime): much better quality than Tesseract for
# screenshot text. Optional: falls back to Tesseract when absent.
find_library(ONNXRUNTIME_LIBRARY onnxruntime)
find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h PATH_SUFFIXES onnxruntime)
if(ONNXRUNTIME_LIBRARY AND ONNXRUNTIME_INCLUDE_DIR)
    message(STATUS "Enabling PP-OCR (ONNX Runtime) engine: ${ONNXRUNTIME_LIBRARY}")
    target_sources(4yshot PRIVATE src/core/ocr_engine.cpp)
    target_link_libraries(4yshot PRIVATE ${ONNXRUNTIME_LIBRARY})
    target_include_directories(4yshot PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
    target_compile_definitions(4yshot PRIVATE HAS_ONNXRUNTIME)
else()
    message(STATUS "ONNX Runtime not found; OCR uses the Tesseract fallback")
endif()

# Bundle PP-OCR models next to the build artifact / into the install tree.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/models/ch_PP-OCRv4_det_infer.onnx)
    add_custom_command(TARGET 4yshot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/models
                $<TARGET_FILE_DIR:4yshot>/models
        COMMENT "Copying PP-OCR models next to the build artifact"
        VERBATIM
    )
    install(DIRECTORY models/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/4yshot/models
    )
endif()

include(GNUInstallDirs)

install(TARGETS 4yshot RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES resources/4yshot.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

# Register a user-local authorization entry pointing at the dev build
# artifact so `./build/4yshot` can also use the restricted interface.
add_custom_command(TARGET 4yshot POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DFOURYSHOT_EXE=$<TARGET_FILE:4yshot>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ensure-dev-kwin-auth.cmake
    COMMENT "Registering KWin screenshot authorization for the dev build"
    VERBATIM
)

# Build a native Debian package with: cpack -G DEB
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "4yshot")
set(CPACK_PACKAGE_VENDOR "4YShot")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Wayland screenshot and annotation tool")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "4YShot maintainers")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "4YShot maintainers")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "tesseract-ocr, tesseract-ocr-chi-sim, tesseract-ocr-eng, libonnxruntime1.21")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
include(CPack)
