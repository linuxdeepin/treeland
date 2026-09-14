# `wayland-idle-inhibit-unstable-v1` 测试规范

## 范围

- XML / interface：`zwp_idle_inhibit_manager_v1` / `zwp_idle_inhibitor_v1`（version 1，无事件）
- 测试源码：`tests/protocols/wayland-idle-inhibit-unstable-v1/`
- Fixture：headless output fixture + 映射 xdg-toplevel + 跨协议 ext-idle-notify
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zwp_idle_inhibit_manager_v1` | 资源创建成功 | P |
| 创建 inhibitor | `create_inhibitor(mapped_surface)` | 返回非 NULL 资源 | P |
| **跨协议抑制** | — | ext-idle-notify `get_idle_notification(1ms)` 后 500ms 内 `idled` **不**触发 | **E** |
| **解除抑制** | 销毁 inhibitor | ext-idle-notify `get_idle_notification(1ms)` 后 `idled` 触发 | **E** |

## 生产结果

测试观察 Treeland 通过 `wlr_idle_inhibit_manager_v1_create` 提供的空闲抑制服务。
通过跨协议行为交叉检查：创建 inhibitor 后 ext-idle-notify 的 `idled` 不触发，
销毁后触发。这证明生产 `wlr_idle_notifier_v1` 的 `inhibited` 状态被修改，
而非仅 inhibitor 资源存活。

## 已知边界 / 下一项结果

跨协议测试使用 `ProtocolTest.cmake` 的 `EXTRA_XMLS` 同时包含 ext-idle-notify XML。
接口本身无事件，E 级验证完全依赖跨协议行为。
