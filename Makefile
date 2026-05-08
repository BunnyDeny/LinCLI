.PHONY: all clean build run ag menuconfig oldconfig
all: build
build:
	@cmake -S . -B build && make -C build && ctest --test-dir build --output-on-failure
run: build
	@./build/bin/a.out
clean:
	@rm -rf build
ag: clean build

# Kconfig / menuconfig targets
menuconfig:
	@python3 tools/menuconfig.py

oldconfig:
	@echo "oldconfig not yet implemented (run menuconfig to generate .config)"
