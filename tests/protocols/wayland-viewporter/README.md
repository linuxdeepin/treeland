# `wayland-viewporter` 测试规范

## 范围

- XML / interface：`wp_viewporter` / `wp_viewport`（version 1，stable）
- 测试源码：`tests/protocols/wayland-viewporter/`
- Fixture：headless output fixture
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `wp_viewporter`（v1） | 资源创建成功 | P |
| 创建视口 | `get_viewport(surface)` | 返回非 NULL 的 `wp_viewport` | P |
| 应用变换 | `set_destination` + `commit`，再 `set_source` | 资源存活、无协议错误 | P |

## 生产结果

测试观察 Treeland 通过 `wlr_viewporter_create` 提供的视口缩放/裁剪服务对真实 `wl_surface` 生效。
视口资源在多次请求 + 提交后仍存活，证明 wlroots viewporter 接入正常。

## 已知边界 / 下一项结果

未验证 viewport 变换在已映射、有 buffer 的 surface 上的实际像素裁剪/缩放结果（V 级像素读回），
也未验证 `set_source` 越界等错误语义。
