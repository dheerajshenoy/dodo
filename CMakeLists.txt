cmake_minimum_required(VERSION 3.10)
project(pdf_viewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

add_executable(pdf_viewer
    src/main.cpp
    src/dodo.cpp
)

target_link_libraries(pdf_viewer
    Qt6::Widgets
    Qt6::Core
    poppler
    poppler-qt6
)

