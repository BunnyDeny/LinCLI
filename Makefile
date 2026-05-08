.PHONY: all clean build run ag menuconfig oldconfig defconfig savedefconfig mrproper
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
	@python3 tools/lincli_menuconfig.py

oldconfig:
	@python3 tools/lincli_oldconfig.py

defconfig:
	@if [ -n "$(DEFCONFIG)" ]; then \
		python3 tools/lincli_load_defconfig.py $(DEFCONFIG); \
	else \
		python3 tools/lincli_load_defconfig.py configs/lincli_defconfig; \
	fi

savedefconfig:
	@python3 -c "from kconfiglib import Kconfig; k=Kconfig('Kconfig'); k.load_config('.config'); k.write_min_config('defconfig')"
	@echo "Saved minimal defconfig"

mrproper: clean
	@rm -f .config .config.old defconfig
