# `wayland-viewporter` 测试规范

## 范围

- XML / interface：`wp_viewporter` / `wp_viewport`（version 1，无事件）
- 测试源码：`tests/protocols/wayland-viewporter/`
- Fixture：headless output fixture + xdg-toplevel-client（64×64 solid buffer 映射）
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `wp_viewporter`（v1） | 资源创建成功 | P |
| 创建 viewport | `get_viewport(surface)` | 返回非 NULL 的 `wp_viewport` | P |
| 设置目标矩形 | `set_destination(320, 240)` + `commit` | 无协议错误 | P |
| 真实 viewport 状态 | 回读真实 `wlr_surface::current.viewport` | `has_dst` == true；`dst_width` == 320；`dst_height` == 240 | E |

## 生产结果

测试创建并映射一个 64×64 的真实 xdg_toplevel，为其创建 `wp_viewport` 对象，
调用 `set_destination(320, 240)` 将缓冲缩放至 320×240 并提交。随后通过 server bridge
回读捕获的生产 `SurfaceWrapper` 的 `wlr_surface::current.viewport`，断言 `has_dst` 为
true 且目标尺寸为 320×240。`wp_viewport.set_destination` 是双缓冲状态，在
`wl_surface.commit` 后由 wlroots 应用到 `wlr_surface_state::viewport`，因此此断言验证
viewport 请求确实到达了真实合成器 surface 管线。

## 已知边界 / 下一项结果

未验证 `set_source` 裁剪矩形，也未验证 viewport 对渲染输出的实际像素效果（V 级）。
