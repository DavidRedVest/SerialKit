# SerialKit

跨平台串口调试助手（Qt6 / C++），核心思路是把"裸机调试"和"终端会话"两种工作流统一成**同一份原始字节流的不同视图**，而不是两个拼在一起的软件。目标平台 Windows / macOS / Linux 三端一致，当前已在 macOS 上验证。

## 功能（M1）

- 串口连接：端口枚举/刷新、波特率（含自定义）、数据位、校验位、停止位
- 接收视图：Hex / 文本两种显示模式，可选时间戳，~16ms 批量刷新（高波特率下不卡界面），发送的字节默认不显示在接收框里（避免误认为设备回环，见 `docs/ARCHITECTURE.md` "UI 交互约定" 第 11 条）
- 发送：单行/多行合并为一个发送框，Hex / +CRLF 独立开关，Enter 发送、Shift+Enter 换行，支持定时循环发送
- 文件发送：选文件、按原始字节分片发送
- 多指令宏面板（参考 XCOM/SSCOM）：10 个可命名槽位，每个独立 Hex 开关，可选参与自动循环发送，支持数字小键盘直发，配置持久化保存
- 状态栏实时收发字节计数

详细的架构决策、接口设计、每一条 UI 交互约定背后的原因（包括几次真实踩坑的教训），见 [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)。

## 路线图

M1（当前）之后计划：libvterm 终端仿真（M2）、原始日志与多会话（M3）、TCP/UDP 传输（M4）、波形视图与协议解码插件（M5）、脚本自动化引擎（M6）。里程碑范围定义见 `CLAUDE.md`。

## 构建

依赖 Qt 6.8+（Core / Widgets / SerialPort 模块）和 CMake 3.21+。

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/macos
cmake --build build -j
```

macOS 上构建产物是 `build/src/serialkit.app`（应用包，双击或 `open build/src/serialkit.app` 启动）。

CI（`.github/workflows/build.yml`）用 [`jurplel/install-qt-action`](https://github.com/jurplel/install-qt-action) 下载预编译 Qt，不依赖 vcpkg 编译 Qt 本身；`vcpkg.json` 保留给未来非 Qt 依赖（如 M2 的 libvterm）用。

## 测试

```bash
ctest --test-dir build --output-on-failure
```

核心逻辑（`RingBuffer`、`IFrameStrategy` 及其实现、`send_encoding`）是不依赖 QWidget 的纯 QtCore 类，单元测试无头运行，不需要显示器。

## 项目结构

```
src/
├── transport/   # ITransport 接口 + SerialTransport（QSerialPort 封装）
├── core/        # Session 组合根、RingBuffer、分帧策略、RawLogger
├── view/        # HexView、MainWindow、全局样式
├── send/        # 发送框、多指令宏面板、编码工具函数
└── plugin/api/  # 协议解码器 / 波形视图的占位接口，供后续里程碑扩展
tests/           # Qt Test 无头单元测试
docs/            # 架构文档
```

## 许可证

暂未开源发布，具体许可证待定。`IPlotView` 的具体实现（M5，波形视图）会依赖 GPL-3.0 的 QCustomPlot，因此在架构上被隔离在独立模块中，不影响核心代码——细节见 `docs/ARCHITECTURE.md` §1.1。
