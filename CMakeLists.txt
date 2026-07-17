cmake_minimum_required(VERSION 3.20)

# Two build flavours (see CMakePresets.json):
#  - firmware (default): cross-compiles controller + roomnode; needs PICO_SDK_PATH
#    and the arm-none-eabi toolchain
#  - host: -DPICOHOME_HOST_BUILD=ON builds the portable core library and unit tests
#    with the native compiler
option(PICOHOME_HOST_BUILD "Build the portable core and unit tests for the host instead of firmware" OFF)

if(NOT PICOHOME_HOST_BUILD)
    include(cmake/pico_sdk_import.cmake)
endif()

project(PicoHome C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/ArduinoJson)

add_subdirectory(core)
add_subdirectory(shared)

if(PICOHOME_HOST_BUILD)
    enable_testing()
    add_subdirectory(tests)
else()
    pico_sdk_init()
    add_subdirectory(controller)
    add_subdirectory(roomnode)
endif()

# Vendored but not yet wired into the build (upcoming milestones):
#   external/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico  - SD card config loading
#   external/ioLibrary_Driver                  - W5100 Ethernet + MQTT
#   external/nanomodbus                        - Modbus RTU master/slave
