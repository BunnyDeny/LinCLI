SUBDIRS = ./stateM
export BUILD_DIR
export CFLAGS
.PHONY: all
all:
	@for dir in $(SUBDIRS); do $(MAKE) -s -C $$dir || exit $$?; done;
