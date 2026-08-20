# `wlr-layer-shell-unstable-v1` 测试规范

## 范围

- XML / interface：`3rdparty/wlroots/protocol/wlr-layer-shell-unstable-v1.xml`，
  `zwlr_layer_shell_v1` / `zwlr_layer_surface_v1` v5。
- 测试源码：`tests/protocols/wlr-layer-shell-unstable-v1/`。
- Fixture：完整 Treeland desktop integration fixture、真实 headless `wl_output`、默认 seat。
- 覆盖等级：P / E。

wlroots 提供 native layer-shell；waylib 的 `WLayerShell` / `WLayerSurface` 负责 wrapper 与
native handle 生命周期，`ShellHandler::init()` 通过 `attach<WLayerShell>()` 接入。测试前将
waylib 发布版本由 v4 修正为 v5，故 XML 的 `set_exclusive_edge` 已可由真实 client 请求。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 初始 layer 映射 | 创建 roleless `wl_surface`；`get_layer_surface(top)` 后设置 size、top/left/right anchor、exclusive zone、margin、exclusive keyboard 与 exclusive edge；先空 commit | client 收到唯一初始 `configure`，且尚未 attach buffer | P |
| configure 后映射 | 对最新 configure serial 调用 `ack_configure`，再 attach 1920×40 shm buffer 并 commit | 同一 surface 映射为真实 `WLayerSurface` / `SurfaceWrapper`，进入 top-layer 的 output container；其 anchor、exclusive-zone、top margin、keyboard interactivity 与请求一致 | E |
| exclusive keyboard | 映射 top-layer exclusive surface | 默认 seat container 的真实 `keyboardFocusSurface()` 是该 layer wrapper | E |
| layer 变更 | `set_layer(overlay)` 后 commit | 同一 production wrapper 从 top container 迁移到 overlay container，`WLayerSurface::layer()` 为 overlay | E |
| 生命周期 | `zwlr_layer_surface_v1.destroy`、`zwlr_layer_shell_v1.destroy` | layer resource 与 manager 均可正常销毁，无 protocol error | P |

## 生产结果

测试 client 遵守 XML 的初始映射顺序：先获取 layer role、完成所有双缓冲 state request、空
commit，等待生产 compositor 的 `configure`，ack 后才 attach buffer。fixture 不手工创建
`WLayerSurface` 或发送 configure。

该 buffer commit 进入 wlroots native layer-shell；waylib `WLayerShell` 创建
`WLayerSurface`，`ShellHandler::onLayerSurfaceAdded()` 为同一 surface 创建 `SurfaceWrapper`，
并由 `updateLayerSurfaceContainer()` 放入 Treeland top layer 对应的 output container。测试通过
compositor-thread bridge 读取此真实 wrapper、其 `WLayerSurface` 属性和 container 归属。

top layer 的 exclusive keyboard interactivity 经生产 focus 规则使该 wrapper 成为默认 seat 的
keyboard focus。之后同一 client 的 `set_layer(overlay)` commit 触发 waylib 的 `layerChanged`；
`ShellHandler` 将原 wrapper 移至 overlay container。上述映射、container placement、focus 与
迁移构成 E 级业务链路；configure/ack、exclusive-edge 与 destroy 仅证明协议/生命周期，属于 P。

## 已知边界 / 下一项结果

- 未创建 xdg-popup，故未覆盖 `get_popup` 的 parent 关联。
- 未触发 output 移除，故未覆盖 compositor `closed` event。
- 未在独立失败连接覆盖 role、invalid layer、already constructed、invalid size/anchor/keyboard
  interactivity/exclusive edge 等 protocol error。
- 未验证多个 layer surface 的 z-order、exclusive-zone 对 maximized toplevel usable area 的最终
  几何影响，或最终 output 像素。
