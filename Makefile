# BUILD_DIR = build
# SRC_DIR = ./Src
# CFLAGS += -IInc
# TARGET = stateM
# SRCS = $(shell find $(SRC_DIR) -name '*.c')
# OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
# DEPS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.d,$(SRCS))
# CFLAGS_CLEAN := $(filter-out -MMD -MP -MF%,$(CFLAGS))
# all: $(BUILD_DIR)/$(TARGET)
# $(BUILD_DIR)/%.d: $(SRC_DIR)/%.c Makefile |$(BUILD_DIR)
# 	@rm -f $@;
# 	@printf "  %-7s %s\n" "MM" "$(abspath $<)"
# 	@$(CC) -MM $(CFLAGS_CLEAN) $< >$@.tmp;
# 	@sed 's,\($*\)\.o[ :]*,$(BUILD_DIR)/\1.o $@ : ,g' < $@.tmp > $@;
# 	@rm -f $@.tmp
# $(BUILD_DIR)/%.o: $(SRC_DIR)/%.c Makefile |$(BUILD_DIR)
# 	@mkdir -p $(dir $@)
# 	@printf "  %-7s %s\n" "CC" "$(abspath $<)"
# 	@$(CC) $(CFLAGS_CLEAN) -c -o $@ $<
# $(BUILD_DIR)/$(TARGET): $(OBJS)
# 	@printf "  %-7s %s\n" "LD" "$(OBJS)"
# 	@$(CC) $(wildcard $(BUILD_DIR)/*.o) $(LDFLAGS) -o $@ && \
# 	echo "Created executable:"; \
# 	echo "    path: $(BUILD_DIR)/$(TARGET)"; \
# 	echo "    type: $$(file -b $@ | cut -d',' -f1-2)"; \
# 	echo "    size: $$(ls -lh $@ | awk '{print $$5}')"; \
# 	echo "[OK]: Successfully."
# $(BUILD_DIR):
# 	@mkdir $@
# .PHONY: clean
# clean:
# 	rm -rf $(BUILD_DIR)
# -include $(DEPS)
SUBDIRS = ./cli
C_INCLUDES += -Icli/stateM/Inc
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
__ld:

clean:
	@rm -rf $(BUILD_DIR)
