# SerialKit

跨平台串口调试助手（Qt6 / C++），核心思路是把"裸机调试"和"终端会话"两种工作流统一成**同一份原始字节流的不同视图**，而不是两个拼在一起的软件。目标平台 Windows / macOS / Linux 三端一致，当前已在 macOS 上验证。

## 功能

**M1**：
- 串口连接：端口枚举/刷新、波特率（含自定义）、数据位、校验位、停止位
- Hex 接收视图：Hex / 文本两种显示模式，可选时间戳，~16ms 批量刷新（高波特率下不卡界面），发送的字节默认不显示在接收框里（避免误认为设备回环，见 `docs/ARCHITECTURE.md` "UI 交互约定" 第 11 条）
- 发送：单行/多行合并为一个发送框，Hex / +CRLF 独立开关，Enter 发送、Shift+Enter 换行，支持定时循环发送
- 文件发送：选文件、按原始字节分片发送
- 多指令宏面板（参考 XCOM/SSCOM）：10 个可命名槽位，每个独立 Hex 开关，可选参与自动循环发送，支持数字小键盘直发，配置持久化保存
- 状态栏实时收发字节计数

**M2**：
- Terminal 接收视图（libvterm）：和 Hex 视图是最外层的两个 Tab，两个视图同时订阅同一份原始字节流，切 Tab 不丢数据；Hex Tab 页保留 Receive+Send 两个分组，Terminal Tab 页只有终端本身，不占用 Send 区域的空间
- VT100/xterm 转义序列解析：颜色（16/256/真彩色）、粗体/斜体/下划线/删除线/反显、光标、基础回滚缓冲（鼠标滚轮翻）
- 键盘输入转发：方向键、Ctrl+字母组合、Backspace/Tab/Enter/Esc/Home/End/PageUp/PageDown
- Terminal 字体缩放：`Ctrl/Cmd +` / `Ctrl/Cmd -` / `Ctrl/Cmd 0`，字号持久化保存

**M3**：
- 多会话/多 Tab：外层 `QTabWidget` 管理若干个 `SessionPanel`，每个 Tab 独立的连接、Hex/Terminal 视图、发送框、宏面板，"+" 新增、Tab 可关闭，默认预置 1 个 Tab
- 原始日志开关：连接后可选文件开始/停止记录，落盘原始字节的十六进制 + 时间戳
- macOS 端口选择列表过滤（延续自 M2）：只显示 `cu.*` 可用串口

详细的架构决策、接口设计、每一条 UI 交互约定背后的原因（包括几次真实踩坑的教训），见 [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)。

## 路线图

M1、M2、M3（当前）之后计划：TCP/UDP 传输（M4）、波形视图与协议解码插件（M5）、脚本自动化引擎（M6）。里程碑范围定义见 `CLAUDE.md`。

## 构建

依赖 Qt 6.8+（Core / Widgets / SerialPort 模块）、CMake 3.21+、能联网（首次配置会用 `FetchContent` 拉取 libvterm 源码，见 `cmake/Libvterm.cmake`）。

推荐用 [CMake Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)，命令行和 VS Code CMake Tools 用同一套配置，不会再因为默认生成器不一致（Ninja vs Unix Makefiles）互相打架——这是 M2 开发时真实踩过的坑，见 `docs/ARCHITECTURE.md` "M2 开发过程中的构建系统事故"。

第一次在新机器上用，先建一份本机专用的 `CMakeUserPresets.json`（不提交进仓库，填你自己的 Qt 安装路径）：

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "default",
      "inherits": "ninja",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "/path/to/Qt/6.8.3/macos"
      }
    }
  ],
  "buildPresets": [{ "name": "default", "configurePreset": "default" }],
  "testPresets": [{ "name": "default", "configurePreset": "default", "output": { "outputOnFailure": true } }]
}
```

然后：

```bash
cmake --preset default
cmake --build --preset default -j
```

macOS 上构建产物是 `build/src/serialkit.app`（应用包，双击或 `open build/src/serialkit.app` 启动）。

CI（`.github/workflows/build.yml`）用 [`jurplel/install-qt-action`](https://github.com/jurplel/install-qt-action) 下载预编译 Qt，不依赖 vcpkg 编译 Qt 本身；`vcpkg.json` 保留，libvterm 最终没有走 vcpkg 这条路（见 `docs/ARCHITECTURE.md` §1.2）。

## 测试

```bash
ctest --preset default
```

核心逻辑（`RingBuffer`、`IFrameStrategy` 及其实现、`send_encoding`、`VtermEngine`）单元测试无头运行，不需要显示器；`VtermEngine` 的测试是真的喂 VT100 转义序列进去验证渲染结果，不是空跑。

## 项目结构

```
src/
├── transport/   # ITransport 接口 + SerialTransport（QSerialPort 封装）
├── core/        # Session 组合根、RingBuffer、分帧策略、RawLogger
├── terminal/    # VtermEngine（libvterm 的 Qt 包装，M2）
├── view/        # HexView、TerminalView、MainWindow、全局样式
├── send/        # 发送框、多指令宏面板、编码工具函数
└── plugin/api/  # 协议解码器 / 波形视图的占位接口，供后续里程碑扩展
tests/           # Qt Test 无头单元测试
docs/            # 架构文档
cmake/           # 第三方依赖集成（libvterm）
```

## 许可证

暂未开源发布，具体许可证待定。`IPlotView` 的具体实现（M5，波形视图）会依赖 GPL-3.0 的 QCustomPlot，因此在架构上被隔离在独立模块中，不影响核心代码——细节见 `docs/ARCHITECTURE.md` §1.1。
