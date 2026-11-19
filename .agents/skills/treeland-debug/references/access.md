# 调试访问权限与 socket 位置

本文件是 treeland 调试通道权限模型的唯一出处。全部结论可在仓库内核对：
`src/modules/resource/treelanddebugsocket.cpp`、`src/modules/resource/treelandremotesource.cpp`、
`misc/systemd/treeland.service.in`、`misc/polkit/org.deepin.dde.treeland-debug.policy.in`。

## 构建类型

构建类型决定 socket 名、位置、权限，以及客户端要不要提权。`TREELAND_DEBUG_DEV_BUILD`
只在 `CMAKE_BUILD_TYPE=Debug` 时定义（见顶层 `CMakeLists.txt`）；没给 `CMAKE_BUILD_TYPE`
时两种都不是，按 Release 行为处理。

| | Release 构建 | Debug 构建（`TREELAND_DEBUG_DEV_BUILD`） |
| --- | --- | --- |
| socket 名 | `org.deepin.dde.treeland.debug` | `org.deepin.dde.treeland.debug-dev` |
| 默认目录 | 全局服务 `/run/treeland-debug`；会话模式 `$XDG_RUNTIME_DIR`（`/run/user/<uid>`） | `/tmp/treeland-debug-<uid>`（目录 `0700`，属主为该 UID） |
| socket 权限 | `0000`（只有 root 靠 `CAP_DAC_OVERRIDE` 连得上，连合成器自己的用户也挡） | `0600`（仅所属 UID 的进程，root 保留特权访问） |
| `remoteDebug` 开关 | 生效，默认关闭 | 忽略，始终开启 |
| 客户端提权 | 连不上就通过 `pkexec`（polkit）以 root 重跑自己 | 不提权；同 UID 直接连 |

名字冲突时追加数字后缀（`-1`…`-99`），用非阻塞 `flock` 加活 socket 探测选择，避免覆盖
另一个实例的 socket。Debug 构建的 `-dev` 名与 Release 名互不干扰，所以嵌套 dev 实例
可以和全局 `treeland.service` 同时存在。

## 调试目录

- 全局服务用**自己的目录** `/run/treeland-debug`，不是进程的 `XDG_RUNTIME_DIR`（后者是
  `/run/treeland`）：no-auth 窗口要把目录放宽到 `0711`，而 Qt 要求 `XDG_RUNTIME_DIR`
  保持 `0700`，否则会退回 `$TMPDIR/runtime-<user>`。systemd 用
  `RuntimeDirectory=treeland treeland-debug` 创建这个目录，并在每次启动时重建成 `0700`。
- 会话模式（`TREELAND_RUN_MODE=user`，或全局目录不可写）绑定在进程的 `XDG_RUNTIME_DIR`。
- Debug 构建绑定 `/tmp/treeland-debug-<uid>`，不往宿主的 `/run` 里放 socket。

## 客户端探测顺序

`--url` → `$TREELAND_DEBUG_SOCKET` → 自动探测。自动探测按目录顺序逐个尝试候选，每个
候选后面跟它自己的 `-N` 实例：

1. 自己的目录（Debug 构建是 `/tmp/treeland-debug-<uid>`；Release 构建是全局目录）
2. `/run/treeland-debug`
3. `/run/treeland`（legacy 全局目录）
4. runtime 目录：Debug 构建只加自己的；Release 构建扫描 `/run/user/*`（root 可访问任何用户的会话 socket）

Debug 构建的候选名顺序是 `-dev` 再 Release 名，因此构建类型偏好优先于后缀实例。
`--timeout-ms` 在各候选间共享，每个候选至少留约 3 秒的探测预算；一个死 socket 只会
拖慢而不会饿死首选实例，所以探测总耗时可达 `--timeout-ms` 加每个候选一次探测。
显式给了 `--url` 或 `$TREELAND_DEBUG_SOCKET` 就只用它，不再探测其它候选。

`$TREELAND_DEBUG_SOCKET` 取 socket 路径或裸名（等价 `local:<值>`），带 scheme 的值原样
使用。`TREELAND_DEBUG_DIR` 是显式开发/测试覆盖项，同时作用于服务端目录选择。

## 提权与 `--no-escalate`

Release 构建里客户端先以自身身份尝试连接；非 root 连不到 root-only 的 socket，所以这次
尝试会立即失败，随后才通过 `pkexec`（polkit）重新执行自己并弹出认证。随包安装的
polkit 策略保证开箱即用：源文件 `misc/polkit/org.deepin.dde.treeland-debug.policy.in`，
安装为 `org.deepin.dde.treeland-debug.policy`（`/usr/share/polkit-1/actions`）。

`--no-escalate` 跳过 polkit，直接报告需要提权 —— 脚本和 CI 用这个，避免挂起等认证。
`pkexec` 不可用时，非 root 调用者会得到一条说明如何继续的错误。

## `no-auth`：临时放开 Release socket

`treeland-debug no-auth`（`WindowTreeRemote` 的 `requestNoAuth` 方法）请 treeland 把 socket
置为 `0666`、它自己的调试目录置为 `0711`（可穿越不可列目录），之后任意本机用户都能直接
连上、不弹认证。

- 发请求的必须是已经穿过 `0000` socket 的调用者 —— Release 构建里那只有 root，这次连接
  本身就是鉴权。
- 窗口 30 分钟内无调试调用就关闭，期间每次远程调用（包括 `cursor` 的属性读取）都会重新
  推后 30 分钟；关闭时窗口期内建立的连接也被一并断开（重建 host）。
- 收窗只把 socket 收回 `0000`。目录在第一次开窗时被放宽并保持 `0711` 到进程退出
  （多个实例可能共用这个目录），下次启动由 systemd 重建成 `0700`。
- 只对全局服务自己的目录生效。会话模式下 socket 在 `/run/user/<uid>`（那里还有会话总线等），
  不放宽；`no-auth` 在那里失败，仍然需要 root。
- Debug 构建本来就在启动时放开到同 UID，`requestNoAuth` 是空操作、直接成功。

## 需要 root 之外提权的一条命令

`debug [on|off|status]` 是唯一不需要调试 socket 的命令（socket 本来就是它开出来的）。
开关按用户存放，所以合成器不是以你的身份运行时它会自己走 `pkexec`；`--no-escalate`
只报告需要提权。REPL（`shell`）不能转发提权，因此 `shell` 里的 `debug` 只提示以一次性
命令运行。
