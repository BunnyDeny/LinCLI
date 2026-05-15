# 🔌 第三方组件

> 🎯 本文档汇总 LinCLI 内置的所有第三方组件，提供快速导航～ ✨

---

## 📋 组件列表

LinCLI 内置了以下第三方组件，可通过 `make menuconfig` → `Third Party Components` 按需开启：

| 组件 | 说明 | 文档 |
|------|------|------|
| 🚀 workqueue | 轻量级工作队列，支持异步任务投递与延迟调度 | [👉 工作队列使用指南](third_party.md) |
| 🖨️ hexdump | 轻量级内存转储工具，支持十六进制 + ASCII 双列显示 | [👉 hexdump 使用指南](hexdump.md) |

---

## ⚙️ 通用开启方式

```bash
cd /path/to/LinCLI
make menuconfig
```

在菜单中进入：

```
Third Party Components
  └── [*] 选择需要的组件
```

✅ 勾选后按 `S` 保存，按 `Q` 退出，重新编译即可生效：

```bash
make
```

> 💡 各组件的详细 API、配置项和使用示例，请点击上表中的链接查看对应文档。
