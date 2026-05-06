.PHONY: all clean build run ag
all: build
build:
	@cmake -S . -B build && make -C build && ctest --test-dir build --output-on-failure
run: build
	@./build/bin/a.out
clean:
	@rm -rf build
ag: clean build
