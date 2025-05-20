cmake_minimum_required(VERSION 3.10)
project(dodo VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Sql)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/dodo.cpp
    src/Panel.cpp
    src/Model.cpp

    src/RenderTask.hpp

    src/LinkItem.hpp
    src/BrowseLinkItem.hpp
    src/GotoLinkItem.hpp

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
