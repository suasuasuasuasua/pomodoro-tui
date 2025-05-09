cmake_minimum_required(VERSION 3.25)
project(pomodoro-tui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Curses REQUIRED)

add_executable(pomodoro
    src/main.cpp
    src/pomodoro.cpp
)

target_include_directories(pomodoro PRIVATE src)

target_compile_options(pomodoro PRIVATE -Wall -Wextra -Wpedantic -Werror)

target_link_libraries(pomodoro PRIVATE ${CURSES_LIBRARIES})
