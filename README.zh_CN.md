# Treeland

treeland 是一个基于 wlroots 和 QtQuick 开发的 Wayland 合成器，旨在提供高效且灵活的图形界面支持。

## 依赖

查看 `debian/control` 文件来了解具体的构建与运行时依赖，或者使用 `cmake` 检查缺失的必要组件。

核心构建依赖：

- [waylib](https://github.com/vioken/waylib) 整合 wlroots 和 QtQuick 的 Wayland 合成器开发库
  - Qt >= 6.8.0
  - wlroots = 0.20
- [treeland-protocols](https://github.com/linuxdeepin/treeland-protocols) treeland 使用的私有 wayland 协议

推荐的运行时依赖：

- [ddm](https://github.com/linuxdeepin/ddm) 为多用户优化的登录管理器

## 构建

treeland 使用 cmake 进行构建，`WITH_SUBMODULE_WAYLIB` 选项可以强制使用子模块中的 `waylib` 代码，如果希望使用系统提供的 `waylib` 应该设置为 `OFF`。

使用系统库提供的 `waylib`：

```shell
$ git clone git@github.com:linuxdeepin/treeland.git
$ cd treeland
$ cmake -Bbuild -DWITH_SUBMODULE_WAYLIB=OFF
$ cmake --build build
```
使用子模块中的 `waylib`：

```shell
$ git clone git@github.com:linuxdeepin/treeland.git --recursive
$ cd treeland
$ cmake -Bbuild -DWITH_SUBMODULE_WAYLIB=ON
$ cmake --build build
```

## 打包

在 *deepin* 桌面发行版进行此软件包的构建，我们还提供了一个 `debian` 目录。若要构建软件包，可参照下面的命令进行构建：

```shell
$ sudo apt build-dep . # 安装构建依赖
$ dpkg-buildpackage -uc -us -nc -b # 构建二进制软件包
```

## treeland-debug

`treeland-debug` 是面向运行中 Treeland 合成器的 adb 式命令行检查与控制工具。它通过
Qt Remote Objects 连接到 Treeland 的调试 Remote Object，提供检查窗口树、控制窗口、
注入输入事件、抓取截图等子命令。类似 `adb`，它有两种模式：

- **非 shell（单次执行）模式** —— 默认：`treeland-debug <command> [args]`。
- **shell 模式** —— 交互式 REPL：`treeland-debug shell`。

客户端随 Treeland 正常构建一起编译（默认开启；传 `-DBUILD_TREELAND_DEBUG=OFF` 可跳过）：

```bash
cmake -B build
cmake --build build --target treeland-debug
sudo cmake --install build --component treeland-debug
```

### 开启调试 source

调试 Remote Object 默认关闭：它会把窗口内容和输入注入能力交出去，跑在用户会话里的合成器
不应默认提供。`remoteDebug` 是它的总开关：开关为开时，Treeland 会
发布调试 socket，并给连上来的客户端提供窗口树远程数据源——该数据源是个薄 QObject，
只在每次调用时干活，没有客户端连着时不占资源。把开关关掉就完全禁止调试，socket 也会
一起消失。Release 构建中 socket 只允许 root 连接（权限 0000）；Debug 构建
（`CMAKE_BUILD_TYPE=Debug`）忽略这个开关，始终发布 `-dev` socket 到
`/tmp/treeland-debug-<uid>`（目录 0700，socket 0600），只允许合成器所属 UID 访问。
以同一 UID 运行 Debug 客户端即可连接，不需要 polkit；Debug 构建即使处于 user mode
也使用这个 dev 目录。`TREELAND_DEBUG_DIR` 可显式覆盖开发目录。Release 会话实例的
socket 仍位于 `$XDG_RUNTIME_DIR`，在那里只有所属用户（和 root）能连接。

Release 构建的 root 限制可以通过调试协议本身临时放开：`treeland-debug no-auth`
（即 `WindowTreeRemote` 的 `requestNoAuth` 方法）请求合成器把它自己的调试目录开到 0711、
socket 开到 0666，任意本机用户都能连上。只有已经穿过 0000 socket 的调用者才能发这个
请求，而在 Release 构建里那只有 root：0000 连合成器用户自己也挡住，只有
`CAP_DAC_OVERRIDE` 能过。这次连接本身就是鉴权——所以开窗总要 root 过一次 `pkexec` 认证，
而窗口期内连上来的用户全程无需密码。窗口 30 分钟后自动关闭；
开着的每一次远程调用都会把窗口重新推后 30 分钟，所以忘记关闭也不会比「闲置半小时」敞开
更久。关闭时窗口期内建立的连接也会被一并断开，所以挂着一个客户端并不能把窗口留住。

只有全局服务会得到这个窗口，而且只放宽它自己的目录：`/run/treeland-debug`
（`RuntimeDirectory=treeland-debug`），绝不是它的 `$XDG_RUNTIME_DIR`（`/run/treeland`）——
Qt 要求后者保持 0700，否则新会话的 wayland socket 会落到 `$TMPDIR`。会话模式下
Treeland 位于 `/run/user/<uid>` 里，该目录同时还放着会话总线和其它会话 socket，因此
那里不会放宽目录权限，仍然需要 root。

窗口只存在于内存里，进程不在就什么都不剩：崩溃或重启回来就是 root-only，即使上一个
进程带着打开的窗口死掉，启动时也会重新收回每个 socket。目录本身只在第一次开窗时放宽一次
（0711，可穿越不可列目录），之后一直保持到进程退出——多个实例可能共用这个目录，一个实例
收窗不能把另一个还在开窗的实例切断；systemd 下次启动时会把目录重建为 0700。实际把门的是
socket 自身的权限位。窗口开着时可以同时挂多个客户端。如果读不到 socket，要么
开关被关了，要么 Treeland 没在运行——客户端会直接在 stderr 上说明并指向这个开关。
切换 `remoteDebug` 即时生效：远程源会动态创建或销毁，无需重启。

用 `treeland-debug debug on` / `treeland-debug debug off` 切换（不带参数的 `debug`
打印当前状态）。开关是按用户存放的 DConfig 键，Treeland 只读自己运行用户的那个，因此该
命令会从运行中的合成器查到这个用户，不是你本人时通过 `pkexec` 提权（`--no-escalate`
只报告需要提权而不弹认证）。它在底层以 root 执行等价于：

```bash
dde-dconfig -u <合成器 uid> set \
  -a org.deepin.dde.treeland \
  -r org.deepin.dde.treeland \
  -k remoteDebug \
  -v true
```

下文所有命令均以 `dde` 用户运行，例如 `sudo -u dde -- treeland-debug windows`。

### socket 命名与连接

Treeland 在本地 socket 上发布调试源，默认名为 `org.deepin.dde.treeland.debug`，绑定在
合成器自己的目录里的绝对路径上：全局 Release 服务为
`/run/treeland-debug/org.deepin.dde.treeland.debug`，Release 会话模式为
`/run/user/<uid>/...`（即 `$XDG_RUNTIME_DIR`）。Debug dev 构建（包括 user mode）默认使用
`/tmp/treeland-debug-<uid>/...`（目录 0700、socket 0600），普通用户只有所属 UID 可访问；
root 仍保留特权访问。这样可以避免写入宿主 `/run`；`TREELAND_DEBUG_DIR` 是显式开发/测试
覆盖项。全局 Release 目录由 systemd 以 0700 创建，且刻意不使用 `$XDG_RUNTIME_DIR`——
免鉴权窗口会把它开到 0711，而 Qt 要求 runtime 目录保持 0700。

Debug 客户端先探测自己的 dev 目录，再探测共享的 `/run/treeland-debug`、legacy
`/run/treeland` 和自己的 runtime 目录，不枚举其他 UID 的 runtime socket。Release root
客户端保留原有探测顺序，并可列出每个 `/run/user/<uid>` 目录。
当该名称已被另一个正在运行的 treeland（全局服务、会话服务、本地构建）占用时，会自动追加数字
后缀（`org.deepin.dde.treeland.debug-1`、`-2`…），通过非阻塞 advisory lock 选择
（与 libwayland 选择 Wayland socket 的方式一致）。崩溃实例残留的 socket 文件会被
回收，而无锁文件的旧版本实例会被探测并跳过。

Debug 构建使用不同的默认名（`org.deepin.dde.treeland.debug-dev`），因此调试版与正式版
treeland 可同时运行。Debug 构建的 treeland-debug 客户端自动优先连接调试实例，连不上时
回退到正式版实例；Release 构建的客户端连接正式版实例。socket 的选择顺序是：`--url`、
`$TREELAND_DEBUG_SOCKET`（socket 路径或裸名，等价于 `local:<值>`；带 scheme 的值原样使用）、
然后是上面的自动探测。显式 URL 始终优先，不做任何隐式处理 —— 它也是连接旧版 treeland
留在迁移前 `/tmp/<name>` 的 socket 的方式：`--url local:/tmp/org.deepin.dde.treeland.debug`。

### 全局选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `--url <url>` | `$TREELAND_DEBUG_SOCKET`，否则自动：Debug 构建优先调试 socket 并回退到正式版 socket，再否则正式版 socket | Remote object host URL。 |
| `--name <name>` | `WindowTree` | Remote object 名称。 |
| `--timeout-ms <n>` | `30000` | 请求超时（毫秒，非负整数）。 |
| `--json` | 关 | 为 `tree`/`cursor`/`windows`/`clients` 输出机器可读 JSON。 |
| `--preview` | 自动 | 强制在终端内联显示截图预览（默认自动检测终端）。 |
| `--no-preview` | 关 | 关闭终端内联截图预览。 |
| `-h, --help` | — | 显示帮助。 |
| `-v, --version` | — | 显示版本。 |
| `--tree` / `--cursor` | — | `tree` / `cursor` 命令的向后兼容别名。 |

### 命令参考

窗口控制命令的目标可用**数字 `id`**（由 `windows`/`clients`/`top` 打印）或 **`appId`**
（取第一个匹配的窗口）指定。

#### 检查

| 命令 | 参数 | 输出 |
| --- | --- | --- |
| `tree` | _（无，默认）_ | 布局树（人读格式；`--json` 输出 JSON）。 |
| `cursor` | _（无）_ | 光标位置 `x=… y=…`（`--json` 输出 `{"x","y"}`）。 |
| `windows` | _（无）_ | 窗口表格；`--json` 输出 JSON 数组。 |
| `clients` | _（无）_ | 客户端 + 窗口表格；`--json` 输出 JSON 数组。 |
| `top` | `[interval-ms]`（默认 1000） | 实时刷新的 top 式客户端视图（Ctrl+C 退出）。 |

#### 窗口控制

| 命令 | 参数 | 输出 |
| --- | --- | --- |
| `activate` | `<id>` | `ok` / `failed`。 |
| `close` | `<id>` | `ok` / `failed`。 |
| `minimize` | `<id>` | `ok` / `failed`。 |
| `maximize` | `<id>` | `ok` / `failed`（切换最大化）。 |
| `fullscreen` | `<id>` | `ok` / `failed`（切换全屏）。 |
| `move` | `<id> <x> <y>` | `ok` / `failed`。 |
| `resize` | `<id> <w> <h>` | `ok` / `failed`。 |
| `workspace` | `<id> <ws-id>` | `ok` / `failed`（移动窗口到工作区）。 |

#### 输入 / 事件注入

| 命令 | 参数 | 输出 |
| --- | --- | --- |
| `move-cursor` | `<x> <y>` | `ok` / `failed`。 |
| `event motion` | `<x> <y>` | `ok` / `failed`。 |
| `event button` | `<left\|right\|middle\|code> [press\|release\|click]` | `ok` / `failed`（默认 `click`）。 |
| `event key` | `<name\|evdev-code> [press\|release\|tap]` | `ok` / `failed`（默认 `tap`）。 |

指针按键使用 Linux input 码（`left`=0x110、`right`=0x111、`middle`=0x112，或数字码）。
键盘按键使用 Linux evdev 键码；支持常见名称（`esc`、`enter`、`space`、`tab`、`a`–`z`、
`0`–`9`、方向键、`f1`–`f12`、`home`/`end`/`pageup`/`pagedown`/`insert`/`del` 等），也可直接传
原始码。按键发送到当前键盘焦点 surface——先用 `activate` 激活目标窗口；指针按键发送到
光标所在 surface。

#### 图像抓取

截图在服务端渲染后以 PNG 字节返回给 `treeland-debug` 客户端，由客户端写入文件并打印路径；合成器本身不碰文件系统。省略 `file` 时在 `/tmp` 下自动生成路径。

| 命令 | 参数 | 输出 |
| --- | --- | --- |
| `screenshot output` | `[name] [file]` | PNG 文件路径（stdout）。 |
| `screenshot window` | `<id> [file]` | PNG 文件路径（stdout）。 |

#### 交互

| 命令 | 参数 | 输出 |
| --- | --- | --- |
| `shell` | _（无）_ | REPL（`treeland>`），接受所有命令及 `help`/`exit`。 |
| `help` | _（无）_ | 完整帮助文本。 |

### 输出格式

`tree` 和 `cursor` 默认输出人读格式；传 `--json` 输出机器可读 JSON。`windows` 和
`clients` 同样默认输出人读表格，加 `--json` 输出 JSON 数组。

窗口 JSON 对象（`WindowInfo`）：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | integer | 稳定窗口 id（wl_surface 的 wl_resource 指针），全局唯一，可被控制命令使用。 |
| `appId` | string | 应用 id。 |
| `title` | string | 窗口标题。 |
| `output` | string | 输出名。 |
| `container` | string | 容器名。 |
| `workspace` | integer | 工作区 id。 |
| `layer` | integer | 层 id。 |
| `z` | integer | Z 序。 |
| `type` | integer | 窗口类型。 |
| `state` | integer | `0` Normal、`1` Maximized、`2` Minimized、`3` Fullscreen、`4` Tiling。 |
| `visible` | boolean | 可见性。 |
| `active` | boolean | 是否激活/聚焦。 |
| `geometry` | object | `{"x","y","width","height"}`。 |
| `titlebarGeometry` | object | 同上结构。 |
| `boundingRect` | object | 同上结构。 |
| `iconGeometry` | object | 同上结构。 |
| `position` | object | `{"x","y"}`。 |
| `frames` | integer | 已提交帧数（取自 `wlr_surface`）。 |
| `damage` | object | 上次提交的缓冲区刷新区域 `{"x","y","width","height"}`。 |
| `inputRegion` | array of object | 输入区域，surface 局部坐标的 box 列表（`[{"x","y","width","height"}, ...]`），已按 surface 尺寸裁剪。按协议，未设置（`NULL`）的输入区域是**无穷大**，因此读回来是覆盖整面的一个 box；只有客户端提交了空 region（表示该 surface 不接受输入）或 surface 尚未提交过 buffer 时，列表才为空。 |
| `opaqueRegion` | array of object | 不透明区域，结构同上；为空表示客户端没有声明不透明区域（协议默认值，首次 commit 前也是这个值）；它只是重绘优化提示，不代表窗口真的透明。 |

客户端 JSON 对象（`ClientInfo`）：`id`（integer）、`appId`（string）、`pid`（integer，不可用时为 `0`）、
`executable`（string）、`windows`（`WindowInfo` 数组）。

`tree` JSON 结构为
`{"currentMode": str, "layers": [{"name", "layer", "windows": [WindowInfo], "workspaces": [{"id", "isActive", "windows": [WindowInfo]}]}]}`。

### 退出码

成功返回 `0`；连接失败、RPC 失败、未知命令或控制命令返回 `failed` 时返回 `1`。

### 使用示例

以下命令均以 `dde` 用户运行：

```bash
# 查看窗口树（人读格式，默认命令）
sudo -u dde -- treeland-debug tree

# 光标位置
sudo -u dde -- treeland-debug cursor
# → x=960 y=540

# 列出窗口
sudo -u dde -- treeland-debug windows

# 列出客户端及其窗口
sudo -u dde -- treeland-debug clients

# 实时刷新的 top 视图（1 秒间隔）
sudo -u dde -- treeland-debug top
# （Ctrl+C 退出）

# 按 id 激活窗口
sudo -u dde -- treeland-debug activate 1407374883553280

# 按 appId 激活窗口
sudo -u dde -- treeland-debug activate dde-file-manager

# 移动窗口
sudo -u dde -- treeland-debug move 1407374883553280 100 200

# 调整窗口大小
sudo -u dde -- treeland-debug resize 1407374883553280 800 600

# 移动窗口到工作区 2
sudo -u dde -- treeland-debug workspace 1407374883553280 2

# 移动光标
sudo -u dde -- treeland-debug move-cursor 960 540

# 发送一次指针点击
sudo -u dde -- treeland-debug event button left click

# 发送一次按键
sudo -u dde -- treeland-debug event key enter tap

# 截图主输出到文件
sudo -u dde -- treeland-debug screenshot output /tmp/ss.png

# 截取窗口（终端支持时内联预览）
sudo -u dde -- treeland-debug screenshot window 1407374883553280

# 交互式 shell 模式
sudo -u dde -- treeland-debug shell
treeland> help
treeland> windows
treeland> exit

# 机器可读 JSON 输出
sudo -u dde -- treeland-debug --json windows
sudo -u dde -- treeland-debug --json cursor
```

`top` 视图使用 QTimer 按 `interval-ms`（默认 1000 毫秒）周期刷新。每个周期通过
Qt Remote Objects 调用 `getClients()`，清屏（`\033[2J\033[H`）后重新打印带当前
时间戳与客户端数量的表头，随后是客户端 + 窗口表格。按 Ctrl+C 退出。
## GitHub Actions / 持续集成

本项目使用 GitHub Actions 进行持续集成。配置了以下工作流：

- **waylib 构建**：当 `waylib/**` 文件被修改时触发
- **treeland 构建**：主项目构建

## 参与贡献

- [通过 GitHub 发起代码贡献](https://github.com/linuxdeepin/treeland/)
- [通过 GitHub Issues 与 GitHub Discussions 汇报缺陷与反馈建议](https://github.com/linuxdeepin/developer-center/issues/new/choose)

## 许可协议

**Treeland** 使用 Apache-2.0, LGPL-3.0-only, GPL-2.0-only 或 GPL-3.0-only 许可协议进行发布。
