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
| 创建会话 | `create_session(source, 0)` + roundtrip | 收到 `buffer_size` 事件（宽高 > 0） | P |

## 生产结果

测试观察 Treeland 通过 wlroots 提供的输出图像捕获服务。从真实 headless 输出创建捕获源并启动
copy-capture 会话后，服务端下发 `buffer_size`（输出几何），证明捕获源/会话路径完整接入输出。

## 已知边界 / 下一项结果

仅验证 buffer_size 事件到达且维度非零；未创建 frame、未断言实际像素捕获内容（V 级）。
