cmake_minimum_required(VERSION 3.22)
project(Editor C)

set(CMAKE_C_STANDARD 99)
find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

add_executable(Editor main.c )
target_link_libraries(Editor ${CURSES_LIBRARIES})