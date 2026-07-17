# Headers shared between the controller and roomnode firmware targets
# (Modbus register maps — the single source of truth for the bus protocol).
add_library(picohome_shared INTERFACE)
target_include_directories(picohome_shared INTERFACE include)