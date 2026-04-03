SRC_DIR = .
MAKE_FILE_NAME = Makefile
SRCS = $(shell find $(SRC_DIR) -name '*.c')
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.d,$(SRCS))
CFLAGS_CLEAN := $(filter-out -MMD -MP -MF%,$(CFLAGS))
all: $(OBJS)
$(BUILD_DIR)/%.d: $(SRC_DIR)/%.c $(MAKEFILE_LIST) |$(BUILD_DIR)
	@rm -f $@;
	@printf "  %-4s %s\n" "MM" "$(abspath $<)"
	@$(CC) -MM $(CFLAGS_CLEAN) $< >$@.tmp; 
	@sed 's,\($*\)\.o[ :]*,$(BUILD_DIR)/\1.o $@ : ,g' < $@.tmp > $@; 
	@rm -f $@.tmp
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(MAKEFILE_LIST) |$(BUILD_DIR)
	@mkdir -p $(dir $@)
	@printf "  %-4s %s\n" "CC" "$(abspath $<)"
	@$(CC) $(CFLAGS_CLEAN) -c -o $@ $<
$(BUILD_DIR):
	@mkdir $@
-include $(DEPS)
