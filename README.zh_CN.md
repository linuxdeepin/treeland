# Treeland

treeland 是一个基于 wlroots 和 QtQuick 开发的 Wayland 合成器，旨在提供高效且灵活的图形界面支持。

## 依赖

查看 `debian/control` 文件来了解具体的构建与运行时依赖，或者使用 `cmake` 检查缺失的必要组件。

核心构建依赖：

- [waylib](https://github.com/vioken/waylib) 整合 wlroots 和 QtQuick 的 Wayland 合成器开发库
  - Qt >= 6.8.0
  - wlroots = 0.19
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

调试 Remote Object 默认关闭。请以 `dde` 用户开启 `debugSource` DConfig 选项
（Treeland 在 global 模式下以 `dde` 身份运行，其本地 socket 仅属主可访问），随后重启
Treeland：

```bash
sudo -u dde -- dde-dconfig set \
  -a org.deepin.dde.treeland \
  -r org.deepin.dde.treeland \
  -k debugSource \
  -v true
```

下文所有命令均以 `dde` 用户运行，例如 `sudo -u dde -- treeland-debug windows`。

### 全局选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `--url <url>` | `local:org.deepin.dde.treeland.debug` | Remote object host URL。 |
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

截图在服务端渲染并写入 PNG 文件，打印文件路径。省略 `file` 时在 `/tmp` 下自动生成路径。

| 命令 | 参数 | 输出 |
| --- | --- | --- |
| `screenshot output` | `[name] [file]` | PNG 文件路径（stdout）。 |
| `screenshot window` | `<id> [file]` | PNG 文件路径（stdout）。 |
| `screenshot screen` | `[file]` | PNG 文件路径（stdout）。 |

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
| `id` | integer | 稳定窗口 id，可被控制命令使用。 |
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

客户端 JSON 对象（`ClientInfo`）：`id`（integer）、`pid`（integer，不可用时为 `0`）、
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
sudo -u dde -- treeland-debug screenshot screen /tmp/ss.png

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
