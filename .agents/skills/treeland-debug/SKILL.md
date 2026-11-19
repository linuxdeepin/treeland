---
name: treeland-debug
description: "检查和操控运行中的 treeland 合成器，用于定位窗口、焦点、输入、输出、渲染与卡死问题：窗口树与单窗口详情、客户端清单、空闲与空闲抑制来源、快捷键、输入注入、截图、QtQuick 场景树、实时视图，以及调试 socket 的访问权限和 Qt 日志分类。在调试/复现 treeland 运行时问题、读写 remoteDebug 开关、连接调试 socket、按 treeland 与 waylib 的日志分类读日志时使用。"
---

# Treeland 调试

> **铁律**：先用 `treeland-debug` 读到实时状态再改代码；不要只凭日志或猜测改行为。

已退出进程的崩溃可先依据用户提供的 core/backtrace 和对应源码定位；若无法访问故障机器或没有 `treeland-debug`，应明确记录限制，不必为获取实时状态重启服务或注入事件。用户要求不编译时，不执行下文的构建验证，并说明未完成运行时验证。

## 范围
本 skill 说明如何检查与控制**运行中**的 treeland 合成器来定位问题。问题可在运行时观察到时使用（窗口状态不对、焦点/激活异常、输入不落地、输出缺失或黑屏、崩溃、卡死、性能问题）。构建/安装见 `treeland-build`；日志改动见 `treeland-logging`；DConfig 见 `treeland-dconfig`；协议见协议类 skill。

主要仓库位置：

- `src/core/` —— 生命周期、QML 引擎胶水、`WaylandServer`（`src/core/waylandserver.cpp`）。
- `src/surface/` —— surface/window 封装、状态、可见性、几何（`surfacewrapper.cpp`）。
- `src/seat/`、`src/input/` —— seat 与输入；桌面启动与协议组装在 `src/core/waylandserver.*`。
- `src/workspace/` —— workspace 模型与切换。
- `src/output/`、`src/modules/output-manager/` —— output 与布局。
- `src/idle/` —— 空闲通知与空闲抑制。
- `src/session/` —— 会话、用户切换与 per-session 资源。
- `src/modules/` —— 功能模块（compositor-action、input-manager、shortcut、dde-shell 等）。
- `src/plugins/`（lockscreen、multitaskview）—— 插件与功能集成。
- `src/common/treelandlogging.*` —— 集中式日志分类。
- `waylib/` —— 底层 wlroots+QtQuick 合成器框架（output 对应 `QQuickWindow`，surface 对应 `QQuickItem`）。
- `3rdparty/wlroots/` —— 内联的 wlroots（通过 `<wlr_all.h>` 使用原始 C API）。
- `tools/treeland-debug/` —— 检查/控制运行中合成器的 CLI；`tools/treeland-debug/README.md` 是该工具自己的完整文档。

## 运行环境
treeland 既可作为 Release 服务运行，也可作为开发中的构建运行：

- **全局模式**（首选，DDM 场景）：单进程管理所有用户；以 **`dde`** 用户身份由 systemd **system** 单元 `treeland.service` 启动（`User=dde`，安装在 `SYSTEMD_SYSTEM_UNIT_DIR`），总线名 `org.deepin.Compositor1`，`ExecStart=.../treeland.sh --lockscreen`。该单元还设置 `LIBSEAT_BACKEND=seatd`、`SEATD_SOCK=/run/dde-seatd.sock` 与 `XDG_RUNTIME_DIR=%t/treeland`（见 `misc/systemd/treeland.service.in`）。
- **用户模式**：按用户启动（启动器导出 `TREELAND_RUN_MODE=user`，见 `misc/wayland-sessions/treeland-user-wrapper.in`），行为类似普通窗口管理器。
- **开发（dev）构建、嵌套运行**：正在开发的版本通常**不是**已安装的发行版，而是从工作树构建、作为**当前用户身份**的独立进程，嵌套在已有 Wayland 或 X11 会话里。见下一节。

调试 socket 的位置、权限与提权边界完全由构建类型决定（Release / Debug / 会话模式各一套），详见 [references/access.md](references/access.md)。先记住两条：Release 构建的 socket 只有 root 能连，`treeland-debug` 会自动走 `pkexec`；Debug 构建的 dev socket 仅所属 UID 可连、不需要 polkit。

## 调试开发（dev）构建
当问题出在当前正在开发的版本上，你以当前用户身份把该构建作为独立进程、嵌套在已有 Wayland 或 X11 会话里运行：

```bash
# 在构建树中，嵌套于已有 Wayland 会话
WLR_BACKENDS=wayland ./build/src/treeland.sh
# 嵌套于已有 X11 会话
WLR_BACKENDS=x11 ./build/src/treeland.sh
```

`treeland.sh` 包装 `treeland`：先以 `--try-exec` 探测，输出里出现 `failed to create dri2 screen` 时就以 `WLR_RENDERER=pixman` 软件渲染重跑（与 service 行为一致）。

- **debug source**：Release 构建默认关闭（会话级运行时不默认暴露窗口内容与输入注入），先 `treeland-debug debug on` 打开；**Debug 构建始终开启、不看开关**，直接跑 `treeland-debug` 即可。
- **不需要 `sudo -u dde`**：以**当前用户**运行 `treeland-debug`，与实例处于同一会话；dev socket 仅当前 UID 可访问。
- **日志进终端，不进 journalctl**：treeland 的 stdout/stderr（Qt 分类日志）落在你启动它的地方。在该进程环境里设置 `QT_LOGGING_RULES`；Release 构建可传 `--console-log` 强制开启控制台日志（Debug 构建总是输出到控制台）。
- **与全局服务无 socket 冲突**：Debug 构建使用独立的 `-dev` socket 名，并对并发实例自动加数字后缀，因此嵌套 dev 实例与全局 `treeland.service` 可以共存 —— 不必停掉后者。

## 启用 debug source（第一步）
`remoteDebug` 是调试功能的总开关，**Release 构建里默认关闭**，Debug 构建里则被忽略（始终开启）：开关打开时 Treeland 就会发布调试 socket，谁连上来就给谁提供 `WindowTree` 远程对象。

- 连不上时先看 stderr：`treeland-debug` 会提示"没有发布调试 socket"，此时要么开关没开，要么 Treeland 没在运行。
- 开关归属合成器运行用户：DConfig 的应用级键按用户分开存储，Treeland 只读自己运行用户的那一个，所以要用该用户身份（global 模式下是 `dde`）由 root 以 `dde-dconfig -u <uid>` 写入。`treeland-debug debug on|off` 已经封装了这一步：它从运行中的合成器查出该 uid，必要时自己走 `pkexec` 提权。
- 远程源是个无状态薄壳：窗口树、截图、场景树都在每次调用时现场采集。唯一的会话级资源是输入事件捕获 —— 只在客户端轮询 `getEvents()` 时挂上 `eventFilter` 与环形缓冲，空闲后自动拆掉。所以没人连时除了那条 socket 没有别的开销；要彻底禁止调试就把开关关掉（socket 也随之消失）。

```bash
# 打开调试（默认是关），必要时会通过 polkit 提权
treeland-debug debug on
# 关回去 / 只看当前状态
treeland-debug debug off
treeland-debug debug
```

等价的底层命令（root 身份，`<uid>` 为合成器运行用户）：

```bash
dde-dconfig -u <uid> set -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k remoteDebug -v true
```

切换 `remoteDebug` **立即生效** —— remote source 会在运行时被创建或销毁，无需重启（合成器运行时监听 `remoteDebugChanged`）。`treeland-debug` 连接的是构建类型对应的调试 socket，remote object 名为 `WindowTree`。

## 快速上手
```bash
# remoteDebug 是总开关、Release 默认关闭，普通命令不会改它：先 treeland-debug debug on
# （Debug 构建不看开关、始终开启，dev 实例直接跑即可）
# 连不上时会提示"没有发布调试 socket" —— 那就是开关没开或 treeland 没在运行

# 检查 —— Release 构建会通过 polkit 自动提权到 root；Debug 构建以当前用户运行
treeland-debug tree
treeland-debug --json windows
treeland-debug clients
treeland-debug idle          # 空闲时长与空闲抑制来源

# 控制
treeland-debug activate dde-file-manager
treeland-debug maximize 93824992268800
treeland-debug close dde-file-manager

# 注入输入
treeland-debug event key Return tap
treeland-debug event button left click

# 截图
treeland-debug screenshot output DP-1 /tmp/ss.png   # 语法：screenshot output [name] [file]
treeland-debug screenshot window 93824992268800
```

命令全集、每个检查命令的 `--json` 字段、输出约定、`listen`/MCP 接口与事件类型编号见 [references/cli.md](references/cli.md)（`treeland-debug --help` 是命令清单的权威来源）。

## 诊断流程
从症状出发，选最小可靠的检查手段。先复现，再逐层隔离（treeland vs waylib vs wlroots vs 客户端）。

**窗口状态不对（最大/最小/全屏/平铺、几何、可见性）：**
1. `treeland-debug --json windows` —— 读取目标窗口（按 `appId` 匹配）的 `state`、`visible`、`active`、`minimized`、`geometry`、`outputs`、`frames`、`damage`。
2. `treeland-debug tree` —— 确认窗口所在的 layer/workspace。
3. `treeland-debug window <id>` —— 完整单窗口视图：region、SSD 矩形、`Frames`、`Damage`、`Frame pacing`，末尾一条可直接贴进报告的 `-geometry WxH+X+Y`。
4. `treeland-debug scene <id>` —— 布局看着对但渲染不对时，检查 QtQuick 场景（菜单/弹窗/装饰）。
5. 与日志交叉验证：`QT_LOGGING_RULES='treeland.surface.debug=true;treeland.workspace.debug=true'`。

**窗口不在任何 output 上（`Output`/`Outputs` 为 `(none)`）：**
1. `treeland-debug window <id>` —— 看 `Last leave`（哪个 output、离开时它是否 enabled、是否把该 output 的窗口列表清空）与 `Last outputs update`（上次重新计算 outputs 的原因、参与计算/启用/自动模式的 output 数量）。这两行就是回答"为什么它现在不在任何 output 上"的证据。
2. `treeland-debug --json windows` 里的 `outputs`/`framePacingOutput` 对比 `tree` 中的 output 是否存在。
3. 相关分类：`treeland.output`、`treeland.surface`。

**焦点 / 激活不对：**
1. `treeland-debug --json windows` —— 检查 `active`；`treeland-debug cursor` 的 `window:` 字段即光标下的窗口（键盘焦点只暴露在 HTTP/WS）：`treeland-debug listen --host 127.0.0.1 --port 8080` 后 `curl -s 127.0.0.1:8080/api/focused`。
2. `treeland-debug activate <id|appId>` 后复查。
3. 相关分类：`treeland.seat`、`treeland.popup.focus`、`treeland.activation`。

**快捷键不生效 / 想知道谁绑了什么：**
1. `treeland-debug shortcuts` —— 看当前注册的绑定（只有客户端通过 `treeland_shortcut_manager_v3` 绑定过才有内容，空列表通常意味着没有 shell 绑定）。
2. `treeland-debug events` —— 边操作边看：`shortcut-registered`/`shortcut-unregistered`（重新登录或重启 shell 会先把旧绑定都注销再重注册）与 `shortcut-triggered`（按键触发带 `[press|release|repeat]`，`target` 是**按键触发那一刻**的键盘聚焦窗口 —— 动作执行前采样，所以切工作区/任务切换不会报成目标窗口；swipe/hold 手势带 `(gesture)` 且 `target` 为 0）。
3. 相关分类：`treeland.shortcut`。

**输入不落地 / 目标不对：**
1. `treeland-debug move-cursor <x> <y>` 后 `treeland-debug event button left click`；`treeland-debug event key Return tap`。
2. 用 `windows`（`ACTIVE` 列）确认键盘/指针焦点 —— 键盘事件发往键盘聚焦的 surface，指针按键发往光标下的 surface。
3. 输入区域不对时看 `window <id>` 的 `Input region`（surface 局部坐标的 box 列表）：未设置（`NULL`）的输入区域按协议是**无穷大**，打印出来是覆盖整面的一个 box；为空则要么是客户端显式提交了空 region（完全不接受输入），要么是 surface 还没提交过 buffer。出现多个小块通常是客户端只为命中区设了输入区域（如标题栏、按钮）。`Opaque region` 为空只表示客户端没有声明不透明区域（默认值），不要读成窗口透明，也不要和"不接收输入"混。
4. 相关分类：`treeland.input`、`treeland.seat`、`waylib.input.pointer`、`waylib.seat`。检查 seatd：`sudo journalctl -u dde-seatd -f`，以及 `LIBSEAT_BACKEND=seatd`/`SEATD_SOCK=/run/dde-seatd.sock` 是否已设置（见 `misc/systemd/treeland.service.in`）。

**输出黑屏 / 缺失 / 分辨率不对：**
1. `treeland-debug tree` —— 确认 output 存在；`treeland-debug --json windows` 按 output 查看。
2. `treeland-debug screenshot output <name> <file>` 看合成器实际渲染了什么。
3. 相关分类：`treeland.output`、`waylib.output`、`waylib.output.drm`。硬件渲染失败时 `treeland.sh` 的 pixman 回退会生效。

**崩溃 / 卡死：**
1. 检查服务状态与重启日志：`sudo journalctl -u treeland.service -n 200 --no-pager`（Qt 日志通过 journal appender 直接写入；标准错误的捕获见"读日志"）。
2. 若启用了 ASAN：经 `treeland.service` 启动时日志写到 `/tmp/treeland-asan*` 文件而不是 journalctl（`ASAN_OPTIONS` 在 `misc/systemd/treeland.service.in` 里设）；本地直接跑则仍在 stderr。
3. 用 `treeland-debug`（注入事件、activate/close 窗口）以最小步骤复现，并记录崩溃前最后几个动作。
4. 卡死时用 `top`/`clients` 看是否有客户端卡住，必要时 gdb attach（`sudo gdb -p <pid>`）。

**怀疑客户端问题：** 用 `treeland-debug clients` 查客户端 pid；确认它确实在与合成器通信（`windows` 存在、`frames` 在递增）。冻结的客户端会表现为 `frames` 不再推进。

## 读日志
treeland 使用 Qt 日志分类，全部集中在 `src/common/treelandlogging.*`。分类字符串 ID 遵循 `treeland.<module>[.<submodule>]`（例如 `treeland.surface`、`treeland.input`、`treeland.workspace`、`treeland.output`、`treeland.seat`、`treeland.protocol`、`treeland.wallpaper`、`treeland.xwayland`、`treeland.shell.xdg`、`treeland.popup.focus`、`treeland.debug`）。waylib 的分类在 `waylib/src/server/wayliblogging.*`，ID 为 `waylib.*`。完整分类表见 `treeland-logging`。

用 `QT_LOGGING_RULES` 控制日志详细程度。全局模式下 `treeland.service` 是 **system** 单元，因此要通过 systemd drop-in 设置该变量（system 服务没有 `--user` 环境）。嵌套 dev 实例则在启动进程的环境里设置（见上面的 dev 构建一节）。

```bash
sudo mkdir -p /etc/systemd/system/treeland.service.d
printf '[Service]\nEnvironment=QT_LOGGING_RULES=treeland.surface.debug=true;treeland.input.debug=true\n' | sudo tee /etc/systemd/system/treeland.service.d/debug.conf
sudo systemctl daemon-reload
sudo systemctl restart treeland.service
```

注意：`src/main.cpp` 通过 `DLogManager::registerJournalAppender()` 直接写 journal，Qt 分类日志不依赖 stdout/stderr。随包安装的 unit 设置了 `StandardOutput=null` / `StandardError=null`，丢弃的是脚本及直接写到标准输出/错误的内容。仅在需要捕获这些内容时，才在 drop-in 中追加 `StandardOutput=journal` / `StandardError=journal`；Release 构建若还需要控制台 Qt 日志，应同时传入 `--console-log`。journal 本身是 system journal（不是 `--user`）：

```bash
# 全局 treeland 服务（以 dde 运行）最近的日志
sudo journalctl -u treeland.service -n 200 --no-pager
# 或带完整详细度并跟随
sudo journalctl -u treeland.service -f
```

若某问题只在硬件渲染下复现，注意 `treeland.sh` 的 pixman 回退可能掩盖它。

## 规则
- 需要解析的状态用 `--json`；控制窗口前先用 `windows`/`clients`/`top` 查到稳定的窗口 id。
- 截图是强证据：布局 `tree` 正确但图像不对，问题在渲染/QtQuick 场景，不在布局。
- 优先修根因；实时工具用于观察与复现，不能替代阅读拥有该状态的那段代码路径。
- 修改权限/提权相关的理解前，先读 [references/access.md](references/access.md)，不要凭"root 能连任何 socket"类推到会话模式。

## 验证

- [ ] 改动前先用一条 `treeland-debug` 命令（通常是 `--json windows` 或 `tree`）记录当前实时状态
- [ ] 若涉及代码，先构建（见 `treeland-build`）
- [ ] 用同一条命令复验，输出符合预期
- [ ] 在交付说明里写出：哪条 `treeland-debug` 命令可以确认这次改动

## 参考文件
- [references/cli.md](references/cli.md) —— 命令全集、全局选项、`--json` 覆盖范围、输出约定、`listen`/MCP、事件类型编号。
- [references/access.md](references/access.md) —— 构建类型 × 访问权限矩阵、调试 socket 命名与探测顺序、polkit/`--no-escalate`、`no-auth` 调试窗口、`TREELAND_DEBUG_DIR`/`TREELAND_DEBUG_SOCKET`。
- `tools/treeland-debug/README.md` —— 该工具自身的完整文档（含 REST/WS/MCP 细节）。
