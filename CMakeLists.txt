cmake_minimum_required(VERSION 3.10)
project(dodo VERSION 0.5.2 LANGUAGES CXX)


option(ENABLE_LLM_SUPPORT "Enable LLM support for advanced features" OFF)

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
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

find_package(Qt6 REQUIRED COMPONENTS Gui Widgets Core Svg SvgWidgets Concurrent)
#OpenGLWidgets )
find_package(PkgConfig REQUIRED)

pkg_check_modules(synctex QUIET synctex)

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
    src/RecentFilesStore.cpp
    src/RecentFilesModel.cpp
    src/SearchBar.cpp
    src/SaveSessionDialog.cpp
    src/FloatingOverlayWidget.cpp
    src/HighlightSearchWidget.cpp
    src/utils.cpp
    src/WaitingSpinnerWidget.cpp
    src/CommandPaletteWidget.cpp

    src/GraphicsPixmapItem.hpp
    src/BrowseLinkItem.hpp
    src/OutlineWidget.hpp
    src/OutlineModel.hpp
    src/ElidableLabel.hpp
    src/HighlightAnnotation.hpp
    src/FloatingOverlayWidget.hpp
    src/HighlightSearchWidget.hpp
    src/EditLastPagesWidget.hpp
    src/RecentFilesStore.hpp
    src/RecentFilesModel.hpp
    src/LinkHint.hpp
    src/JumpMarker.hpp
    src/CircleLabel.hpp
    src/TabWidget.hpp
    src/DraggableTabBar.hpp
    src/commands/RectAnnotationCommand.hpp
    src/commands/TextHighlightAnnotationCommand.hpp
    src/commands/DeleteAnnotationsCommand.hpp
    src/PopupAnnotation.hpp
    src/PlaceholderWidget.hpp
    src/ScrollBar.hpp
    src/CommandPaletteWidget.hpp

)


if (ENABLE_LLM_SUPPORT)
    message(STATUS "LLM support enabled.")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_LLM_SUPPORT=1)

    target_sources(${PROJECT_NAME} PRIVATE
        src/llm/http/HttpStreamClient.cpp
        src/llm/LLMWidget.cpp
        src/llm/providers/LLMProvider.cpp
        src/llm/providers/ollama/OllamaProvider.cpp
    )

    find_package(CURL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/llm
    )
else()
    message(STATUS "LLM support disabled.")
endif()


qt_add_resources(RESOURCES resources.qrc)

# Add MuPDF headers
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/extern/mupdf/include
)

target_sources(${PROJECT_NAME} PRIVATE ${RESOURCES})

if(synctex_FOUND)
    message(STATUS "Found SyncTex: ${synctex_VERSION}. Compiling with SyncTeX support.")
    target_compile_definitions(${PROJECT_NAME} PUBLIC HAS_SYNCTEX=1)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Widgets Qt6::Core Qt6::SvgWidgets Qt6::Concurrent
        ${CMAKE_SOURCE_DIR}/extern/mupdf/build/release/libmupdf.a
        ${CMAKE_SOURCE_DIR}/extern/mupdf/build/release/libmupdf-third.a
        synctex
    )
else()
    message(FATAL_ERROR "SyncTex not found. If you want SyncTex support please install it and compile again.")

    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Widgets Qt6::Core Qt6::SvgWidgets Qt6::Concurrent
        ${CMAKE_SOURCE_DIR}/extern/mupdf/build/release/libmupdf.a
        ${CMAKE_SOURCE_DIR}/extern/mupdf/build/release/libmupdf-third.a
    )
endif()


install(TARGETS ${PROJECT_NAME} DESTINATION bin)
#Install icons to correct freedesktop - compliant locations

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dodo-template.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/dodo.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dodo.desktop DESTINATION share/applications)

install(DIRECTORY resources/hicolor DESTINATION share/icons FILES_MATCHING PATTERN "*.png")
