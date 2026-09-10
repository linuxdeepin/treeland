# `wayland-pointer-gestures-unstable-v1` 测试规范

## 范围

- XML / interface：`zwp_pointer_gestures_v1` / `zwp_pointer_gesture_swipe_v1` / `zwp_pointer_gesture_pinch_v1`（version 3）
- 测试源码：`tests/protocols/wayland-pointer-gestures-unstable-v1/`
- Fixture：headless output fixture + setup 显式声明 seat 指针能力 + client_created 监听器
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 声明能力 | setup 中 `wlr_seat_set_capabilities(POINTER)` | 客户端 `capabilities` 事件含指针位 | P |
| 绑定全局 | 绑定 `zwp_pointer_gestures_v1`（v3） | 资源创建成功 | P |
| 注册手势 | `get_swipe_gestures` + `get_pinch_gestures` | 均返回非 NULL 资源 | P |
| 生产回读 | 创建 swipe + pinch 资源后回读 | 真实 `wlr_pointer_gestures_v1::swipes` 和 `::pinches` 列表长度均 ≥ 1 | E |

## 生产结果

测试观察 Treeland 通过 `wlr_pointer_gestures_v1_create` 提供的手势服务。
在真实 `wl_pointer` 上成功创建 swipe + pinch 手势资源后，通过
`wl_client_for_each_resource` 遍历测试客户端绑定的资源，定位
`zwp_pointer_gestures_v1` 资源的 `user_data`（即真实的
`wlr_pointer_gestures_v1*` 句柄），回读其 `swipes` 和 `pinches` 列表
长度，断言两者均非空。

该句柄存储在 `WSeatPrivate::gesture`（无公共访问路径），测试通过
`wl_client_for_each_resource` 从客户端绑定资源中提取，而非直接访问私有成员。

## 已知边界 / 下一项结果

未验证注入手势事件后 `begin`/`update`/`end` 事件负载（需指针焦点 + WGestureEvent 注入），
也未验证 v3 `get_hold_gestures`。
