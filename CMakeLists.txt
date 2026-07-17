cmake_minimum_required(VERSION 3.15)
project(LugalChess C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Optimization flags for performance
if(MSVC)
    add_compile_options(/O2 /Oi /Ot /W4)
else()
    add_compile_options(-O3 -Wall -Wextra -Wpedantic)
endif()

# Source files
set(SOURCES
    src/main.c
    src/bitboard.c
    src/position.c
    src/movegen.c
    src/zobrist.c
    src/tt.c
    src/evaluation.c
    src/search.c
    src/uci.c
    src/perft.c
    src/console.c
)

# Headers (for IDE/dependency tracking)
set(HEADERS
    src/defs.h
    src/bitboard.h
    src/move.h
    src/position.h
    src/movegen.h
    src/zobrist.h
    src/tt.h
    src/evaluation.h
    src/search.h
    src/uci.h
    src/perft.h
    src/console.h

)

# Executable target
add_executable(lugalchess ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(lugalchess PRIVATE src)
