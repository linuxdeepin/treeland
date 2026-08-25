# `wayland-alpha-modifier-v1` 测试规范

## 范围

- XML / interface：`wp_alpha_modifier_v1` / `wp_alpha_modifier_surface_v1`（version 1）
- 测试源码：`tests/protocols/wayland-alpha-modifier-v1/`
- Fixture：headless output fixture
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `wp_alpha_modifier_v1`（v1） | 资源创建成功 | P |
| 关联表面 | `get_surface(surface)` | 返回非 NULL 的 modifier surface | P |
| 设置透明度 | `set_multiplier` + `commit`，再设一次 | 资源存活、无协议错误 | P |

## 生产结果

测试观察 Treeland 通过 `wlr_alpha_modifier_v1_create` 提供的表面透明度调节服务对真实
`wl_surface` 生效。modifier surface 在多次设置 + 提交后仍存活，证明 wlroots alpha-modifier 接入正常。

## 已知边界 / 下一项结果

未验证半透明效果在已映射、有 buffer 的 surface 上的实际像素合成结果（V 级像素读回）。
