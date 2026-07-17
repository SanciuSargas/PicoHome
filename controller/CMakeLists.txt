add_executable(controller
    src/main.cpp
    src/hal/pico_pins.cpp
)

target_include_directories(controller PRIVATE src)

target_link_libraries(controller PRIVATE
    picohome_core
    picohome_shared
    pico_stdlib
    pico_multicore
    hardware_gpio
)

pico_enable_stdio_usb(controller 1)
pico_enable_stdio_uart(controller 0)
pico_add_extra_outputs(controller)