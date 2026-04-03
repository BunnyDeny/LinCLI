SUBDIRS = ./stateM
C_INCLUDES += -IstateM/Inc
C_INCLUDES_REL := $(patsubst -I%,%,$(C_INCLUDES))
C_INCLUDES_ABS := $(addprefix -I,$(abspath $(C_INCLUDES_REL)))
CFLAGS += $(C_INCLUDES_ABS)
BUILD_DIR = $(abspath ../build)
CFLAGS += -Wall 
MODULE_TOP_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))
export BUILD_DIR
export CFLAGS
export MODULE_TOP_MAKEFILE
.PHONY: all clean
all:
	@for dir in $(SUBDIRS); do $(MAKE) -s -C $$dir || exit $$?; done;
clean:
	@rm -rf $(BUILD_DIR)
