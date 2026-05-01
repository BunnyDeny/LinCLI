# ============================================================
# LinCLI 外部项目 Makefile
# ============================================================

# 指定LinCLI源码根目录
LINCLI_ROOT  := $(realpath ../..)

# 外部项目配置
USER_SRCS    := $(wildcard *.c)
BUILD_DIR    := $(PWD)/build
OUTPUT       := $(BUILD_DIR)/my_app

INJECT_DIR   := $(LINCLI_ROOT)/tests

.PHONY: all clean run

all: $(OUTPUT)

$(OUTPUT): $(USER_SRCS)
	@echo "==> 注入外部源文件到 $(INJECT_DIR)..."
	@cp $(USER_SRCS) $(INJECT_DIR)/

	@echo "==> 在 LinCLI 中编译..."
	@$(MAKE) -C $(LINCLI_ROOT) build > /dev/null 2>&1 || \
		(rm -f $(INJECT_DIR)/$(notdir $(USER_SRCS)); \
		 echo "[ERR] 编译失败，已清理注入文件"; \
		 exit 1)

	@echo "==> 复制产物到本目录..."
	@mkdir -p $(BUILD_DIR)
	@cp $(LINCLI_ROOT)/build/bin/a.out $@
	@chmod +x $@

	@echo "==> 清理注入文件..."
	@rm -f $(INJECT_DIR)/$(notdir $(USER_SRCS))

	@echo "==> 完成: $@"

run: $(OUTPUT)
	@$(OUTPUT)

clean:
	@rm -rf $(BUILD_DIR)
	@rm -f $(INJECT_DIR)/$(notdir $(USER_SRCS))
	@echo "清理完成"
