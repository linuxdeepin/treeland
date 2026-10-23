---
name: treeland-debug
description: "触发场景：调试/诊断/复现/分析运行中的 treeland（窗口、工作区、输入、输出、渲染、崩溃、卡死、焦点、实时状态检查）、`treeland-debug`、`remoteDebug`、`WindowTree` Remote Object、`QT_LOGGING_RULES`、treeland 日志分类、treeland 的 `journalctl`、`org.deepin.Compositor1`、窗口树检查、输入事件注入、从运行中的合成器截图、`pkexec`、`polkit`、`--no-escalate`、treeland-debug 的 `root` 权限。"
---

# Treeland 调试

> **铁律**：先用 `treeland-debug` 读到实时状态再改代码；不要只凭日志或猜测改行为。

## 范围
本 skill 说明如何检查与控制**运行中**的 treeland 合成器来定位问题。问题可在运行时观察到时使用（窗口状态不对、焦点/激活异常、输入不落地、输出缺失或黑屏、崩溃、卡死、性能问题）。构建/测试问题见 `AGENTS.md` 的构建与测试；协议集成见协议类 skill；DConfig 见 `treeland-dconfig`。

主要仓库位置：

- `src/core/` —— 生命周期与 QML 引擎胶水（`src/core/qmlengine.*`、`src/core/treeland.*`）。
- `src/surface/` —— surface/window 封装、状态、可见性、几何（`surfacewrapper.cpp`）。
- `src/seat/` —— seat 管理；桌面启动与协议组装在 `src/core/waylandserver.*`。
- `src/workspace/` —— workspace 模型与切换。
- `src/modules/` —— 功能模块（show-desktop、input-manager、output-manager、dde-shell 等）。
- `src/plugins/`（lockscreen、multitaskview）+ `src/modules/` —— 插件与功能集成。
- `src/common/treelandlogging.*` —— 集中式日志分类。
- `waylib/` —— 底层 wlroots+QtQuick 合成器框架（output 对应 `QQuickWindow`，surface 对应 `QQuickItem`）。
- `wlroots/` + `3rdparty/wlroots/` —— 内联的 wlroots（通过 `<wlr_all.h>` 使用原始 C API）。
- `tools/treeland-debug/` —— 用于检查/控制运行中合成器的 CLI。

## 运行环境
treeland 既可作为发行版服务运行，也可作为开发中的构建运行：

- **全局模式**（首选，DDM 场景）：单进程管理所有用户；以 **`dde`** 用户身份由 systemd **system** 单元 `treeland.service` 启动（`User=dde`，安装在 `SYSTEMD_SYSTEM_UNIT_DIR`），总线名 `org.deepin.Compositor1`，`ExecStart=.../treeland.sh --lockscreen`。
- **用户模式**：按用户启动，行为类似普通窗口管理器。
- **开发（dev）构建、嵌套运行**：正在开发的版本通常**不是**已安装的发行版，而是从工作树构建、作为**当前用户身份**的独立进程，嵌套在已有 Wayland 或 X11 会话里。见下一节。

调试 Remote Object 的本地 socket 仅属主可访问，所以 `treeland-debug` 必须以能访问该 socket 的用户身份运行。发行版构建中 `treeland-debug` 会通过 polkit 自动提权到 **root**（见下方访问控制），而 root 可访问任何用户的 socket，因此不需要手动加用户前缀。调试构建中，`treeland-debug` 必须以与 treeland 实例**相同的用户**运行：

- 全局服务（以 `dde` 运行、发行版构建）→ 直接运行 `treeland-debug`，它会自动提权到 root。
- 嵌套的 dev 实例（以你的身份运行、调试构建）→ 在自己会话里以**当前用户**运行，不加前缀。

### 访问控制
`treeland-debug` 按构建类型限制使用者：

- **发行版构建** —— 只有 `root` 能运行 `treeland-debug`。非 root 调用者会被自动通过 `pkexec`（polkit）重新执行，并提示认证。传 `--no-escalate` 可跳过提示并立即失败（脚本或 CI 中常用）。随包安装 polkit 策略以保证开箱即用：源文件 `misc/polkit/org.deepin.dde.treeland-debug.policy.in`，安装为 `org.deepin.dde.treeland-debug.policy`。
- **调试构建**（`TREELAND_DEBUG_DEV_BUILD`）—— 所有用户都能无限制运行 `treeland-debug`。

因为 root 可访问任何用户的 socket，发行版构建无需 `sudo -u dde` 前缀即可连到全局 `dde` 服务。调试构建中则必须以与 treeland 实例相同的用户运行。

### 调试 socket 命名
调试 Remote Object 把本地 socket 放在 `QDir::tempPath()`（通常是 `/tmp`，遵循 `$TMPDIR`）—— 不是 `XDG_RUNTIME_DIR` —— 这样在任何 runtime 目录下运行的 `treeland-debug` 客户端都能发现它（socket 仍仅属主可访问，所以客户端必须以与 treeland 实例相同的 Unix 用户身份、或以 **root** 身份运行，见访问控制）。基础名按构建类型区分：

- **发行版构建**：`org.deepin.dde.treeland.debug`
- **调试构建**：`org.deepin.dde.treeland.debug-dev`

名字已被其他实例占用时，会追加数字后缀（`-1`、`-2`，…，最多到 `-99`），通过非阻塞 `flock` 建议锁选取 —— 与 libwayland 处理 Wayland socket 的方式一致（`src/modules/resource/treelanddebugsocket.*`）。这意味着**多个 treeland 实例可以共存**（全局服务 + 会话服务 + 嵌套 dev 构建）而不冲突，不再需要停掉其中一个才能运行另一个。崩溃实例留下的陈旧 socket 文件会被自动回收。

**调试构建**的 `treeland-debug` 客户端会自动优先用调试 socket（`…debug-dev`），调试实例不可达时回退到发行版 socket（`…debug`）；**发行版构建**的客户端使用发行版 socket。显式 `--url` 永远覆盖以上逻辑，不做任何隐式处理。

## 调试开发（dev）构建
当问题出在当前正在开发的版本上，你以当前用户身份把该构建作为独立进程、嵌套在已有 Wayland 或 X11 会话里运行：

```bash
# 在构建树中，嵌套于已有 Wayland 会话
WLR_BACKENDS=wayland ./build/src/treeland.sh
# 嵌套于已有 X11 会话
WLR_BACKENDS=x11 ./build/src/treeland.sh
```

`treeland.sh` 包装 `treeland`（在 `dri2` 失败时回退到 pixman 软件渲染，与 service 行为一致）。调试该实例时：

- **debug source 已默认开启**：调试构建（`TREELAND_DEBUG_DEV_BUILD`）中调试 Remote Object **默认开启**（Debug 配置会定义 `ALWAYS_ENABLE_TREELAND_DEBUG`；给 CMake 传 `-DTREELAND_DEBUG_SOURCE=OFF` 可保持关闭）—— 可跳过下面的 DConfig 步骤。只有发行版构建需要设置 `remoteDebug`。
- **不需要 `sudo -u dde`**：以**当前用户**运行 `treeland-debug`，与实例处于同一会话。因为 socket 仅属主可访问，默认就连到你自己的实例。
- **日志进终端，不进 journalctl**：treeland 的 stdout/stderr（Qt 分类日志）落在你启动它的地方。在该进程环境里设置 `QT_LOGGING_RULES`；发行版构建可传 `--console-log` 强制开启控制台日志（调试构建总是输出到控制台）。
- **与全局服务无 socket 冲突**：调试构建使用独立的 `-dev` socket 名，并对并发实例自动加后缀（见上面的 socket 命名），因此嵌套 dev 实例与全局 `treeland.service` 可以共存 —— 不必停掉后者。调试构建的 `treeland-debug` 默认优先连你的 dev 实例。

## 启用 debug source（第一步）
`WindowTree` 调试 Remote Object 在**发行版构建里默认关闭**（调试构建已默认开启）。若要在发行版构建上开启，执行一次：

```bash
sudo -u dde -- dde-dconfig set \
  -a org.deepin.dde.treeland \
  -r org.deepin.dde.treeland \
  -k remoteDebug \
  -v true
```

切换 `remoteDebug` **立即生效** —— remote source 会在运行时被创建或销毁，无需重启（合成器运行时监听 `remoteDebugChanged`）。`treeland-debug` 连接的是构建类型对应的调试 socket（见上面的 socket 命名），remote object 名为 `WindowTree`。

## treeland-debug CLI
`treeland-debug` 是 adb 风格的检查/控制工具。两种模式：一次性 `treeland-debug <cmd> [args]` 与 REPL `treeland-debug shell`。发行版构建中它会通过 polkit 自动提权到 root；调试构建中以与 treeland 实例相同的用户运行。`--json` 为 `tree`/`cursor`/`windows`/`clients`/`idle` 提供机器可读输出。

全局选项：`--url <url>`（默认 auto —— 构建类型对应的 socket；调试构建优先调试 socket，回退到发行版 socket）、`--name <name>`（默认 `WindowTree`）、`--timeout-ms <n>`（默认 30000）、`--json`、`--preview` / `--no-preview`（终端内联图片预览，默认自动探测）。需要指定某个实例时显式传 `--url`（原样使用，不做隐式处理）。

### 检查
| 命令 | 输出 |
| --- | --- |
| `tree` | 完整布局树（人类可读；`--json` 输出 JSON）。`{"currentMode", "layers":[{name,layer,windows,workspaces:[{id,isActive,windows}]}]}` |
| `cursor` | 光标位置 `{"x","y"}`。 |
| `windows` | 顶层窗口表格 / JSON 数组。 |
| `clients` | 客户端及其窗口（含 `pid`、`executable`、`command`）。 |
| `idle` | 空闲状态：距上次输入活动的时长 `idleMs`、空闲抑制是否生效 `inhibited`/`screensaverInhibited`，以及每个抑制来源（`zwp_idle_inhibit_manager_v1` surface 或 `treeland_screensaver_v2` inhibit，含 `pid`/`appId`/`title`/`appName`/`reason`/`heldMs`）。合成器没有全局空闲阈值（阈值由客户端通过 `ext-idle-notify-v1` 自行设定），因此只报告时长与抑制来源。列表**包含当前未生效**的来源（如被最小化的抑制窗口），用 `active` 区分，只有 `active` 的才算进 `inhibited`；`surfaceId` 只有在抑制挂在 toplevel 上时才等于可控制命令的窗口 id（layer-shell / subsurface 抑制查不到窗口）。 |
| `top [interval-ms]` | 类似 `top` 的客户端/窗口实时视图（Ctrl+C 退出），顶部含 `idle`/`inhibited` 汇总行。 |
| `events [interval-ms]` | 输入事件实时流（默认 50ms）。 |
| `watch <id> [interval-ms]` | 轮询单个窗口的变化（默认 250ms）。 |
| `scene [id]` | 单个窗口（按 id/appId）或整个场景的 QtQuick 场景树 —— 用于查看不在布局 `tree` 里的菜单/弹窗/装饰。 |

每个窗口都有稳定的数字 `id`（以及 `appId`），所有控制命令都接受这两种标识。用 `windows`/`clients`/`top` 查 id。

### 窗口控制
`activate`、`close`、`minimize`、`maximize`、`fullscreen`、`move <id> <x> <y>`、`resize <id> <w> <h>`、`workspace <id> <ws-id>`。每条命令输出 `ok`/`failed`。

### 输入注入
`move-cursor <x> <y>`、`event motion <x> <y>`、`event button <btn> [press|release|click]`、`event key <key> [press|release|tap]`。指针按键使用 Linux input code（`left`=0x110，`right`=0x111，`middle`=0x112）；数字键值按 **Qt::Key** 传入（不是 Linux evdev keycode），也可以使用常见名字（`enter`、`esc`、`space`、`a`–`z`、`0`–`9`、`f1`–`f12`、方向键）。键盘事件发往**键盘聚焦**的 surface —— 先 `activate` 窗口；指针按键发往光标下的 surface —— 先把光标移过去。

### 截图
`screenshot output [name] [file]` 与 `screenshot window <id> [file]`。服务端渲染，PNG 字节返回给客户端，由客户端写文件并打印路径（省略 `file` 时自动落在 `/tmp` 下）。

### Shell / listen
`shell` —— 交互式 REPL。`listen [--port <p>] [--host <a>]` —— 通过 HTTP/WebSocket 暴露同样能力，供浏览器前端使用（`/api/*`、`ws://host:port/ws`）。它只在定义了 `TREELAND_DEBUG_LISTEN` 时才编译进来，且**无认证**，默认绑定 `0.0.0.0:8080` —— 本地调试务必只绑回环：`listen --host 127.0.0.1 --port 8080`。完整 REST/WS 说明见 `tools/treeland-debug/README.md`。

### 快速上手
```bash
# 启用一次（立即生效，无需重启）—— 仅发行版构建需要；调试构建默认已开
sudo -u dde -- dde-dconfig set -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k remoteDebug -v true

# 检查 —— 发行版构建会通过 polkit 自动提权到 root；调试构建以当前用户运行
treeland-debug tree
treeland-debug --json windows
treeland-debug clients
treeland-debug idle          # 空闲时长与空闲抑制来源

# 控制
treeland-debug activate dde-file-manager
treeland-debug maximize 93824992268800
treeland-debug close dde-file-manager

# 注入输入
treeland-debug event key enter tap
treeland-debug event button left click

# 截图
treeland-debug screenshot output DP-1 /tmp/ss.png   # 语法：screenshot output <output-name> [file]
treeland-debug screenshot window 93824992268800
```

## 读日志
treeland 使用 Qt 日志分类，全部集中在 `src/common/treelandlogging.*`。分类字符串 ID 遵循 `treeland.<module>[.<submodule>]`（例如 `treeland.surface`、`treeland.input`、`treeland.workspace`、`treeland.output`、`treeland.seat`、`treeland.protocol`、`treeland.wallpaper`、`treeland.xwayland`、`treeland.shell.xdg`、`treeland.popup.focus`、`treeland.debug`）。waylib 的分类在 `waylib/src/server/wayliblogging.*`，ID 为 `waylib.*`。

用 `QT_LOGGING_RULES` 控制日志详细程度。全局模式下 `treeland.service` 是 **system** 单元，因此要通过 systemd drop-in 设置该变量（system 服务没有 `--user` 环境）。嵌套 dev 实例则在启动进程的环境里设置（见上面的 dev 构建一节）。

```bash
sudo mkdir -p /etc/systemd/system/treeland.service.d
printf '[Service]\nEnvironment=QT_LOGGING_RULES=treeland.surface.debug=true;treeland.input.debug=true\n' | sudo tee /etc/systemd/system/treeland.service.d/debug.conf
sudo systemctl daemon-reload
sudo systemctl restart treeland.service
```

注意：随包安装的 unit 设置了 `StandardOutput=null` / `StandardError=null`，treeland 自身的 stdout/stderr（包括 Qt 日志）会被**丢弃**，不会出现在 journal 里。要捕获它们，需在同一 drop-in 中追加 `StandardOutput=journal` / `StandardError=journal`。journal 本身是 system journal（不是 `--user`）：

```bash
# 全局 treeland 服务（以 dde 运行）最近的日志
sudo journalctl -u treeland.service -n 200 --no-pager
# 或带完整详细度并跟随
sudo journalctl -u treeland.service -f
```

`treeland.sh` 包装脚本：启动时检查 `--try-exec`，若以 `failed to create dri2 screen` 失败则回退到 `WLR_RENDERER=pixman` 软件渲染（无 3D 的 VirtualBox）。若某问题只在硬件渲染下复现，注意该回退可能掩盖它。

## 诊断流程
从症状出发，选最小可靠的检查手段。先复现，再逐层隔离（treeland vs waylib vs wlroots vs 客户端）。

**窗口状态不对（最大/最小/全屏/平铺、几何、可见性）：**
1. `treeland-debug --json windows` —— 读取目标窗口（按 `appId` 匹配）的 `state`、`visible`、`active`、`geometry`、`workspace`、`layer`、`frames`、`damage`。
2. `treeland-debug tree` —— 确认窗口所在的 layer/workspace。
3. `treeland-debug scene <id>` —— 布局看着对但渲染不对时，检查 QtQuick 场景（菜单/弹窗/装饰）。
4. 与日志交叉验证：`QT_LOGGING_RULES='treeland.surface.debug=true;treeland.workspace.debug=true'`。

**焦点 / 激活不对：**
1. `treeland-debug --json windows` —— 检查 `active` 标记；`treeland-debug cursor` 看指针位置。光标下的对象没有 CLI 子命令（`cursor-window`/`focused` 只暴露在 HTTP/WS）：`treeland-debug listen --host 127.0.0.1` 后 `curl -s 127.0.0.1:8080/api/cursor-window`、`curl -s 127.0.0.1:8080/api/focused`。
2. `treeland-debug activate <id|appId>` 后复查。
3. 相关分类：`treeland.seat`、`treeland.popup.focus`、`treeland.activation`。

**输入不落地 / 目标不对：**
1. `treeland-debug move-cursor <x> <y>` 后 `treeland-debug event button left click`；`treeland-debug event key enter tap`。
2. 用 `windows`（`active`）确认指针/键盘焦点 —— 键盘事件发往键盘聚焦的 surface，按键发往光标下的 surface。
3. 相关分类：`treeland.input`、`treeland.seat`、`waylib.input.pointer`、`waylib.seat`。检查 seatd：`sudo journalctl -u dde-seatd -f`，以及 `LIBSEAT_BACKEND=seatd`/`SEATD_SOCK=/run/dde-seatd.sock` 是否已设置（见 `misc/systemd/treeland.service.in`）。

**输出黑屏 / 缺失 / 分辨率不对：**
1. `treeland-debug tree` —— 确认 output 存在；`treeland-debug --json windows` 按 output 查看。
2. `treeland-debug screenshot output <name> <file>` 看合成器实际渲染了什么。
3. 相关分类：`treeland.output`、`waylib.output`、`waylib.output.drm`。硬件渲染失败时 `treeland.sh` 的 pixman 回退会生效。

**崩溃 / 卡死：**
1. 检查服务状态与重启日志：`sudo journalctl -u treeland.service -n 200 --no-pager`（先加 `StandardError=journal` drop-in，见“读日志”）。
2. 若启用了 ASAN：经 `treeland.service` 启动时日志写到 `/tmp/treeland-asan*` 文件而不是 journalctl（`ASAN_OPTIONS` 在 `misc/systemd/treeland.service.in` 里设）；本地直接跑则仍在 stderr。
3. 用 `treeland-debug`（注入事件、activate/close 窗口）以最小步骤复现，并记录崩溃前最后几个动作。
4. 卡死时用 `top`/`clients` 看是否有客户端卡住，必要时 gdb attach（`sudo gdb -p <pid>`）。

**怀疑客户端问题：** 用 `treeland-debug clients` 查客户端 pid；确认它确实在与合成器通信（`windows` 存在、`frames` 在递增）。冻结的客户端会表现为 `frames` 不再推进。

## 规则
- 发行版构建中 `treeland-debug` 会通过 polkit 自动提权到 root（root 可访问任何用户的 socket），因此不需要手动加用户前缀。调试构建中必须以与 treeland 实例**相同的用户**运行。调试 socket 仅属主可访问。传 `--no-escalate` 可在脚本/CI 中跳过 polkit 提权。
- 需要解析的状态用 `--json`；控制窗口前先用 `windows`/`clients`/`top` 查到稳定的窗口 id。
- 截图是强证据：布局 `tree` 正确但图像不对，问题在渲染/QtQuick 场景，不在布局。
- 优先修根因；实时工具用于观察与复现，不能替代阅读拥有该状态的那段代码路径。
- 需要运行时改动时优先用对应 skill：日志改动 -> `treeland-logging`；DConfig -> `treeland-dconfig`；协议 -> 协议类 skill。

## 验证

- [ ] 改动前先用一条 `treeland-debug` 命令（通常是 `--json windows` 或 `tree`）记录当前实时状态
- [ ] 若涉及代码，先构建（见 `AGENTS.md`）
- [ ] 用同一条命令复验，输出符合预期
- [ ] 在交付说明里写出：哪条 `treeland-debug` 命令可以确认这次改动
