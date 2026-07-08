# Top-level developer entrypoints.
#
#   make test   - configure, build and run the host unit test suite
#   make build  - configure + build host targets only
#   make clean  - remove the host build directory
#
# The ESP32-S3 firmware is built separately (see firmware/README once added):
#   . ../esp/esp-idf/export.sh && idf.py -C firmware build

BUILD_DIR ?= build

.PHONY: test build clean

build:
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR)
