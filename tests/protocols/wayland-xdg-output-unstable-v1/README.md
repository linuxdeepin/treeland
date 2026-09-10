# `wayland-xdg-output-unstable-v1` 测试规范

## 范围

- XML / interface：`zxdg_output_manager_v1` / `zxdg_output_v1`（version 3）
- 测试源码：`tests/protocols/wayland-xdg-output-unstable-v1/`
- Fixture：headless output fixture（`add_headless_output`，1920x1080 @ 0,0）
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zxdg_output_manager_v1` | 资源创建成功 | P |
| 输出逻辑几何 | `get_xdg_output(wl_output)` 后 roundtrip | 收到 `logical_position`、`logical_size`、`done` 事件 | P |
| 真实输出几何一致 | 经 `invoke_on_server_thread` 回读真实 `WOutput` | 客户端 `logical_position`/`logical_size` 与真实 `WOutput::position()`/`effectiveSize()` 完全一致 | E |

## 生产结果

测试观察 Treeland 通过 `WXdgOutputManager` 包装器为真实 `wl_output` 资源发布的逻辑几何，
并跨进程回读合成器内真实 `WOutput` 的布局几何（`position()`/`effectiveSize()`，
二者分别读取 `wlr_output_layout_output->x,y` 与 `wlr_output_effective_resolution`，
正是 `WXdgOutputManager` 用以填充 `logical_position`/`logical_size` 的同一 wlroots 状态）。
客户端收到的协议事件与该真实生产输出对象的几何逐字段相等，证明 Treeland 的 xdg-output
实现正确接入了其输出布局——而不仅是事件载荷非零。

## 已知边界 / 下一项结果

仅验证单个 headless 输出的静态几何与真实 `WOutput` 的一致性。未验证多输出布局变化后
`done` 的批量发送语义、输出缩放/变换下的逻辑几何，以及输出热插拔时的几何更新（V 级像素读回亦未覆盖）。
