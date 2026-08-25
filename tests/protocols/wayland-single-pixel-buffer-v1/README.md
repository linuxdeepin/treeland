# `wayland-single-pixel-buffer-v1` 测试规范

## 范围

- XML / interface：`wp_single_pixel_buffer_manager_v1`（version 1）
- 测试源码：`tests/protocols/wayland-single-pixel-buffer-v1/`
- Fixture：headless output fixture
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `wp_single_pixel_buffer_manager_v1`（v1） | 资源创建成功 | P |
| 创建缓冲 | `create_u32_rgba_buffer(r,g,b,a)` | 返回非 NULL 的 `wl_buffer` | P |
| 附着提交 | `wl_surface_attach` + `commit`（两次） | 无协议错误，buffer 存活 | P |

## 生产结果

测试观察 Treeland 通过 `wlr_single_pixel_buffer_manager_v1_create`（master commit 00f0a3572）
提供的单像素缓冲服务。生成的 `wl_buffer` 可被真实 `wl_surface` 附着并提交，证明缓冲工厂路径完整接入。

## 已知边界 / 下一项结果

未验证单像素缓冲在已映射 surface 上的实际像素颜色（V 级像素读回）。
