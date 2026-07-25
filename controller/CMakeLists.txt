add_executable(controller
    src/main.cpp
    src/hal/pico_pins.cpp
    src/hw_config.c
    src/sd_config_loader.cpp
)

target_include_directories(controller PRIVATE src)

target_link_libraries(controller PRIVATE
    picohome_core
    picohome_shared
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
    pico_stdlib
    pico_multicore
    hardware_gpio
)

pico_enable_stdio_usb(controller 1)
pico_enable_stdio_uart(controller 0)
pico_add_extra_outputs(controller)