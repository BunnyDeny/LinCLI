.PHONY: all clean build run ag
all: build
build:
	@cmake -S . -B build && make -C build
run: build
	@./build/bin/a.out
clean:
	@rm -rf build
ag: clean build
