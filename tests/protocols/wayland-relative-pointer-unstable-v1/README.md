# `wayland-relative-pointer-unstable-v1` 测试规范

## 范围

- XML / interface：`zwp_relative_pointer_manager_v1` / `zwp_relative_pointer_v1`（version 1）
- 测试源码：`tests/protocols/wayland-relative-pointer-unstable-v1/`
- Fixture：headless output fixture + setup 显式声明 seat 指针能力
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 声明能力 | setup 中 `wlr_seat_set_capabilities(POINTER)` | 客户端 `capabilities` 事件含指针位 | P |
| 绑定全局 | 绑定 `zwp_relative_pointer_manager_v1`（v1） | 资源创建成功 | P |
| 创建相对指针 | `get_relative_pointer(wl_pointer)` | 返回非 NULL 的 `zwp_relative_pointer_v1` | P |

## 生产结果

测试观察 Treeland 通过 `wlr_relative_pointer_manager_v1_create` 提供的相对指针服务。
在真实 `wl_pointer` 上成功创建 relative-pointer 资源，证明相对指针路径接入 seat。

## 已知边界 / 下一项结果

未验证注入指针运动后 `relative_motion` 事件的 dx/dy 负载（需 I 级注入 wlr 指针运动事件）。
