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

C_INCLUDES += -Icli/stateM/Inc
CFLAGS += C_INCLUDES
export CFLAGS