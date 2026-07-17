add_executable(roomnode
    src/main.cpp
)

target_link_libraries(roomnode PRIVATE
    picohome_shared
    pico_stdlib
)

pico_enable_stdio_usb(roomnode 1)
pico_enable_stdio_uart(roomnode 0)
pico_add_extra_outputs(roomnode)
