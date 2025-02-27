cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

# A project needs a name
project(my_firmware)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(firmware)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)

# generate the header file into the source tree as it is included in the RP2040 datasheet
pico_generate_pio_header(firmware ${CMAKE_CURRENT_LIST_DIR}/rabi.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)
pico_generate_pio_header(firmware ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

target_sources(firmware PRIVATE main.c usb_descriptors.c)

# Make sure TinyUSB can find tusb_config.h
target_include_directories(firmware PUBLIC
        ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(firmware PRIVATE pico_stdlib hardware_pio tinyusb_device tinyusb_board pico_unique_id)

## enable usb output
pico_enable_stdio_usb(firmware 1)
## disable uart output
pico_enable_stdio_uart(firmware 0)

pico_add_extra_outputs(firmware)
