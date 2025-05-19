cmake_minimum_required(VERSION 3.10)
project(dodo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/dodo.cpp
    src/Panel.cpp

    src/RenderTask.hpp
)

target_link_libraries(${PROJECT_NAME}
    Qt6::Widgets
    Qt6::Core
    poppler
    poppler-qt6
)

