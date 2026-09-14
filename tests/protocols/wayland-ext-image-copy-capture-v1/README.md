# `wayland-ext-image-copy-capture-v1` 测试规范

## 范围

- XML / interface：
  - 主：`ext_image_copy_capture_manager_v1` / `ext_image_copy_capture_session_v1`（version 1）
  - 附加：`ext_output_image_capture_source_manager_v1` / `ext_image_capture_source_v1`（version 1）
- 测试源码：`tests/protocols/wayland-ext-image-copy-capture-v1/`
- Fixture：headless output fixture（1920×1080）
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 capture-source 管理器 + copy-capture 管理器 | 资源创建成功 | P |
| 创建输出源 | `create_source(wl_output)` | 返回非 NULL 的 capture source | P |

## 生产结果

测试观察 Treeland 通过 wlroots 提供的输出图像捕获服务：客户端从真实 headless `wl_output`
创建并销毁 capture source，证明 output-to-source 工厂路径可用，而不仅是两个 global 被公告。

## 已知边界 / 下一项结果

不创建 capture session：headless 下进入 `wlr_output_configure_primary_swapchain()` 可能死锁
（issue #1407）。因此 `create_session()`、`buffer_size`、frame 创建和实际像素捕获（V 级）
均未覆盖；修复该问题后应补齐这些场景。
