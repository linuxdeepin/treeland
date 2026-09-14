# `wayland-single-pixel-buffer-v1` 测试规范

## 范围

- XML / interface：`wp_single_pixel_buffer_manager_v1`（version 1，无事件）
- 测试源码：`tests/protocols/wayland-single-pixel-buffer-v1/`
- Fixture：headless output fixture + xdg-toplevel-client（`create_pending` → ack → attach 单像素缓冲 → commit）
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `wp_single_pixel_buffer_manager_v1`（v1） | 资源创建成功 | P |
| 创建单像素缓冲 | `create_u32_rgba_buffer(0xffff, 0, 0, 0xffff)` | 返回非 NULL 的 `wl_buffer` | P |
| 映射 toplevel | 用单像素缓冲 attach + commit 映射 xdg_toplevel | 无协议错误 | P |
| 真实缓冲尺寸 | 回读真实 `wlr_surface::current.buffer_width/height` | 均为 1（1×1 单像素缓冲） | E |
| 真实映射状态 | 回读真实 `WSurface::mapped()` | 为 true | E |

## 生产结果

测试创建一个真实 xdg_toplevel，在 ack configure 后用 `wp_single_pixel_buffer_manager_v1`
创建的 1×1 RGBA 缓冲（而非 SHM 缓冲）映射该 toplevel。随后通过 server bridge 回读
捕获的生产 `SurfaceWrapper` 的 `WSurface::handle()` 所指向的 `wlr_surface`，断言其
`current.buffer_width` 和 `current.buffer_height` 均为 1，且 `WSurface::mapped()` 为
true。此断言验证单像素缓冲确实进入了真实合成器 surface 管线并被映射，而非仅资源
创建无协议错误。

## 已知边界 / 下一项结果

未验证缓冲的实际 RGBA 像素值（V 级像素读回亦未覆盖）。
