# 用 dde-dconfig 调试

由 `SKILL.md` 按需加载：需要手动读写运行时 DConfig 值、检查元数据、或验证实时信号时读本文件。

DConfig 描述文件安装之后，用 `dde-dconfig` 做手动运行时检查。该 CLI 读取已安装的配置元数据；只改源码树里的 `misc/dconfig/*.json` 不够。先安装项目或以其他方式把描述文件装到 DSG 配置位置，通常由 `misc/dconfig/CMakeLists.txt` 安装到：

```text
${CMAKE_INSTALL_DATADIR}/dsg/configs/org.deepin.dde.treeland
```

treeland 的参数映射：

- `-a org.deepin.dde.treeland`：treeland DConfig 封装使用的 app id。
- `-r <resource>`：描述文件/配置 id，通常是描述文件名去掉 `.json`。
- `-s <subpath>`：seat、output 或应用作用域配置的 subpath。
- `-k <key>`：配置键。
- `-v <value>`：`set` 使用的值。
- `-u <uid>`：用户偏好（含动画/特效档位阈值）、seat 和应用配置必须指定其所属用户 UID。

常用命令：

```bash
# 列出已安装的可配置 app/resource/subpath 条目
dde-dconfig list

# 列出合成器全局 TreelandConfig 的键
dde-dconfig get -a org.deepin.dde.treeland -r org.deepin.dde.treeland

# 读写合成器全局键（含系统动效档位 effectLevel：none/low/medium/high）
dde-dconfig get -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k showOtherUserOption
dde-dconfig set -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k showOtherUserOption -v true
dde-dconfig set -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k effectLevel -v '"medium"'

# 读写指定用户 TreelandUserConfig 的键（含 0=none 1=low 2=medium 3=high 动画/特效阈值）
dde-dconfig get -a org.deepin.dde.treeland -u 1000 -r org.deepin.dde.treeland.user -k cursorSize
dde-dconfig get -a org.deepin.dde.treeland -u 1000 -r org.deepin.dde.treeland.user -k windowOpen
dde-dconfig set -a org.deepin.dde.treeland -u 1000 -r org.deepin.dde.treeland.user -k windowOpen -v 2

# 读取按应用 AppConfig 的键
dde-dconfig get -a org.deepin.dde.treeland -u 1000 -r org.deepin.dde.treeland.user.app -s /deepin-screen-recorder -k enablePrelaunchSplash

# 重置单个键；省略 -k 则重置该配置文件的所有缓存值
dde-dconfig reset -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k showOtherUserOption

# 验证实时信号处理时监听某个键
dde-dconfig watch -a org.deepin.dde.treeland -u 1000 -r org.deepin.dde.treeland.user -k cursorSize
```

查看元数据用带 `-m` 的 `get`：

```bash
dde-dconfig get -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k showOtherUserOption -m permissions
dde-dconfig get -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k showOtherUserOption -m visibility
dde-dconfig get -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k showOtherUserOption -m isDefaultValue
dde-dconfig get -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k showOtherUserOption -m name -l zh_CN
```

说明：

- 值由 `dde-dconfig` 解析；使用 JSON 风格取值（`true`、`false`、数字，必要时用带引号的字符串）。不要写 `ture` 这类拼写错误，即使某个本地工具版本看起来会强制接受。
- `-r org.deepin.dde.treeland` 对应 `TreelandConfig`（含系统级 `effectLevel`）。
- `-u <uid> -r org.deepin.dde.treeland.user` 对应指定用户的 `TreelandUserConfig`（含动画/特效阈值，subpath 为空）。
- `-u <uid> -r org.deepin.dde.treeland.user.seat -s /<seatName>` 对应 `SeatUserDConfig`。
- `-r org.deepin.dde.treeland.output -s /<outputName>` 对应 `OutputConfig`。
- `-u <uid> -r org.deepin.dde.treeland.user.app -s /<appId>` 对应 `WindowConfigStore` / `AppConfig`。
- `watch` 输出发生变化的键名，不会直接输出新值；新值需用 `get` 查询。它验证配置服务的变更通知；生成类的 `*Changed`、用户切换重连与实际设置生效还需结合 Treeland 的运行效果检查。
