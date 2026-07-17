# This is a copy of <PicoSDK>/external/pico_sdk_import.cmake

if (NOT PICO_SDK_PATH)
    # Check environment variable
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()

if (NOT PICO_SDK_PATH)
    # If not found, download automatically using FetchContent
    message(STATUS "PICO_SDK_PATH not set; fetching Pico SDK from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        pico_sdk
        GIT_REPOSITORY https://github.com/raspberrypi/pico-sdk.git
        GIT_TAG master # tag/branch for RP2350 support
    )
    FetchContent_MakeAvailable(pico_sdk)
    set(PICO_SDK_PATH ${pico_sdk_SOURCE_DIR} CACHE PATH "Path to the Pico SDK" FORCE)
endif()

get_filename_component(PICO_SDK_PATH "${PICO_SDK_PATH}" REALPATH)
if (NOT EXISTS "${PICO_SDK_PATH}/pico_sdk_init.cmake")
    message(FATAL_ERROR "Directory '${PICO_SDK_PATH}' does not appear to contain the Pico SDK")
endif()

set(PICO_SDK_PATH "${PICO_SDK_PATH}" CACHE PATH "Path to the Pico SDK" FORCE)

# Include the sdk initialization
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)
