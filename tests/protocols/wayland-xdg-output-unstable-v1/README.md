# `wayland-xdg-output-unstable-v1` 测试规范

## 范围

- XML / interface：`zxdg_output_manager_v1` / `zxdg_output_v1`（version 3）
- 测试源码：`tests/protocols/wayland-xdg-output-unstable-v1/`
- Fixture：headless output fixture（`add_headless_output`，1920x1080 @ 0,0）
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zxdg_output_manager_v1`（v3） | 资源创建成功 | P |
| 输出逻辑几何 | `get_xdg_output(wl_output)` 后 roundtrip | `logical_position`=(0,0)、`logical_size`=(1920,1080)、`done` 事件 | P |

## 生产结果

测试观察 Treeland 通过 `WXdgOutputManager` 包装器为真实 `wl_output` 资源发布的逻辑几何。
`logical_size` 与 headless 输出几何一致，证明 Treeland 的 xdg-output 实现接入了其输出布局。

## 已知边界 / 下一项结果

仅验证单个 headless 输出的静态几何。未验证多输出布局变化后 `done` 的批量发送语义、
输出缩放/变换下的逻辑几何，以及输出热插拔时的几何更新。
