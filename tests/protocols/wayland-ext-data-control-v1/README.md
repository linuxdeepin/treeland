# `wayland-ext-data-control-v1` 测试规范

## 范围

- XML / interface：`ext_data_control_manager_v1` / `ext_data_control_device_v1`（version 1）
- 测试源码：`tests/protocols/wayland-ext-data-control-v1/`
- Fixture：headless output fixture
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `ext_data_control_manager_v1` | 资源创建成功 | P |
| 创建设备 | `get_data_device(seat)` | 收到初始 `selection(NULL)` 事件 | P |
| 设置选区 | `create_offer` + `offer("text/plain")` + `set_selection` | 收到非 NULL `selection` 事件 | P |
| **生产回读** | — | 回读真实 `wlr_seat::selection_source` 非空 | **E** |

## 生产结果

测试观察 Treeland 通过 `wlr_ext_data_control_manager_v1_create` 提供的数据控制服务。
客户端创建 offer 并 `set_selection` 后，通过 `invoke_on_server_thread` 回读真实
`wlr_seat::selection_source` 字段，断言其非空，证明数据控制选区请求到达真实 seat 管道。

## 已知边界 / 下一项结果

仅验证 `selection_source` 非空；未验证 `primary_selection` 设置路径与 offer 的 mime type 内容。
