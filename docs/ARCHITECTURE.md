# 架构设计文档（编码前定案）

项目英文名定为 **SerialKit**（Finder/Dock/窗口标题显示这个名字）；代码里的 CMake 项目名、C++ namespace、可执行文件名、QSettings 组织名/应用名沿用小写 `serialkit`——特意不做这层改动，避免影响已经存到 `QSettings` 里的用户配置（比如 Macros 面板存的指令）。

本文档是对原始方案设计（`串口助手方案设计.md`）的评审结论和精化架构，作为后续所有里程碑编码的依据。原方案的定位、总体分层思路、技术选型对比表（libvterm vs QTermWidget vs xterm.js 等）予以保留，不重复摘抄；本文档只记录**评审中发现的问题、做出的架构决定、以及原方案没有细化到的接口/目录层面的内容**。

## 1. 评审发现与结论

### 1.1 QCustomPlot 许可证与 GPL 隔离（关键决定）

- 核对确认：vcpkg 的 `qcustomplot` 端口是 **GPL-3.0-or-later**，商业授权需直接联系作者付费。
- 本项目发布模式：**闭源/商业化留口子**（用户已确认）。
- 决定：绘图功能全程通过 `IPlotView` 接口访问，具体实现（基于 QCustomPlot 或未来可能的自绘 QPainter 方案）放在单独的、可选编译的模块里。**M1-M4 完全不引入 QCustomPlot 依赖**，只有到 M5 真正开始做波形视图时才决定是否接受 GPL 或换成自绘方案。核心代码（`src/core`、`src/transport`、`src/send`）任何时候都不得依赖它。
- 附带说明：Qt6 Widgets/SerialPort 本身在 LGPLv3 下动态链接闭源发布是可行的行业惯例，需保证动态链接（不静态链接 Qt）并提供重新链接说明；如果后续要上 Mac App Store / 做商业授权谈判，这一点需要重新确认。

### 1.2 libvterm 打包方式修正

- 核对确认：vcpkg 官方仓库**没有** libvterm 的 port。
- 原方案"libvterm、QCustomPlot 走包管理"这句不成立。
- 决定（M2 启动时敲定，overlay port 方案确认放弃）：`cmake/Libvterm.cmake` 用 `FetchContent_Declare` + `FetchContent_Populate` 拉取 `neovim/libvterm`（钉死 commit `934bc2fbf21800ac3458a499df8820ca5fb45fd3`），手动 glob `src/*.c` 定义一个 `add_library(vterm_c STATIC ...)`。放弃 vcpkg overlay port 的原因：实测确认 libvterm 自己没有 CMakeLists.txt（用手写 Makefile），`FetchContent_MakeAvailable` 会因为找不到 `CMakeLists.txt` 报错，必须手动 populate+编译；这条路径比写一个 vcpkg overlay port 更简单可控，且 Unicode 编码表（`src/encoding/*.inc`）已经生成好提交在仓库里，不需要在构建时跑 Perl。License 是 MIT，不需要像 QCustomPlot 那样做 GPL 隔离。

### 1.3 线程模型简化

- `QSerialPort::readyRead` 是 Qt 事件循环驱动的异步信号，不阻塞 GUI 线程本身。原方案"接收线程/异步读取"的表述容易让人以为需要给 I/O 单开 `QThread`，这是不必要的复杂度来源（`moveToThread` 会引入跨线程信号槽的生命周期管理负担）。
- 真正的卡顿来源是"收到字节就重绘 UI"，原方案 §4 的 ~16ms 批量刷新定时器已经对症下药，予以保留。
- 决定：**M1-M3 不给 `SerialTransport` 单开线程**，保持在 GUI 线程、信号驱动。仅当分帧/终端解析（CPU 密集型工作）被性能测试证实拖慢主线程时，才把这部分解析下沉到 worker `QThread`；跨线程只传递"已经点好边界的数据"（如 `QByteArray` 或 `Frame` 值对象），不传递 `QSerialPort*` 或其他有线程亲和性的 Qt 对象。

### 1.4 RingBuffer 的多消费者语义

- 原设计图里 `Framer` 是所有视图的唯一上游，这对终端仿真/波形/协议解码视图是对的（它们需要帧语义），但 **Hex 视图和日志应该看到未经分帧加工的原始字节**——包括帧边界之间的噪声字节、超时产生的半帧。如果 Hex 视图接在 Framer 后面，这些信息会丢失，等于"Hex 视图"名不副实。
- 决定：RingBuffer 是**并列的多播源**，不是 Framer 的私有输入：
  - `RingBuffer.bytesAppended(QByteArray)` → Hex 视图、RawLogger（原始字节，不经过分帧）
  - `RingBuffer` 的数据同时喂给 `IFrameStrategy.feed()` → `frameReady(Frame)` → 终端仿真 / 波形 / 协议解码视图（需要帧语义的消费者）

### 1.5 Session 组合根提前到 M1

- 原方案把 `SessionManager`（多会话/多 Tab）放在 M3。但"一个会话 = 一个 Transport + 一个 RingBuffer + 一个 Framer + 一个 RawLogger + N 个视图订阅"这个组合关系如果不在 M1 就定下来，M1 的代码大概率会写成隐式单例（全局的 transport、全局的 buffer），M3 开放多会话时就要把这些全局状态拆出来重构。
- 决定：M1 就实现 `Session` 类作为唯一的组合根，`SessionManager` 管理 `vector<unique_ptr<Session>>`，M1 阶段这个 vector 恒长为 1，UI 上不做 Tab 切换。M3 只需要把"恒为 1"这个限制放开并加 Tab UI，不需要改 `Session` 内部结构。

### 1.6 协议解码插件接口提前定义

- 原方案 M5 才做协议解码插件，但 Framer 的"协议驱动分帧"策略（原方案 §4 四种分帧策略之一）从设计上就依赖一个解码器来判定帧边界，如果接口到 M5 才设计，`IFrameStrategy` 的签名届时可能要改，影响已经写好的定长/分隔符/超时三种实现。
- 决定：M1 就定义 `IProtocolDecoder` 占位接口（见 §3），只提供一个 `RawPassthroughDecoder` 实现。M5 只是新增实现类，不改接口签名。是否做成 `QPluginLoader` 动态加载 `.so/.dll`，还是保持静态链接的策略类，延后到 M5 决定——这只影响插件的**分发形态**，不影响 M1-M4 的接口设计。

### 1.7 测试策略（原方案缺失）

- 这个工具的核心卖点是数据管道正确性（分帧边界、CRC 校验、自动化回归脚本），但原方案完全没提测试策略。
- 决定：`RingBuffer`、`IFrameStrategy` 及其所有实现必须是**不依赖 QWidget 的纯 QtCore 类**，可以用 Qt Test 做无头单元测试，三端 CI 都能跑（不需要显示器/虚拟 framebuffer）。集成测试（真实分帧场景）用虚拟串口对：Linux/macOS 用 `socat` 建 pty pair，Windows 用 com0com；先在 `tests/` 目录占位，不强求 M1 的 CI 就跑通集成测试。

## 2. 数据流（并列订阅模型）

```
ITransport (SerialTransport)
      │ raw bytes
      ▼
  RingBuffer ── bytesAppended(QByteArray) ──► HexView
      │                                  ├──► TerminalView（M2，见下方修正）
      │                                  └──► RawLogger（始终记录，不受视图影响）
      │ raw bytes
      ▼
  IFrameStrategy.feed()
      │ frameReady(Frame)
      ▼
  {IPlotView(M5) / IProtocolDecoder(M5)}
```

**M2 修正**：最初这张图把 `TerminalView` 画成 `frameReady` 的下游，M2 实际开发时发现这是错的——终端会话要的是连续、不经过任何分帧语义加工的原始字节流，"分隔符/超时半帧"这些概念对交互式终端毫无意义，而且 `Session` 同一时间只有一个激活的 `IFrameStrategy`，用户为了 Hex 视图切分帧策略会把 Terminal 视图的字节流切碎。所以 `TerminalView` 和 `HexView`、`RawLogger` 一样，是 `RingBuffer` 的并列订阅者，不经过 Framer。

UI 侧一律通过 ~16ms 批量定时器 repaint：各视图内部维护自己的"待渲染增量"队列，定时器触发时才真正绘制，避免每字节/每帧都触发一次重绘。

## 3. 核心接口（M1 落地，纯声明）

```cpp
// transport/itransport.h
class ITransport : public QObject {
    Q_OBJECT
public:
    virtual bool open(const QVariantMap& params) = 0;
    virtual void close() = 0;
    virtual qint64 write(const QByteArray& data) = 0;
    virtual bool isOpen() const = 0;
signals:
    void bytesReceived(const QByteArray& data);
    void errorOccurred(const QString& message);
};

// core/frame_strategy.h
struct Frame {
    QByteArray payload;
    QDateTime timestamp;
};

class IFrameStrategy : public QObject {
    Q_OBJECT
public:
    virtual void feed(const QByteArray& data) = 0;
    virtual void reset() = 0;
signals:
    void frameReady(const Frame& frame);
};
// M1 实现：DelimiterFrameStrategy、PassthroughFrameStrategy
// M5 实现：ProtocolDrivenFrameStrategy（内部持有 IProtocolDecoder）

// plugin/api/iprotocol_decoder.h（占位接口，M1 只有 RawPassthroughDecoder）
class IProtocolDecoder {
public:
    virtual ~IProtocolDecoder() = default;
    virtual QString name() const = 0;
    virtual std::optional<Frame> tryDecode(const QByteArray& buffered) = 0;
};

// plugin/api/iplot_view.h（占位接口，M1-M4 不引入任何实现，不链接 QCustomPlot）
class IPlotView {
public:
    virtual ~IPlotView() = default;
    virtual void pushSample(qint64 timestampMs, double value) = 0;
};
```

## 4. 目录结构

```
serialkit/
├── CMakeLists.txt
├── CLAUDE.md
├── docs/
│   └── ARCHITECTURE.md
├── vcpkg.json                      # M1: qtbase, qtserialport 只有这两个
├── src/
│   ├── transport/
│   │   ├── itransport.h
│   │   └── serial_transport.{h,cpp}
│   ├── core/
│   │   ├── session.{h,cpp}                    # 组合根
│   │   ├── session_manager.{h,cpp}            # M1 恒长 1
│   │   ├── ring_buffer.{h,cpp}                # 纯 QtCore，可无头测试
│   │   ├── frame_strategy.h
│   │   ├── delimiter_frame_strategy.{h,cpp}
│   │   ├── passthrough_frame_strategy.{h,cpp}
│   │   └── raw_logger.{h,cpp}
│   ├── view/
│   │   └── hex_view.{h,cpp}        # M1 只有 Hex/ASCII 双模式，无终端/波形
│   ├── send/
│   │   ├── manual_send.{h,cpp}
│   │   └── timed_send.{h,cpp}
│   ├── plugin/
│   │   └── api/
│   │       ├── iprotocol_decoder.h
│   │       └── iplot_view.h
│   └── main.cpp
├── tests/
│   ├── test_ring_buffer.cpp
│   └── test_frame_strategy.cpp
└── .github/workflows/build.yml     # M1: 三端构建 + 无头单元测试
```

## 5. 里程碑（沿用原方案 M1-M6，M1 任务清单具体化）

M2-M6 范围和原方案一致（libvterm 终端仿真 / 多 Tab+宏+日志 / TCP+UDP / 波形+协议解码插件 / 脚本引擎），此处只展开 M1。**M3 的"多命令宏"这一项已提前在 M1 阶段做完**（`src/send/macro_panel.h/.cpp`），原因是用户直接点名要这个功能，不是按里程碑顺序推进——M3 到时候不用再重做，只需要在 `SessionManager` 开放多会话后确认宏面板是否要按会话独立还是全局共用即可。

- [x] `ITransport` + `SerialTransport`（QSerialPort 封装，open/close/write/bytesReceived）
- [x] `RingBuffer`（纯 QtCore，环形缓冲 + `bytesAppended` 信号）
- [x] `IFrameStrategy` + `DelimiterFrameStrategy` + `PassthroughFrameStrategy`
- [x] `IProtocolDecoder` 占位接口 + `RawPassthroughDecoder`
- [x] `IPlotView` 占位接口（无实现，无 QCustomPlot 依赖）
- [x] `RawLogger`（订阅 RingBuffer，落盘原始字节 + 时间戳）
- [x] `Session` 组合根 + `SessionManager`（恒长 1）
- [x] `HexView`（Hex 显示/时间戳复选框，连续流式渲染，16ms 批量刷新；TX 默认不显示，见下方"UI 交互约定"）
- [x] 发送框（单行/多行合并为一个，见下方"UI 交互约定"）：Hex/+CRLF 复选框，看到啥发啥不做转义解析，Enter 发送/Shift+Enter 换行，依附其上的定时发送 + 文件发送（原始字节，4KB 分片）
- [x] 多指令宏面板（`MacroPanel`，10 槽位，从 M3 提前，详见上方说明）
- [x] 端口枚举（`QSerialPortInfo`）+ 数据位/校验位/停止位下拉框
- [x] 全局 QSS 视觉打磨（`view/app_style.h/.cpp`，Fusion style）
- [x] `tests/test_ring_buffer.cpp`、`tests/test_frame_strategy.cpp`、`tests/test_send_encoding.cpp`（Qt Test，无头，本地跑通）
- [x] CMake 构建通过（本地用 Qt 6.8.3 + `CMAKE_PREFIX_PATH`，未依赖 vcpkg）；GitHub Actions CI 用 `jurplel/install-qt-action` 跑三端构建 + 无头单元测试

**实现备注（与本文档 §3/§4 的差异）**：
- CI 没有用 vcpkg 拉取 Qt 本身——vcpkg 的 `qtbase` 端口是从源码编译 Qt，CI 太慢——改用 `jurplel/install-qt-action` 下载预编译 Qt 6.8。`vcpkg.json` 保留，未来如果有真正需要 vcpkg 管理的依赖再用；libvterm 最终没有走这条路（见 §1.2）。
- 本地开发环境没有装 vcpkg，直接用系统里的 Qt 6.8.3（`~/Qt/6.8.3/macos`）+ `-DCMAKE_PREFIX_PATH` 配置构建；CMakeLists.txt 本身不绑定任何一种拉取方式，两条路径都能走通。
- `Session::send()` / `Session::bytesSent` 信号是接口大纲之外新增的一处：`ITransport` 本身不感知"已发送"概念，由 `Session` 包一层，`HexView` 的 TX 着色和未来的日志/统计都订阅这个信号，而不是直接调用 `transport()->write()`。

### M2：libvterm 终端仿真

- [x] `cmake/Libvterm.cmake`：FetchContent 拉取 `neovim/libvterm`（钉死 commit，见 §1.2），手动 `add_library(vterm_c ...)`，根 `CMakeLists.txt` 加 `LANGUAGES C CXX`（libvterm 是纯 C99）。
- [x] `src/terminal/vterm_engine.h/.cpp`：`VtermEngine`，包一层 libvterm C API——`feed`/`resize`/`reset`/`sendKeyEvent`/`cellAt`/`scrollbackLine`，回调用"静态 trampoline 转 this 指针"模式接进 Qt 信号（`damaged`/`cursorMoved`/`bell`/`titleChanged`/`dataToSend`/`scrollbackChanged`）。颜色统一用 `vterm_screen_convert_color_to_rgb` 转成 RGB 再读，不用自己维护 256 色调色板。回滚缓冲存的是原始 `VTermScreenCell`（不是转换后的显示数据），保证 `sb_popline` 回调能拿到完整保真度的数据还给 libvterm。
- [x] `src/view/terminal_view.h/.cpp`：`TerminalView`，`QPainter` 按等宽字符格画格子（前景/背景色、粗体/斜体/下划线/删除线/反显都支持），方向键/Ctrl+字母/Backspace/Tab/Enter/Esc/Home/End/PageUp/PageDown 转发给 libvterm，鼠标滚轮翻回滚缓冲。直接订阅 `RingBuffer`，不经过 `IFrameStrategy`（见 §2 的"M2 修正"）。
- [x] `MainWindow`：顶层就是一个 `QTabWidget`（Hex / Terminal 两个 Tab，不再套一层"Receive"分组），两个视图都全程订阅同一个 Session，不因为 Tab 不在前台就停止更新。
- [x] `tests/test_vterm_engine.cpp`：真的喂 VT100 转义序列进 `VtermEngine` 测（纯文本、SGR 颜色、光标定位、回滚缓冲、reset），不是空测试；链接 `serialkit_ui` 而不是 `serialkit_headless`（`VtermEngine` 目前放在 ui 库里，见 `src/CMakeLists.txt` 注释），但测试本身不建任何 QWidget，无头运行不需要显示器。
- [x] Terminal 字体缩放（`Ctrl/Cmd +/-/0`，`QSettings` 持久化）+ Hex/Terminal 顶层 Tab 各自独立布局（Hex 页含 Send 区，Terminal 页没有），见 §7 第 15、16 条。
- [ ] 集成验证（连真实 Linux 设备 / `socat` pty，实测颜色、方向键、`vim`/`htop`、`Ctrl+C`、resize）——这部分需要用户配合实测，见 §6 风险登记表和本节末尾的验证方式说明，不能由这边单方面宣称"测过了"。

**M2 范围内明确不做**（以后有需要再单独提，别默认加上）：鼠标事件转发给远端程序、iTerm2/kitty 图片协议、可配置配色方案（先用固定默认色）、`moverect` 的最小重绘优化（M2 统一按 damage 全量重绘，正确性优先于性能，见 `terminal_view.cpp` 的 `handleDamaged`）。

**M2 开发过程中的构建系统事故**：根 `CMakeLists.txt` 加了 `LANGUAGES C CXX` 之后，命令行不带 `-G` 参数的 `cmake -S . -B build` 和 VS Code CMake Tools 插件（固定传 `-G Ninja`）对同一个 `build/` 目录的默认生成器选择不一致——谁先配置谁说了算，另一边再配置就会因为"generator 不匹配"直接报错，还连带把 `FetchContent` 的 libvterm 子构建搞坏，报错一路传导到"VS Code 和控制台都编译不出来"。根因不是代码，是两个工具对同一个 `build/` 目录的生成器预期不一致。**修复**：加了 `CMakePresets.json`（提交进仓库，只放一个 `hidden` 的 `ninja` 基础 preset，固定 `"generator": "Ninja"`）+ `CMakeUserPresets.json`（不提交，机器本地的 `default` preset 继承 `ninja` 并加上本机的 `CMAKE_PREFIX_PATH`）。以后配置/构建/测试统一用 `cmake --preset default` / `cmake --build --preset default` / `ctest --preset default`，VS Code CMake Tools 也会自动识别这套 preset，两边不会再对生成器有分歧。新开发机器要做的唯一一件事是照着 `CMakePresets.json` 里 `ninja` 的样子在本地建一份 `CMakeUserPresets.json`，填自己的 Qt 路径。

## 6. 风险登记表（在原方案 §8 基础上补充）

| 风险 | 状态 | 应对 |
|------|------|------|
| QCustomPlot GPL 传染整个仓库 | 已通过架构隔离解决 | `IPlotView` 接口隔离，M1-M4 不链接 |
| libvterm 假设可直接 `vcpkg install` | 已修正，且确认 vcpkg 没有官方 port | 改用 `cmake/Libvterm.cmake`（FetchContent + 手动编译），见 §1.2 |
| 为 QSerialPort 过度设计线程模型 | 已通过架构决定规避 | M1-M3 单线程，仅在性能验证后按需下沉 worker 线程 |
| Hex 视图接在 Framer 后面丢失原始字节语义 | 已通过并列订阅模型解决 | RingBuffer 多播，Hex/Logger 不经过 Framer |
| M1 写成隐式单例，M3 多会话被迫重构 | 已通过提前引入 Session 解决 | M1 即建组合根，M3 只放开数量限制 |
| 终端仿真正确性（libvterm 学习曲线） | 代码已实现（M2），**真实 Linux 控制台连接测试还没做**——这边没有 Linux 硬件/开发板，需要用户实测 | 已实现方向键/Ctrl 组合/颜色/属性/回滚；等用户拿真实设备连一次，确认 vim/htop 这类全屏程序、resize、Ctrl+C 表现正常 |
| libvterm 回调用 C 函数指针，容易和 Qt 信号的生命周期管理搞混 | 已通过设计规避 | `VtermEngine` 不用 Qt parent 持有（避免和外层 `std::unique_ptr` 双重所有权），callback trampoline 全部是 static 函数转 `this` 指针，不依赖 Qt 元对象系统 |
| 高速数据 UI 性能 | 原方案已识别，未变 | §4 批量刷新，本文档 §1.3 补充线程模型边界 |
| macOS/Linux 驱动与串口权限（CH340/CP210x） | 原方案已识别，未变 | 需实测覆盖，M1 验收项之一 |
| QSerialPort 自定义波特率在 Linux 下不可靠 | 原方案已识别，核对确认 termios2 存在"调用成功但波特率未真正生效"的已知问题，与驱动相关 | M1 先支持标准波特率，自定义波特率兜底方案作为独立 spike，不阻塞 M1 发布 |
| 测试策略缺失 | 已通过测试策略解决 | 核心逻辑类保持无 QWidget 依赖，Qt Test 无头单测 |

## 7. UI 交互约定（M1 返工后确定，参考 XCOM）

用户实测第一版 UI 后反馈"不合格"：看不到数据位/校验位/停止位设置、接收区和发送区各自散落一个长得一样的 Hex/ASCII 下拉框（观感重复）、时间戳和回车换行是硬编码行为而不是开关、发送侧要求手打 `\r\n\xNN` 转义、且没有多行发送/文件发送。参考 XCOM 返工后，以下几条作为**后续新增交互一律遵守**的约定，不要在新功能里又长回被否掉的模式：

1. **开关用复选框，不用下拉框**：一个功能是"开/关"语义时用 `QCheckBox`（如 `HexView` 的 Hex 显示、时间戳；`ManualSend`/`MultiLineSend` 的 Hex、+CRLF），不要用只有两个选项的 `QComboBox` 去表达它——那样界面上会出现多个长得一样的下拉框，用户分不清各自管什么（这正是第一版被否的直接原因）。
2. **发送内容"看到啥发啥"，不做转义解析**：文本发送框（`send_encoding.h` 的 `toRawBytes`）把输入原样编码成字节，不解析 `\r \n \t \xNN` 之类的转义序列；需要发送控制字符/任意字节就切到 Hex 模式，或者勾选"+CRLF"（`appendCrlfIfRequested`）单独控制换行——两件事分开，不要把"文本内容"和"是否加换行"糅进同一个转义语法里。
3. **接收显示不做字节级加工**：`HexView` 的 Hex 模式是连续的空格分隔十六进制流，没有偏移量列、没有旁边再放一份 ASCII——那是 hex 编辑器的做法，不是这个工具的做法。文本模式下 `\r\n`（含单独的 `\r`）转成真实换行，其余不可打印字节用 `.` 占位，每个字节都有确定的可见映射，不能因为"好看"就悄悄丢字节。落盘日志（`RawLogger`）不受这些开关影响，永远记录原始字节+时间戳。
4. **单行发送和多行发送已合并成一个发送框**（用户明确要求，`ManualSend` 已删除，逻辑全部并入 `MultiLineSend`）：一个 `QPlainTextEdit`，Enter 发送、Shift+Enter 换行（`MultiLineSend::eventFilter` 拦截 `QEvent::KeyPress`，chat 软件的约定，不是 Qt 默认行为）；定时/循环发送（`m_timedCheck`/`m_intervalSpin`/`m_timer`）依附在这个发送框上；文件发送在一条分隔线（`QFrame::HLine`）之下，视觉上和 Hex/+CRLF/定时发送完全分开，避免用户误以为那两个复选框是给文件发送用的（第一版就踩过这个坑）。以后不要为了"区分单行/多行使用场景"又拆回两个框——已经确认过一次不需要。
5. **文件发送是独立字节路径**：`MultiLineSend::handleSendFileClicked` 按 4KB 分片读文件原始字节直接 `Session::send()`，完全不经过 Hex/+CRLF 那两个复选框（那两个只作用于文本框内容）——文件内容不应该被任何编码开关影响。
6. **多指令宏面板的"共享 vs 每行独立"是对着 XCOM/SSCOM 两个参考逐项定的，不是拍脑袋**：`MacroPanel` 里 Hex 复选框是**每行独立**的（参考 SSCOM，同一个面板里可以同时放一条 Hex 查询帧和一条 ASCII 文本命令）；+CRLF、关联数字键盘、自动循环发送+周期是**面板级共享**的（两个参考在"多条发送"区域都没有做成每行一份 CRLF，循环发送也是共用一个节奏）；槽位数量固定 10 个，不做 SSCOM 那种 +/- 动态增删。以后再调整这个面板前，先想清楚是要更像 XCOM 还是更像 SSCOM，不要在两者之间随意混搭出第三种行为。
7. **面板默认折叠，用一个按钮展开/收起**：`MacroPanel` 默认 `hide()`，`MainWindow` 用一个"▸/▾ Macros"按钮切换可见性——这是用户特别认可的 SSCOM"隐藏"按钮交互，不要改成 Tab 切换或者始终展开。
8. **`MacroPanel` 的槽位列表必须限高+可滚动，不能直接 `addLayout`**：10 个槽位全展开会不断撑高 Send 分组，在非全屏窗口下会把 Receive 区挤到几乎看不见。槽位列表包在一个 `QScrollArea`（`setMaximumHeight`、`setWidgetResizable(true)`）里，共享设置行留在滚动区外面始终可见；`HexView` 的输出框另外设了 `setMinimumHeight`，两边一起保证 Receive 区不会被压没。以后往 `MacroPanel` 或 Send 分组里加更多常驻控件时，先想一下会不会重新引入这个问题。
9. **文件路径用只读 `QLineEdit` 显示，不用 `QLabel`**：`QLabel` 显示长路径会被截断且不能选中复制；只读 `QLineEdit` 可以横向滚动查看/复制完整路径，光标复位到开头（`setCursorPosition(0)`）让用户先看到文件名开头而不是被路径中间的部分挤到看不见文件名。
10. **状态栏用 `addPermanentWidget` 放字节计数，不和主状态文字混在一起**：`MainWindow::m_byteCountLabel` 通过 `statusBar()->addPermanentWidget()` 常驻右侧，和左侧 `addWidget()` 的连接状态文字（`m_statusLabel`）分开，符合状态栏"左边讲状态、右边讲统计"的通用约定。计数在每次新连接时清零（`toggleConnection` 里），断开后保留数字直到下次连接，方便用户看这次会话总共收发了多少。
11. **`HexView` 默认不显示 TX（发送）字节，这是一次真实的用户误判事故，不是猜出来的规则**：早期版本把 TX/RX 合并显示在一个框里，用颜色区分（蓝=发送，黑=接收）。用户用裸 USB 转串口（RX/TX 完全没接）实测，发现"没短接也会收到数据，而且点一次发送就出现一次"，怀疑软件在假回环。追下来发现根本不是回环——是他自己发送的字节被显示在了标着"Receive"的框里，他没注意颜色区分，直接把自己的发送当成了异常接收。这说明"用颜色区分 TX/RX 塞进一个框"这个设计本身就是误导性的，尤其是这个框的标题明确写着"接收"。修复：`HexView` 默认只订阅 `RingBuffer`（真实收到的字节），**不**订阅 `Session::bytesSent`；加一个"Show sent"复选框给需要看统一时间线的人自己打开，默认关闭。以后再往 `HexView` 加"顺便显示点别的什么"之前，先想一下会不会让一个标题明确的框显示出不属于那个标题的内容。
12. **`SerialTransport::open()` 成功后要清空驱动缓冲区**：`m_port->clear(QSerialPort::AllDirections)`。端口空闲时可能积累了电气噪声或者上一次会话的残留字节，不清空的话一连接就会把这些"旧"字节当成新数据显示，容易被误判成"没接线也有数据"（虽然后来查明这次具体的误判另有原因，见上一条，但清缓冲区本身是独立的、必要的防御性修复，两个问题都要处理）。
13. **Hex 视图和 Terminal 视图数据层面不是互斥模式，UI 摆放层面是顶层 Tab**：`HexView`/`TerminalView` 两个都全程 `attachSession`，不因为哪个 Tab 不在前台就停止更新——这是最初方案"同一份字节流的两个视图"的定位（不是"两个拼在一起的软件"），也是 `TerminalView` 直接订阅 `RingBuffer` 而不经过 `IFrameStrategy` 的同一个理由（§2 "M2 修正"）。这条是数据订阅层面的约定，和下面第 16 条"Tab 摆在最外层、每个 Tab 页自己组装布局"是两回事——UI 摆放怎么调整都不该影响这条。以后再加新视图（比如 M5 波形），也应该走这个模式：新增一个 Tab，全程订阅，而不是做成切换时才连接/断开的互斥模式。
14. **新连接要重置 `TerminalView`，断开不用**：`TerminalView::attachSession` 只在挂上一个新的、非空 Session 时才调用 `VtermEngine::reset()`（清屏+清回滚），单纯断开（`attachSession(nullptr)`）不清——和 `HexView` 断开后保留最后内容、等用户手动点 Clear 的行为一致。理由：连接一个新设备时不该看到上一个设备的终端残留画面，但断开连接不代表用户想丢掉刚才看到的内容。
15. **Terminal 字体缩放只用快捷键，不加工具栏按钮**：`Ctrl/Cmd +`/`Ctrl/Cmd -`/`Ctrl/Cmd 0`（`TerminalView::zoomIn`/`zoomOut`/`resetZoom`），对齐 iTerm2/VS Code 终端的习惯，不占界面空间。在 `keyPressEvent` 里拦截、`accept()` 消费掉，不转发给 libvterm（否则远端 shell 会收到一个奇怪的按键）。字号限制在 6~48pt，存 `QSettings`（key `terminal/fontPointSize`），下次启动恢复上次的缩放级别。`resetZoom()` 用 `QFontDatabase::systemFont(QFontDatabase::FixedFont).pointSize()` 现查默认字号，不要硬编码某个数字——不同系统的等宽字体默认磅值不一样。
16. **Hex/Terminal 提到最外层 Tab，Send 区域只存在于 Hex 页里，不是靠联动隐藏**：一开始的方案是 Hex/Terminal 当 Receive 分组内部的子 Tab，Send 分组作为兄弟节点靠 `currentChanged` 信号联动隐藏/显示；讨论后改成更直接的做法——`Hex`/`Terminal` 直接是 `MainWindow` 最外层 `QTabWidget` 的两个 Tab，Hex 的 Tab 页自己装一个"Receive"分组（`HexView`）+ 一个"Send"分组（发送框、Macros）；Terminal 的 Tab 页就是裸的 `TerminalView`，不套"Receive"分组框（Tab 标题已经说明是 Terminal 了，不需要再嵌套一层同义的标题），也没有 Send 区，把整页空间都让给终端。两种做法数据层面等价（见上面第 13 条），选后者纯粹是因为心智模型更直接：用户想的是"我现在是 Hex 模式还是 Terminal 模式"，而不是"Receive 里选了哪个子 Tab、Send 会不会跟着变"。**踩过的坑**：字体缩放这条改动让"改变行列数导致回滚缓冲区里存的行比当前列数窄"这件事从理论情况变成了会实际触发的路径——`VtermEngine::handleSbPopLine` 原来只写 `cells[i] = line.at(i)`（`i < line.size()` 才写），列数变宽后 `cells` 数组后半段留着未初始化的内存，libvterm 的 `resize()` 内部靠 `cells[col].width` 步进读这块内存，宽度如果凑巧是垃圾值 0 就会死循环——实测直接把主线程卡死（`sample` 抓到的调用栈钉死在 `resize_buffer`/`screen.c:699` 那个 `for` 循环里出不来）。修复：补齐的空位显式填一个 `width=1` 的空白 cell，不留未初始化内存。这是"频繁改变终端行列数"第一次被真正跑到的代码路径，如果以后有别的地方也会动态改 `VtermEngine::resize()` 的行列数，先想一下会不会踩到同一类"缓冲区/图层大小不匹配"的坑。
