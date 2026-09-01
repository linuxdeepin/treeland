# `wayland-alpha-modifier-v1` 测试规范

## 范围

- XML / interface：`wp_alpha_modifier_v1` / `wp_alpha_modifier_surface_v1`（version 1，无事件）
- 测试源码：`tests/protocols/wayland-alpha-modifier-v1/`
- Fixture：headless output fixture + xdg-toplevel-client（64×64 solid buffer 映射）
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `wp_alpha_modifier_v1`（v1） | 资源创建成功 | P |
| 关联表面 | `get_surface(surface)` | 返回非 NULL 的 `wp_alpha_modifier_surface_v1` | P |
| 设置透明度 0 | `set_multiplier(0)` + `commit` | 无协议错误 | P |
| 设置不透明度 | `set_multiplier(UINT32_MAX)` + `commit` | 无协议错误 | P |
| 真实乘数 0 | 回读真实 `wlr_alpha_modifier_surface_v1_state::multiplier` | == 0.0 | E |
| 真实乘数 1 | 回读真实 `wlr_alpha_modifier_surface_v1_state::multiplier` | == 1.0 | E |

## 生产结果

测试创建并映射一个真实 xdg_toplevel，为其附加 `wp_alpha_modifier_surface_v1` 对象。
先调用 `set_multiplier(0)` 并提交，通过 server bridge 回读真实
`wlr_alpha_modifier_v1_get_surface_state(wlr_surface)->multiplier`，断言为 0.0；
再调用 `set_multiplier(UINT32_MAX)` 并提交，回读断言为 1.0。`set_multiplier` 是双缓冲
状态，在 `wl_surface.commit` 后由 wlroots 将 uint32 因子转换为 double（÷UINT32_MAX）
并写入 `wlr_alpha_modifier_surface_v1_state`，因此此断言验证请求确实到达了真实合成器
surface 管线。

## 已知边界 / 下一项结果

未验证中间值（如 0x80808080）的浮点精度，也未验证 alpha 修饰对渲染输出的实际像素效果（V 级）。
