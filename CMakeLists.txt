# Host build for the portable core + unit tests (NOT the ESP-IDF firmware build).
# Firmware is built separately from ./firmware with idf.py.
cmake_minimum_required(VERSION 3.16)
project(simonsays_host C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Werror)

enable_testing()

# Portable core: the exact same sources compiled for the ESP32-S3 firmware.
add_library(core STATIC
    firmware/core/src/ringbuf.c
    firmware/core/src/power.c
    firmware/core/src/fsm.c
    firmware/core/src/app.c
    firmware/core/src/nn/quant.c
    firmware/core/src/nn/kernels.c
    firmware/core/src/dsp/fft.c
    firmware/core/src/dsp/melspec.c
    firmware/core/src/kws.c
    firmware/core/src/kws_model.c
    firmware/core/src/kws_frontend.c
    firmware/core/src/net/stream_proto.c
    firmware/core/src/net/stream_client.c
)
target_include_directories(core PUBLIC firmware/core/include)
target_link_libraries(core PUBLIC m)

add_subdirectory(server)
add_subdirectory(tools/host)
add_subdirectory(tests/host)
