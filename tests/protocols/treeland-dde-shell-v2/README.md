# `treeland-dde-shell-v2` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-dde-shell-v2/`
- Fixture：headless output、普通 `wl_surface` 和真实 mapped XDG toplevel
- 覆盖等级：**E / P / I**——真实 C 客户端检查 `role` 事件载荷，并读取生产
  `SurfaceWrapper` 的置顶状态与 z 层级；其余元数据与生命周期通过协议对象状态验证。
- 用户操作通过 fixture 调用与窗口菜单相同的 `SurfaceWrapper::setAlwaysOnTop()`，
  不直接设置协议状态或发送事件；本测试不模拟鼠标点击菜单。

v2 适配期间 v1 全局对象保留并行注册；本测试只覆盖
`dde/treeland-dde-shell-unstable-v2.xml` 的 manager/surface 两个接口。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| 创建 | `get_shell_surface(surface)` | 服务端创建 `DDEShellSurfaceV2`，默认 role 为 overlay（值 1） |
| role | `set_role(overlay=1)` | 服务端 role 保持 overlay |
| role（normal） | `set_role(normal=0)` | 服务端 role 切到 normal（回归普通窗口层） |
| 初始通知 | 绑定接口版本 2，创建对象后请求 normal | 客户端先收到初始 overlay，再收到 normal |
| 置顶请求 | `set_role(always_on_top)` | 客户端收到 always_on_top，真实 wrapper 置顶且 z=1 |
| 重复请求 | 再次请求相同 role | 不重复发送通知 |
| 用户取消置顶 | 对生产 wrapper 调用 `setAlwaysOnTop(false)` | role 与通知均为 normal，置顶清除且 z=0 |
| 用户手动置顶 | 对生产 wrapper 调用 `setAlwaysOnTop(true)` | role 与通知均为 always_on_top，z=1 |
| 客户端从 overlay 切换为置顶 | `set_role(always_on_top)` | 切换为 always_on_top，z=1 |
| 请求切换角色 | 从置顶切到 overlay 或 normal | 清除普通置顶标志，实际 z 分别为 2、0，通知与实际角色一致 |
| Overlay 子窗口 | 父窗口切换置顶和普通角色 | 子窗口保持 z=2，且无额外 role 事件 |
| 旧版本兼容 | 绑定接口版本 1，执行既有请求且不安装事件监听器 | 连接正常，不发送版本 2 的 role 事件 |
| 定位 hint | `set_position_hint(output, 42, 24)` | 服务端解析为全局坐标 `positionHint == (42,24)`，cursor hint 为空 |
| 定位 hint（空 output） | `set_position_hint(NULL, 7, 9)` | 以主屏原点为锚点解析，`positionHint == (7,9)` |
| 定位 hint（0,0） | `set_position_hint(NULL, 0, 0)` | `(0,0)` 是合法固定位置，position hint 存在且 cursor hint 为空 |
| 光标 hint | `set_cursor_placement_hint(11, 5)` | `cursorPlacementHint == (11,5)`，且 position hint 被清空（后发请求决定模式） |
| 模式切回 | 再次 `set_position_hint(NULL, 3, 4)` | `positionHint == (3,4)`，cursor hint 清空 |
| skip 位域 | `set_skip_flags(0x7)` 后 `set_skip_flags(0)` | 服务端 skipFlags 依次为 7、0 |
| 键盘焦点 | `set_accept_keyboard_focus(0)` | 服务端 acceptKeyboardFocus 为 false |
| 销毁 | `destroy` 后重新 `get_shell_surface` | 资源销毁后允许重建 |
| 重复创建 | 对同一 `wl_surface` 二次 `get_shell_surface` | 合成器抛出 `already_shell_surface`（服务端错误计数器断言），客户端连接被终止 |

## 已知边界

Treeland 的窗口菜单将 Overlay 窗口的「总在最上」显示为已勾选且禁用。
客户端仍可通过 `set_role(always_on_top)` 将窗口从 Overlay 切换到普通置顶层。

定位 hint 的全局坐标解析依赖 headless 输出位于原点 (0,0)，故解析结果与请求坐标一致；
多输出布局下的坐标换算由 `WOutput::position()` 保证。`set_role` 覆盖 normal、overlay 与 always_on_top 三个取值。

服务端不在 wl_surface 销毁时自动回收 shell surface（本质是设置 SurfaceWrapper
的协议对象，生命周期随 SurfaceWrapper 走）；`ShellHandler::onXdgToplevelSurfaceRemoved`
统一清理未创建 wrapper 场景下残留的 dde-shell 对象。角色、用户置顶状态及真实 z 层级已通过 mapped XDG 窗口验证；skip 布尔量和
`clientRequstPos`/光标放置的 wrapper 集成仍未覆盖。

## TODO

- TODO: 增加非原点第二 headless 输出，并断言 `set_position_hint(output, x, y)` 换算后的
  全局坐标（包括 (0,0) 固定位置），以覆盖多输出布局下 `WOutput::position()` / 主屏原点
  的坐标转换路径；当前单输出 (0,0) fixture 无法捕获相关回归。
- TODO: 增加 mapped XDG toplevel fixture，断言 `ShellHandler::handleDdeShellSurfaceV2Added()`
  与 `Output` 实际摆放后的真实全局几何（position/cursor 两种模式及其运行时切换）；当前用例
  仅在未映射的普通 `wl_surface` 上读取服务端 hint 状态，未覆盖 wrapper 集成与摆放逻辑。
