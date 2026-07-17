cmake_minimum_required(VERSION 3.15)

# Include Pico SDK configuration
include(pico_sdk_import.cmake)

project(lugalchess_firmware C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

# Initialize Pico SDK
pico_sdk_init()

# Define firmware executable
add_executable(lugalchess_firmware
    main.c
    tm1638.c
    ../src/bitboard.c
    ../src/position.c
    ../src/movegen.c
    ../src/zobrist.c
    ../src/tt.c
    ../src/evaluation.c
    ../src/search.c
    ../src/uci.c
    ../src/console.c
)

# Include search path for headers
target_include_directories(lugalchess_firmware PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../src
)

# Set compile definitions (use 0 KB on-the-fly sliding attacks for embedded RAM limits)
target_compile_definitions(lugalchess_firmware PRIVATE USE_MAGIC_BITBOARDS=0)

# Link hardware libraries
target_link_libraries(lugalchess_firmware PRIVATE
    pico_stdlib
    pico_multicore
)



# Enable USB serial output and disable UART to prevent conflicts
pico_enable_stdio_usb(lugalchess_firmware 1)
pico_enable_stdio_uart(lugalchess_firmware 0)


# Generate UF2 output file
pico_add_extra_outputs(lugalchess_firmware)
