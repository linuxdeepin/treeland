# `wayland-ext-data-control-v1` 测试规范

## 范围

- XML / interface：`ext_data_control_manager_v1` / `ext_data_control_device_v1`（version 1）
- 测试源码：`tests/protocols/wayland-ext-data-control-v1/`
- Fixture：headless output fixture
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `ext_data_control_manager_v1`（v1） | 资源创建成功 | P |
| 创建设备 | `get_data_device(seat)` | 返回非 NULL 的 device | P |
| 初始剪贴板 | 创建设备后 roundtrip | 收到 `selection` 事件（offer=NULL） | P |

## 生产结果

测试观察 Treeland 通过 `wlr_data_control_manager_v1_create` 提供的剪贴板控制服务。
设备创建后 wlroots 主动推送当前剪贴板状态（`selection` 事件），证明 data-control 设备已与 seat 绑定。

## 已知边界 / 下一项结果

未验证 `set_selection` 提供数据源后跨客户端的 MIME 数据传输与 `primary_selection` 同步
（需 I 级双客户端 + 数据源 fixture）。
