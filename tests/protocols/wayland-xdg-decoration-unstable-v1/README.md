# `wayland-xdg-decoration-unstable-v1` 测试规范

## 范围

- XML / interface：`zxdg_decoration_manager_v1` / `zxdg_toplevel_decoration_v1`（version 2）
- 测试源码：`tests/protocols/wayland-xdg-decoration-unstable-v1/`
- Fixture：headless output fixture + xdg-toplevel-client（`create_pending` → `complete_map`）
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zxdg_decoration_manager_v1`（v2） | 资源创建成功 | P |
| 关联装饰 | `get_toplevel_decoration(toplevel)` + `set_mode(CLIENT_SIDE)` | 返回非 NULL 的 decoration | P |
| 装饰协商 | 映射 toplevel 后 roundtrip | 收到 decoration `configure` 事件（含 mode） | P |

## 生产结果

测试观察 Treeland 通过 `wlr_xdg_decoration_manager_v1_create` 提供的 CSD/SSD 协商服务。
真实 xdg toplevel 映射后服务端下发 decoration `configure` 事件，证明装饰管理器接入 xdg-shell 生命周期。

## 已知边界 / 下一项结果

仅验证 configure 事件到达；未断言最终 mode 取值（Treeland 可强制 SERVER_SIDE），也未验证
SSD 模式下的实际标题栏渲染（V 级）。
