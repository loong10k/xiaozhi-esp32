# AgentScope 嵌入式组件接入状态

当前只接入**可选构建依赖**，尚未把 AgentScope 运行时接到小智语音、MCP 或音频主链，不能据此声明迁移完成或生产就绪。此开关默认关闭，小智仍保留自己的唯一 `app_main`。

构建前将 `AGENTSCOPE_CPP_ROOT` 设为 `agentscope-cpp` 的绝对路径，再在 menuconfig 中启用 `Xiaozhi Assistant → Enable embedded AgentScope component`。启用后 `main` 链接 `agentscope-framework`，由它引入 Core 与 Extensions；路径缺失会在 CMake 配置期报错。禁用时无需设置该环境变量。

```powershell
$env:AGENTSCOPE_CPP_ROOT = 'E:\workspaces\workspace-agent-fabric\agentscope-cpp'
# 使用本仓库 scripts/build.py 选择实际板型和变体；须先配置 ESP-IDF 6.0.1 或 6.1。
```

下一步须将单拥有者 Agent 适配器接入 `Application::Schedule`、真实语音事件、现有入站 MCP 路由及 `SendMcpMessage`；旧轮网络/工具/TTS/DMA 全部收口后才释放语音运行。不可编入 `agentscope-cpp/examples/xiaozhi_esp32/main/app_main.cc`，其独立示例入口会与小智入口冲突。详细验收见 AgentScope C++ 的 OpenSpec `complete-esp32-equivalent-migration` 任务 24.4。

本地仅用 CMake stub 对外部组件路径做了配置期正/反验证；这**不是** ESP-IDF 固件编译。小智最低要求 ESP-IDF 6.0.1，推荐 6.1；启用/禁用固件链接、S3 实板语音与 MCP、资源和长稳验收均未执行。
