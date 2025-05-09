cmake_minimum_required(VERSION 3.20)
project(pomodoro_tui LANGUAGES CXX)

# Require C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable strict warnings and pedantic mode
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

find_package(Curses REQUIRED)

add_executable(pomodoro main.cpp)
target_link_libraries(pomodoro PRIVATE ${CURSES_LIBRARIES})
target_include_directories(pomodoro PRIVATE ${CURSES_INCLUDE_DIR})
