.PHONY: all clean build run ag menuconfig oldconfig defconfig savedefconfig mrproper sync-kconfig
all: build
build:
	@cmake -S . -B build && make -C build && ctest --test-dir build --output-on-failure
run:
	@if [ ! -d "build" ]; then \
		echo "Please run 'make' to build the project first."; \
		exit 1; \
	fi
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

# 支持 make xxx_defconfig 自动查找 configs/ 目录下的对应文件
%_defconfig:
	@if [ -f "configs/$@" ]; then \
		python3 tools/lincli_load_defconfig.py configs/$@; \
	else \
		echo "Error: configs/$@ not found"; \
		exit 1; \
	fi

savedefconfig:
	@python3 -c "from kconfiglib import Kconfig; k=Kconfig('Kconfig'); k.load_config('.config'); k.write_min_config('defconfig')"
	@echo "Saved minimal defconfig"

mrproper: clean
	@rm -f .config .config.old defconfig

sync-kconfig:
	@python3 tools/config_to_header.py configs/lincli_defconfig examples/stm32_g431/cli_kconfig.h
	@python3 tools/config_to_header.py configs/lincli_defconfig examples/stm32f103_keil/cli_kconfig.h
	@python3 tools/config_to_header.py configs/lincli_defconfig examples/pc_linux/cli_kconfig.h
	@python3 tools/config_to_header.py configs/lincli_defconfig examples/external_demo/cli_kconfig.h
	@echo "Synced cli_kconfig.h to all example projects from configs/lincli_defconfig"
