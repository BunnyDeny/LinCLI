TARGET = a.out
SUBDIRS = ./cli ./lib ./init
C_INCLUDES += -Icli/stateM/Inc -Iinclude
BUILD_dir = ./build
CFLAGS += $(C_INCLUDES_ABS) -Wall 
BUILD_DIR = $(abspath $(BUILD_dir))
C_INCLUDES_REL := $(patsubst -I%,%,$(C_INCLUDES))
C_INCLUDES_ABS := $(addprefix -I,$(abspath $(C_INCLUDES_REL)))
export C_INCLUDES
export CFLAGS
export BUILD_DIR
.PHONY: all clean

all: __cc __ld
__cc:
	@for dir in $(SUBDIRS); do $(MAKE) -s -C $$dir || exit $$?; done;
__ld: __cc
	@printf "  %-7s %s\n" "LD" "$(OBJS)"
	@$(CC) $(wildcard $(BUILD_DIR)/*.o) $(LDFLAGS) -o $(BUILD_DIR)/$(TARGET) && \
	echo "Created executable:"; \
	echo "    path: $(BUILD_DIR)/$(TARGET)"; \
	echo "    type: $$(file -b $(BUILD_DIR)/$(TARGET) | cut -d',' -f1-2)"; \
	echo "    size: $$(ls -lh $(BUILD_DIR)/$(TARGET) | awk '{print $$5}')"; \
	echo "[OK]: Successfully."

clean:
	@rm -rf $(BUILD_DIR)
