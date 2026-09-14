# `wayland-primary-selection-unstable-v1` 测试规范

## 范围

- XML / interface：`zwp_primary_selection_device_manager_v1` / `zwp_primary_selection_device_v1`（version 1）
- 测试源码：`tests/protocols/wayland-primary-selection-unstable-v1/`
- Fixture：headless output fixture
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zwp_primary_selection_device_manager_v1` | 资源创建成功 | P |
| 创建设备 | `get_device(seat)` | 返回非 NULL `zwp_primary_selection_device_v1` | P |
| 创建 source | `create_source` + `offer("text/plain")` | 返回非 NULL `zwp_primary_selection_source_v1` | P |
| 设置选区 | `set_selection(device, source, 0)` | 无协议错误 | P |
| **生产回读** | — | 回读真实 `wlr_seat::primary_selection_source` 非空 | **E** |

## 生产结果

测试观察 Treeland 通过 `wlr_primary_selection_v1_device_manager_create` 提供的主选区服务。
客户端创建 source 并 `set_selection` 后，通过 `invoke_on_server_thread` 回读真实
`wlr_seat::primary_selection_source` 字段，断言其非空，证明主选区请求到达真实 seat 管道。

## 已知边界 / 下一项结果

仅验证 `primary_selection_source` 非空；未验证 source 的 mime type 内容与 `selection` 事件回传。
