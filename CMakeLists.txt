cmake_minimum_required(VERSION 3.15)
project(LugalChess C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Optimization flags for cross-platform performance
if(MSVC)
    add_compile_options(/O2 /Oi /Ot /W4)
else()
    add_compile_options(-O3 -Wall -Wextra -Wpedantic)
endif()

# Build engine targets
add_subdirectory(engine)
