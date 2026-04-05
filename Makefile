TARGET = a.out
SUBDIRS = ./cli ./lib ./init
C_INCLUDES += -Icli/Inc -Iinclude
BUILD_dir = ./build
CFLAGS += $(C_INCLUDES_ABS) -Wall -g
BUILD_DIR = $(abspath $(BUILD_dir))
C_INCLUDES_REL := $(patsubst -I%,%,$(C_INCLUDES))
C_INCLUDES_ABS := $(addprefix -I,$(abspath $(C_INCLUDES_REL)))
OBJS = $(wildcard $(BUILD_dir)/*.o)
OBJS_NO_PRE = $(patsubst $(BUILD_dir)/%,%,$(OBJS))
export C_INCLUDES
export CFLAGS
export BUILD_DIR
.PHONY: all clean __cc __ld run rbd
all: __cc __ld
__cc:
	@for dir in $(SUBDIRS); do $(MAKE) -s -C $$dir || exit $$?; done;
__ld: $(BUILD_DIR)/$(TARGET)
$(BUILD_DIR)/$(TARGET): $(OBJS)
	@printf "  %-4s %s\n" "LD" "$(OBJS_NO_PRE)"
	@$(CC) $(OBJS) $(LDFLAGS) -o $@ && \
	echo "Created executable:"; \
	echo "    path: $(BUILD_DIR)/$(TARGET)"; \
	echo "    type: $$(file -b $(BUILD_DIR)/$(TARGET) | cut -d',' -f1-2)"; \
	echo "    size: $$(ls -lh $(BUILD_DIR)/$(TARGET) | awk '{print $$5}')"; \
	echo "[OK]: Successfully."
clean:
	@if [ -d "$(BUILD_DIR)" ]; then rm -rf $(BUILD_DIR) && echo "cleaned: $(BUILD_DIR)"; fi
ag:
	@rm -rf $(BUILD_DIR) && $(MAKE)
run:
	@$(MAKE) && $(BUILD_DIR)/$(TARGET)
