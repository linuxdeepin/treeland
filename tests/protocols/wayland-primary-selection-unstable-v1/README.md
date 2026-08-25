# `wayland-primary-selection-unstable-v1` 测试规范

## 范围

- XML / interface：`zwp_primary_selection_device_manager_v1` / `zwp_primary_selection_device_v1`（version 1）
- 测试源码：`tests/protocols/wayland-primary-selection-unstable-v1/`
- Fixture：headless output fixture
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zwp_primary_selection_device_manager_v1`（v1） | 资源创建成功 | P |
| 创建设备 | `get_device(seat)` | 返回非 NULL 的 device | P |
| 初始选择 | 创建设备后 roundtrip | 收到 `selection` 事件（offer=NULL，无选中内容） | P |

## 生产结果

测试观察 Treeland 通过 `wlr_primary_selection_device_manager_v1_create` 提供的主选区服务。
设备创建后 wlroots 主动推送当前选区状态（`selection` 事件），证明主选区设备已与 seat 绑定并工作。

## 已知边界 / 下一项结果

未验证 `set_selection` 后跨客户端的选区同步与 MIME 数据传输（需 I 级双客户端 + 数据源 fixture）。
