cmake_minimum_required(VERSION 3.16)
project(Sentinel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Curses REQUIRED)

add_executable(Sentinel
    main.cpp
    src/Application.cpp
    src/CommandProcessor.cpp
    src/FuzzySearch.cpp
    src/Task.cpp
    src/TaskManager.cpp
)

target_include_directories(Sentinel PRIVATE
    include
    ${CURSES_INCLUDE_DIRS}
)

target_link_libraries(Sentinel PRIVATE ${CURSES_LIBRARIES})

target_compile_options(Sentinel PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
