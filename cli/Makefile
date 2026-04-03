SUBDIRS = ./stateM
MODULE_TOP_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))
export BUILD_DIR
export CFLAGS
export MODULE_TOP_MAKEFILE
.PHONY: all clean
all:
	@for dir in $(SUBDIRS); do $(MAKE) -s -C $$dir || exit $$?; done;
