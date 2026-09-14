# `wayland-idle-inhibit-unstable-v1` 测试规范

## 范围

- XML / interface：`zwp_idle_inhibit_manager_v1` / `zwp_idle_inhibitor_v1`（version 1，无事件）
- 测试源码：`tests/protocols/wayland-idle-inhibit-unstable-v1/`
- Fixture：headless output fixture + 映射 xdg-toplevel
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zwp_idle_inhibit_manager_v1` | 资源创建成功 | P |
| 创建 inhibitor | `create_inhibitor(mapped_surface)` | 返回非 NULL 资源 | P |
| **生产回读** | — | 真实 `wlr_idle_inhibitor_v1` 存在且关联的 `WSurface` 已 mapped | **E** |
| **销毁** | 销毁 inhibitor | 测试 client 不再拥有 `zwp_idle_inhibitor_v1` production resource | **E** |

## 生产结果

测试观察 Treeland 通过 `wlr_idle_inhibit_manager_v1_create` 提供的空闲抑制服务。
创建 inhibitor 后，setup 枚举测试 client 的真实 Wayland resource，读取 wlroots
`wlr_idle_inhibitor_v1` 并断言其关联的 `WSurface` 已 mapped；销毁后再次枚举并要求
该 resource 已移除。这证明 idle-inhibit request 到达生产 manager，并且 inhibitor
实际挂在被抑制的 surface 上。

## 已知边界 / 下一项结果

接口本身无事件；未验证由 `Helper::updateIdleInhibitor()` 计算出的全局 idle-notifier
`inhibited` 状态，该跨协议行为由 ext-idle-notify 测试覆盖。
