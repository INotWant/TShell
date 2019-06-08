cmake_minimum_required(VERSION 3.14)
project(TShell C)

set(CMAKE_C_STANDARD 99)

aux_source_directory(. DIR_ROOT)

add_subdirectory(cmd)
add_subdirectory(util)

add_executable(TShell ${DIR_ROOT})

target_link_libraries(TShell cmd util readline)