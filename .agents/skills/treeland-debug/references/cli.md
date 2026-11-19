# treeland-debug 命令参考

`treeland-debug --help` 是命令清单的权威来源；`tools/treeland-debug/README.md` 是该工具的
完整文档（含 REST/WS/MCP 细节）。本文件只记录用这个工具排查问题时要记住的接口事实。

## 全局选项

```
--url <url>          远程对象 host URL（默认：$TREELAND_DEBUG_SOCKET，否则按构建类型探测）
--name <name>        远程对象名（默认 WindowTree）
--timeout-ms <n>     请求超时，毫秒（默认 30000）
--json               检查命令输出机器可读 JSON（与 listen API 返回的记录一致）
--preview            强制在终端内联预览截图（默认自动探测）
--no-preview         关闭内联预览
--no-escalate        不走 polkit 提权，只报告需要 root（脚本/CI 用）
--log-level <level>  本客户端自身 stderr 诊断的详细度：error、warning、info（默认）、
                     debug。目前只有 info 与 debug 有差别，debug 还会标出回退连接的来源
-h, --help           帮助
-v, --version        版本
```

socket 选择顺序与探测细节见 [access.md](access.md)。`--tree`、`--cursor` 是 `tree`、`cursor`
的向后兼容别名；不带命令时默认执行 `tree`。

## 检查

| 命令 | 输出 |
| --- | --- |
| `tree` | 完整布局树：`Mode:` + 每层 `Layer: <name> (<layer>)` + workspace（`(active)` 标记）+ 一行一窗（`*` 标记 active 窗口，state 带 `+Min` 后缀表示 minimized）。 |
| `cursor` | xdotool 风格一行：`x:N y:N window:N`，`window 0` 表示光标下没有窗口。`--json` 为 `{x,y,window}`。 |
| `windows` | 顶层窗口表格：`ID TYPE APP-ID STATE ACTIVE MIN OUTPUT GEOMETRY TITLE`；`TYPE` 为 shell surface 类型（`xdg-toplevel`/`xdg-popup`/`xwayland`/`layer-shell`/`input-popup`/`lock-screen`/`splash-screen`），`STATE` 为 `Normal`/`Maximized`/`Fullscreen`/`Tiling`，`OUTPUT` 里的 `+` 标记驱动该窗口 frame callback 的 output。 |
| `window <id\|appId>` | 单窗口 `Key: value` 块（`xwininfo -id` 风格）：`App ID`、`Shell type`、`Container`、`Layer`（含 `Z`）、`Workspace`、`State`、`Visible`、`Active`、`Minimized`、`Output`、`Outputs`、`Frame pacing`、`Geometry`、`Position`、`Bounding rect`、`Titlebar`、`Icon`、`Frames`、`Damage`、`Input region`、`Opaque region`，以及"为什么不在任何 output 上"的 `Last leave`（离开的 output、离开时是否 enabled、是否清空了它的窗口列表）与 `Last outputs update`（原因、启用/总数/已计算数量、是否自动模式）；末行是可直接贴报告的 `-geometry WxH+X+Y`。空字段直接省略。 |
| `clients` | 每个客户端一行 `client 0x<id> pid=<pid> [appId=<id>] windows=<n>`，下面跟 `exe:`、`cmd:` 与该客户端的窗口行。 |
| `idle` | `Key: value` 形式：`Idle`（距上次输入的时长）、`Inhibited`、`Screensaver`、`Inhibitors`，后面一行一个抑制来源（`[kind] pid= appId= window= "title" app= reason= active= held= exe=`）。列表**包含当前未生效**的来源，用 `active=` 区分，只有 `active` 的才算进 `Inhibited`。`kind` 为 `idle-inhibit`（`zwp_idle_inhibit_manager_v1` surface）或 `screensaver`（`treeland_screensaver_v2` inhibit）。`surfaceId`/`window=` 只有在抑制挂在 toplevel 上时才等于可控制命令的窗口 id（layer-shell / subsurface 抑制查不到窗口）。合成器没有全局空闲阈值（阈值由客户端通过 `ext-idle-notify-v1` 自行设定），所以只报告时长与抑制来源。 |
| `shortcuts` | 已注册的快捷键与手势，表格 `NAME TRIGGER ACTION FLAGS`：按键绑定触发列为 `Ctrl+Alt+T` 形式、`FLAGS` 为 `press\|release\|repeat`；手势为 `swipe <n>-finger <direction>` 或 `hold <n>-finger`，`FLAGS` 留空。`--json` 为 `{name,type,key,flags,action,fingers,direction}`，`type` 0=key 1=swipe 2=hold。 |
| `top [interval-ms]` | 类似 `top` 的实时视图（默认 1000ms，Ctrl+C 退出），按每间隔帧增量降序排：顶部 `idle`/`inhibited`/抑制来源计数行，表头 `ID TYPE APP-ID STATE MIN FRAMES GEOMETRY MARKER`，`MARKER` 为 `F (focus)`（键盘聚焦）、`C (under cursor)`（光标下）或空。 |
| `events [interval-ms]` | 输入事件实时流（默认 50ms），每行先打事件类型名，同时带快捷键注册/注销/触发事件。类型编号见下。 |
| `watch <id> [interval-ms]` | 轮询单个窗口的变化（默认 250ms）。 |
| `scene [id]` | 单个窗口（按 id/appId）或整个场景的 QtQuick 场景树 —— 用于查看不在布局 `tree` 里的菜单/弹窗/装饰。`--json` 为 `{"sceneTree": "..."}`。 |

每个窗口都有稳定的数字 `id`（以及 `appId`），所有控制命令都接受这两种标识。用
`windows`/`clients`/`top` 查 id。

### 事件类型编号

`1` motion、`2` button、`3` key、`4` focus、`5` cursor-window、`6` shortcut-registered、
`7` shortcut-unregistered、`8` shortcut-triggered（定义见
`src/modules/resource/treelandwindowtree.rep`）。`shortcut-triggered` 的 `target` 是按键触发
那一刻的键盘聚焦窗口（动作执行前采样），手势触发带 `(gesture)` 且 `target` 为 0。

### `--json` 覆盖范围

`tree`、`cursor`、`windows`、`window`、`clients`、`idle`、`shortcuts`、`no-auth`、`scene`
输出 JSON，内容与 `listen` 的 HTTP/WS/MCP 接口一致。`top`、`events`、`watch`、`screenshot`
没有 JSON 形式（它们是流或二进制）。

## 输出约定

人类可读输出统一按 x11 工具的习惯排版：

- **列表**（`windows`、`clients`、`top`、`shortcuts`）是表头 + 每对象恰好一行，无续行，方便
  `grep`/`head` 处理。
- **几何**是 `WxH+X+Y`（`xwininfo -geometry` / `xdotool getwindowgeometry` 顺序），负坐标保留
  自己的符号（`800x600-10+50`），不是从右边缘算的偏移。
- **单个对象**（`window`、`idle`）是 `Key: value` 逐行块，键补齐对齐。
- **嵌套结构**（`tree`、`scene`）保留树枝字符。
- **来源 socket**：每次建立连接的运行结束时都在 stderr 上说明数据来自哪个 socket
  （`treeland-debug: connected to local:...`），`shell` 进入时则作为 login 信息给出同一句话；
  `help`、`--version`、`debug`、`listen` 不建立调试连接，不输出这行。`(fallback from ...)`
  只有在 `--log-level debug` 下才出现（`error`/`warning` 目前与 `info` 无差别）。

## 窗口控制

`activate`、`close`、`minimize`、`maximize`、`fullscreen`、`move <id> <x> <y>`、
`resize <id> <w> <h>`、`workspace <id> <ws-id>`。每条命令输出 `ok`/`failed`；`maximize` 与
`fullscreen` 是切换语义。目标接受数字 id 或 appId；appId 命中多个窗口时，stderr 会说明
显示了哪一个。

## 输入注入

- `move-cursor <x> <y>`、`event motion <x> <y>` —— 移动光标。
- `event button <btn> [press|release|click]` —— 指针按键，`btn` = `left`/`right`/`middle`
  或数字；名字映射到 Linux input code（`left`=0x110、`right`=0x111、`middle`=0x112），
  默认动作 `click`。指针按键发往**光标下**的 surface —— 先把光标移过去。
- `event key <key> [press|release|tap]` —— 键盘事件，默认 `tap`。`key` 可以是 **Qt::Key
  枚举名**（大小写不敏感，可带或不带 `Key_` 前缀，如 `Return`、`Escape`、`Space`、`A`、
  `F1`、`Left`），也可以是**数字**；数字按 Qt::Key 值解释，**不是** Linux evdev keycode。
  键盘事件发往**键盘聚焦**的 surface —— 先 `activate` 窗口。

## 截图

`screenshot output [name] [file]`（省略 name 取主 output）与 `screenshot window <id> [file]`。
服务端渲染，PNG 字节返回给客户端，由客户端写文件并打印路径；省略 `file` 时落在
`/tmp/treeland-debug-<毫秒时间戳>.png`，给了不带后缀的路径会自动补 `.png`。`--preview`/
`--no-preview` 控制是否在终端内联显示。

## 交互式 shell

`shell` 启动 REPL（`help` 看命令、`exit`/`quit` 退出），所有命令（含 `top`/`events`/`watch`）
都可用；Ctrl+C 中断实时视图并回到提示符。Tab 补全命令动词与 `event`/`screenshot`/`debug`/`listen`
的首个参数，↑/↓ 翻历史，提示符下 Ctrl+C 丢弃当前行、Ctrl+D 退出（管道输入仍按行读取）。
一次性命令行的补全定义在 `tools/treeland-debug/completions/`（bash/zsh/fish）。`debug [on|off|status]`
在 REPL 里不可用 —— 它可能需要转发 polkit 提权，只能以一次性命令运行。

三份脚本与命令表由 `tests/test_treeland_debug` 的 `testCompletionScriptsMatchCommandTable()` 逐项
比对（动词 = `debugCommandNames()` 去掉 REPL 专属的 `exit`/`quit`，首参 = `completionCandidates()`），
所以**新增/改名命令或首参时必须同步改这三份脚本**，否则该测试失败。

## 访问

`no-auth` —— 请合成器放开调试 socket 30 分钟（详见 [access.md](access.md)）。

## listen（HTTP / WebSocket / MCP）

`listen [--port <p>] [--host <addr>]`（默认 `0.0.0.0:8080`，端口范围 1-65535）通过 HTTP、
WebSocket 与 MCP 暴露同样的能力，供浏览器前端或其它程序使用。`listen` **始终编译进来**
（`Qt6::HttpServer` 与 `Qt6::WebSockets` 是硬依赖，见 `tools/treeland-debug/CMakeLists.txt`）。

- REST：`/api/tree`、`/api/cursor`、`/api/windows`、`/api/window`、`/api/clients`、`/api/idle`、
  `/api/shortcuts`、`/api/focused`、`/api/cursor-window`、`/api/scene`、`/api/no-auth`、
  `/api/debug`、`/api/events`、`/api/screenshot/output`、`/api/screenshot/window`，以及
  `POST /api/{activate,close,minimize,maximize,fullscreen,move,resize,workspace}`、
  `POST /api/move-cursor`、`POST /api/event/{motion,button,key}`。
- WebSocket：`ws://host:port/ws`。
- MCP：`POST /mcp`（JSON-RPC `initialize` / `tools/list` / `tools/call`）。

它**没有认证**，所以本地调试务必只绑回环：`listen --host 127.0.0.1 --port 8080`。`listen` 在
任何连接建立前就启动服务器，因此它自己不会提权 —— Release 构建下要以 root（或 `dde` 在
no-auth 窗口内）运行；它内部的 `debug` 写入不转发提权，需要 root 时按错误报告。
