# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

跨平台串口助手（**SerialKit**，代码里的包名/命名空间/CMake 项目名仍是小写 `serialkit`，两者不用改成一致）：Qt6/C++ 桌面工具，统一"MCU 裸机调试"和"Linux 终端会话"两种工作流——同一份原始字节流，多个视图（Hex / 终端仿真 / 波形 / 协议解码）。三端目标：Windows / macOS / Linux。

详细架构、接口签名、数据流、里程碑任务清单见 `docs/ARCHITECTURE.md`。设计评审的原始输入见方案文档（不在本仓库内）。**改动核心数据流或新增模块前，先读 `docs/ARCHITECTURE.md`，不要凭直觉重新发明 Session/RingBuffer/Framer 的职责边界。**

## 当前里程碑范围（M1 — 严格执行，禁止范围蔓延）

M1 目标：一个能日常替代 XCOM 的最小可用产品。

**只做**：
- Serial 传输（`ITransport` + `SerialTransport`，基于 QSerialPort）
- Hex / ASCII 双模式视图（单一视图，非 Hex 和终端并存）
- 手动发送（HEX/ASCII + 转义）、定时发送
- 端口枚举
- `Session` / `SessionManager` 骨架（会话数量恒为 1，UI 不做 Tab）
- 三端 CMake 构建跑通

**禁止在 M1 引入**：libvterm 终端仿真、QCustomPlot 波形视图、QJSEngine 脚本引擎、多 Tab UI、任何协议解码插件的具体实现（只留 `IProtocolDecoder` 占位接口 + 一个 `RawPassthroughDecoder`）。

如果任务描述看起来需要碰这些边界之外的东西，先停下来跟用户确认是否要提前挪动里程碑边界，不要默默扩大范围。

## 关键架构约束（违反会导致后续里程碑返工或许可证问题）

1. **GPL 依赖隔离**：QCustomPlot 是 GPL-3.0-or-later（本项目倾向闭源/商业化留口子）。**只允许在 `IPlotView` 的具体实现文件里 `#include` QCustomPlot 头文件**。`src/core/`、`src/transport/`、`src/send/` 下任何文件都不得直接或间接依赖 QCustomPlot。M1-M4 完全不链接这个库。
2. **不要为 QSerialPort 单开线程**：`readyRead` 已经是事件循环驱动的异步信号，不阻塞 GUI。真正的性能问题来源是"收到字节就重绘"，用批量刷新定时器（~16ms）解决，不要用 `moveToThread` 包装 `SerialTransport` 来"防止卡顿"——这是过度设计。只有在分帧/终端解析被证实是 CPU 瓶颈时，才把这部分解析下沉到 worker `QThread`，且只通过队列传递已经点好边界的数据，不跨线程传递 `QSerialPort*`。
3. **RingBuffer 是多个并列订阅者的源头，不是 Framer 的私有输入**：Hex 视图和 RawLogger 直接订阅 RingBuffer 的原始字节，不经过 Framer；Framer 的输出（`frameReady`）只喂给需要帧语义的视图（终端仿真、波形、协议解码）。不要把 Hex 视图接到 Framer 后面，否则会丢失帧边界之间的原始字节和超时半帧。
4. **Session 是组合根**：新增功能时，判断它属于 `Session`（每个会话独立的状态：Transport/RingBuffer/Framer/RawLogger/视图订阅）还是 `SessionManager`（跨会话的管理，如会话列表、Tab 切换）。不要把本该属于 Session 的状态提升成全局单例，M3 开放多会话时会需要它是可复制的。
5. **libvterm 不在 vcpkg 官方仓库**：M2 集成时用 overlay port 或 CMake `FetchContent` 源码集成，不要假设 `vcpkg install libvterm` 能跑通。

## 模型选择建议

- **架构/难点任务**（libvterm 终端状态机集成、跨线程数据管道、Framer 策略扩展、协议解码插件 ABI 设计）：建议用 Opus。
- **常规实现**（UI 绑定、简单 CRUD、已有接口的新增实现类、测试用例填充）：Sonnet 即可。

## 测试

`RingBuffer` 和 `IFrameStrategy` 及其实现必须是不依赖 QWidget 的纯 QtCore 类，用 Qt Test 做无头单元测试（三端 CI 都能跑，不需要显示器）。新增分帧策略或修改 RingBuffer 语义时，必须补充对应单元测试，尤其是环形缓冲区回绕、分帧边界、超时半帧等边界情况。
