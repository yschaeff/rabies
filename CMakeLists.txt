cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

# A project needs a name
project(my_firmware)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(pio_ws2812)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)

# generate the header file into the source tree as it is included in the RP2040 datasheet
pico_generate_pio_header(pio_ws2812 ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

target_sources(pio_ws2812 PRIVATE main.c)

target_link_libraries(pio_ws2812 PRIVATE pico_stdlib hardware_pio)
pico_add_extra_outputs(pio_ws2812)
