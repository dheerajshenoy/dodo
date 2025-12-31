cmake_minimum_required(VERSION 3.10)
project(dodo VERSION 0.5.0 LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
add_compile_definitions(__DODO_BUILD_TYPE="${CMAKE_BUILD_TYPE}")

# Used to .desktop file generation
set(PREFIX "${CMAKE_INSTALL_PREFIX}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#Set flags for Debug build
set(CMAKE_CXX_FLAGS_DEBUG "-fno-omit-frame-pointer -ggdb3 -O0 -Wall -Wextra")

#Set flags for Release build
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

find_package(Qt6 COMPONENTS Gui Widgets Core Sql Svg SvgWidgets Concurrent REQUIRED)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/dodo.cpp
    src/Panel.cpp
    src/Model.cpp
    src/PropertiesWidget.cpp
    src/AboutDialog.cpp
    src/GraphicsView.cpp
    src/GraphicsScene.cpp
    src/DocumentView.cpp
    src/StartupWidget.cpp
    src/MessageBar.cpp
    src/Annotation.cpp
    src/EditLastPagesWidget.cpp
    src/SearchBar.cpp
    src/SaveSessionDialog.cpp
    src/utils.cpp

    src/GraphicsPixmapItem.hpp
    src/BrowseLinkItem.hpp
    src/OutlineWidget.hpp
    src/OutlineModel.hpp
    src/ElidableLabel.hpp
    src/HighlightAnnotation.hpp
    src/EditLastPagesWidget.hpp
    src/LinkHint.hpp
    src/JumpMarker.hpp
    src/CircleLabel.hpp
    src/TabWidget.hpp
    src/commands/RectAnnotationCommand.hpp
    src/commands/TextHighlightAnnotationCommand.hpp
    src/commands/DeleteAnnotationsCommand.hpp
    src/PopupAnnotation.hpp
    src/PlaceholderWidget.hpp
    src/VerticalScrollBar.hpp
)

qt_add_resources(RESOURCES resources.qrc)

# Add MuPDF headers
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/extern/mupdf/include
)

target_sources(${PROJECT_NAME} PRIVATE ${RESOURCES})

target_link_libraries(
    ${PROJECT_NAME} Qt6::Widgets Qt6::Core Qt6::Sql Qt6::SvgWidgets Qt6::Concurrent
    ${CMAKE_SOURCE_DIR}/extern/mupdf/build/release/libmupdf.a
    ${CMAKE_SOURCE_DIR}/extern/mupdf/build/release/libmupdf-third.a
    synctex)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
#Install icons to correct freedesktop - compliant locations

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dodo-template.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/dodo.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dodo.desktop DESTINATION share/applications)

install(DIRECTORY resources/hicolor DESTINATION share/icons FILES_MATCHING PATTERN "*.png")
