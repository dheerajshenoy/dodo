cmake_minimum_required(VERSION 3.10)
project(dodo VERSION 0.1.0)


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Set flags for Debug build
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb -O0 -Wall -Wextra")

# Set flags for Release build
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

find_program(MUPDF mupdf)
if(MUPDF)
    message(STATUS "MuPDF library found: ${MUPDF}")
else()
    message(FATAL_ERROR "MuPDF library not found")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Sql)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/dodo.cpp
    src/Panel.cpp
    src/Model.cpp
    src/RenderTask.cpp
    src/PropertiesWidget.cpp
    src/AboutDialog.cpp
    src/GraphicsView.cpp

    src/BrowseLinkItem.hpp
    src/OutlineWidget.hpp
    src/OutlineModel.hpp
    src/ElidableLabel.hpp
    src/HighlightItem.hpp
    src/EditLastPagesWidget.hpp

)

qt_add_resources(RESOURCES resources.qrc)
target_sources(${PROJECT_NAME} PRIVATE ${RESOURCES})

target_link_libraries(${PROJECT_NAME}
    Qt6::Widgets
    Qt6::Core
    Qt6::Sql
    mupdf
    synctex
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)

install(FILES dodo.desktop DESTINATION share/applications)

# Install icons to correct freedesktop-compliant locations
install(DIRECTORY resources/hicolor
    DESTINATION share/icons
    FILES_MATCHING PATTERN "*.png")
