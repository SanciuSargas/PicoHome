set(IOLIB ${CMAKE_SOURCE_DIR}/external/ioLibrary_Driver)

add_executable(controller
    src/main.cpp
    src/hal/pico_pins.cpp
    src/hw_config.c
    src/sd_config_loader.cpp
    src/net/w5100_link.cpp
    src/net/mqtt_port.cpp
    src/net/net_core.cpp
    # WIZnet W5100 driver + Paho MQTT client. mqtt_interface.c is deliberately
    # replaced by src/net/mqtt_port.cpp (see the comment at the top of it).
    ${IOLIB}/Ethernet/socket.c
    ${IOLIB}/Ethernet/wizchip_conf.c
    ${IOLIB}/Ethernet/W5100/w5100.c
    ${IOLIB}/Internet/MQTT/MQTTClient.c
    ${IOLIB}/Internet/MQTT/MQTTPacket/src/MQTTConnectClient.c
    ${IOLIB}/Internet/MQTT/MQTTPacket/src/MQTTDeserializePublish.c
    ${IOLIB}/Internet/MQTT/MQTTPacket/src/MQTTFormat.c
    ${IOLIB}/Internet/MQTT/MQTTPacket/src/MQTTPacket.c
    ${IOLIB}/Internet/MQTT/MQTTPacket/src/MQTTSerializePublish.c
    ${IOLIB}/Internet/MQTT/MQTTPacket/src/MQTTSubscribeClient.c
    ${IOLIB}/Internet/MQTT/MQTTPacket/src/MQTTUnsubscribeClient.c
)

target_include_directories(controller PRIVATE
    src
    ${IOLIB}/Ethernet
    ${IOLIB}/Ethernet/W5100
    ${IOLIB}/Internet/MQTT
    ${IOLIB}/Internet/MQTT/MQTTPacket/src
)

# _WIZCHIP_ selects the chip driver (the header defaults to a W6300).
# MAX_MESSAGE_HANDLERS must cover the command wildcards plus every remote
# binding source in the config.
target_compile_definitions(controller PRIVATE _WIZCHIP_=5100 MAX_MESSAGE_HANDLERS=16)

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