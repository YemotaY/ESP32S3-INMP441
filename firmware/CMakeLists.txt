# ESP-IDF project for the SimonSays firmware.
#
# Build:
#   . ../esp/esp-idf/export.sh
#   idf.py -C firmware set-target esp32s3
#   idf.py -C firmware build flash monitor
#
# The portable `core` component lives at firmware/core and is shared verbatim with
# the host unit-test build (see top-level CMakeLists.txt / `make test`).
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/core")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(simonsays_fw)
