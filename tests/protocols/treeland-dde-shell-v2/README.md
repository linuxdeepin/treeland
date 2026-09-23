# `treeland-dde-shell-v2` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-dde-shell-v2/`
- Fixture：协议 fixture（headless output，普通 `wl_surface`）
- 覆盖等级：**P / I**——roundtrip 后读取服务端 `DDEShellSurfaceV2` 协议对象状态（P），
  并覆盖服务端资源生命周期（重复创建报错、wl_surface 销毁自动回收）
  （I 的生命周期子集）；wrapper 集成与业务级（E）fixture 随 v1 清理任务补。

v2 适配期间 v1 全局对象保留并行注册；本测试只覆盖
`dde/treeland-dde-shell-unstable-v2.xml` 的 manager/surface 两个接口。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| 创建 | `get_shell_surface(surface)` | 服务端创建 `DDEShellSurfaceV2`，默认 role 为 overlay（0 基） |
| role | `set_role(overlay=0)` | 服务端 role 保持 overlay |
| 定位 hint | `set_position_hint(output, 42, 24)` | 服务端解析为全局坐标 `positionHint == (42,24)`，cursor hint 为空 |
| 定位 hint（空 output） | `set_position_hint(NULL, 7, 9)` | 以主屏原点为锚点解析，`positionHint == (7,9)` |
| 定位 hint（0,0） | `set_position_hint(NULL, 0, 0)` | `(0,0)` 是合法固定位置，position hint 存在且 cursor hint 为空 |
| 光标 hint | `set_cursor_placement_hint(11, 5)` | `cursorPlacementHint == (11,5)`，且 position hint 被清空（后发请求决定模式） |
| 模式切回 | 再次 `set_position_hint(NULL, 3, 4)` | `positionHint == (3,4)`，cursor hint 清空 |
| skip 位域 | `set_skip_flags(0x7)` 后 `set_skip_flags(0)` | 服务端 skipFlags 依次为 7、0 |
| 键盘焦点 | `set_accept_keyboard_focus(0)` | 服务端 acceptKeyboardFocus 为 false |
| 销毁 | `destroy` 后重新 `get_shell_surface` | 资源销毁后允许重建 |
| wl_surface 销毁 | shell resource 存活时销毁 `wl_surface` | v2 资源被自动回收（服务端状态清零），兑现协议承诺 |
| 重复创建 | 对同一 `wl_surface` 二次 `get_shell_surface` | 合成器抛出 `already_shell_surface`（服务端错误计数器断言），客户端连接被终止 |

## 已知边界

定位 hint 的全局坐标解析依赖 headless 输出位于原点 (0,0)，故解析结果与请求坐标一致；
多输出布局下的坐标换算由 `WOutput::position()` 保证。`set_role` 只覆盖 overlay（v2
当前唯一取值）。wrapper 集成（`SurfaceWrapper` 的 Overlay 容器、skip 布尔量、
`clientRequstPos`/光标放置）与 v1 的 desktop fixture 同类覆盖尚未建立，待 dde-shell
客户端迁移后再补充。
