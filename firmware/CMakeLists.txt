cmake_minimum_required(VERSION 3.13)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(../pico-sdk/pico_sdk_init.cmake)

project(battery_control)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
add_executable(battery_control
    main.c
    tusb320.c
    control.c
)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(battery_control pico_stdlib hardware_adc hardware_i2c)

pico_enable_stdio_usb(battery_control 1)
pico_enable_stdio_uart(battery_control 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(battery_control)
