cmake_minimum_required(VERSION 3.15)

# Optional RISC-V architecture selection for RP2350
option(USE_RISCV "Build firmware for RP2350 Hazard3 RISC-V cores" OFF)
if(USE_RISCV)
    set(PICO_PLATFORM "rp2350-riscv" CACHE STRING "Pico platform target" FORCE)
else()
    set(PICO_PLATFORM "rp2350-arm-s" CACHE STRING "Pico platform target" FORCE)
endif()

# Include Pico SDK configuration
include(pico_sdk_import.cmake)

project(lugalchess_firmware C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

# Ensure output executables have .elf extension for picotool conversion
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# Initialize Pico SDK
pico_sdk_init()

# Define firmware executable
add_executable(lugalchess_firmware
    main.c
    tm1638.c
    st7735.c
    ../engine/src/bitboard.c
    ../engine/src/position.c
    ../engine/src/movegen.c
    ../engine/src/zobrist.c
    ../engine/src/tt.c
    ../engine/src/evaluation.c
    ../engine/src/search.c
    ../engine/src/uci.c
    ../engine/src/console.c
)

# Include search path for headers
target_include_directories(lugalchess_firmware PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../engine/include
    ${CMAKE_CURRENT_LIST_DIR}
)

# Set compile definitions (use 0 KB on-the-fly sliding attacks for embedded RAM limits)
if(USE_RISCV)
    target_compile_definitions(lugalchess_firmware PRIVATE USE_MAGIC_BITBOARDS=0 PICO_STACK_SIZE=4096 PICO_CORE1_STACK_SIZE=2048)
else()
    target_compile_definitions(lugalchess_firmware PRIVATE USE_MAGIC_BITBOARDS=0 PICO_STACK_SIZE=4096)
endif()

# Link hardware libraries
target_link_libraries(lugalchess_firmware PRIVATE
    pico_stdlib
    pico_multicore
    hardware_spi
)



# Enable USB serial output and disable UART to prevent conflicts
pico_enable_stdio_usb(lugalchess_firmware 1)
pico_enable_stdio_uart(lugalchess_firmware 0)

# Generate UF2 output file
pico_add_extra_outputs(lugalchess_firmware)

# Define hardware test executable
add_executable(test_tm1638
    test_tm1638.c
    tm1638.c
)

target_include_directories(test_tm1638 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../engine/include
)

target_link_libraries(test_tm1638 PRIVATE
    pico_stdlib
)

pico_enable_stdio_usb(test_tm1638 1)
pico_enable_stdio_uart(test_tm1638 0)
pico_add_extra_outputs(test_tm1638)
