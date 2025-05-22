cmake_minimum_required(VERSION 3.10)
project(dodo VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Set flags for Debug build
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb -O0 -Wall -Wextra")

# Set flags for Release build
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Sql)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/dodo.cpp
    src/Panel.cpp
    src/Model.cpp
    src/RenderTask.cpp
    src/PropertiesWidget.cpp

    src/BrowseLinkItem.hpp
    src/OutlineWidget.hpp
    src/OutlineModel.hpp
    src/GraphicsView.hpp

)

target_link_libraries(${PROJECT_NAME}
    Qt6::Widgets
    Qt6::Core
    Qt6::Sql
    poppler
    poppler-qt6
    mupdf
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
