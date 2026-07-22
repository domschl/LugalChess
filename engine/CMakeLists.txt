cmake_minimum_required(VERSION 3.15)

set(ENGINE_SOURCES
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

set(ENGINE_HEADERS
    include/defs.h
    include/bitboard.h
    include/move.h
    include/position.h
    include/movegen.h
    include/zobrist.h
    include/tt.h
    include/evaluation.h
    include/search.h
    include/uci.h
    include/perft.h
    include/console.h
)

# Shared engine library target
add_library(lugalengine SHARED ${ENGINE_SOURCES} ${ENGINE_HEADERS})
target_include_directories(lugalengine PUBLIC include)

# Standalone CLI executable target
add_executable(lugalchess ${ENGINE_SOURCES} ${ENGINE_HEADERS})
target_include_directories(lugalchess PRIVATE include)
