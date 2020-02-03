cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(CSnake VERSION 0.1.0 LANGUAGES C)

set (CMAKE_C_FLAGS "-lSDL2")

add_executable(CSnake main.c)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
target_link_libraries(CSnake ${SDL2_LIBRARIES})


