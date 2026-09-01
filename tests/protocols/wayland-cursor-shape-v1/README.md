# `wayland-cursor-shape-v1` 测试规范

## 范围

- XML / interface：`wp_cursor_shape_manager_v1` / `wp_cursor_shape_device_v1`（version 2）
- 测试源码：`tests/protocols/wayland-cursor-shape-v1/`
- Fixture：headless output fixture + setup 显式声明 seat 指针能力
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 声明能力 | setup 中 `WInputDevice::Type::Pointer` + `attachInputDevice` | 客户端 `capabilities` 事件含指针位 | P |
| 绑定全局 | 绑定 `wp_cursor_shape_manager_v1`（v2） | 资源创建成功 | P |
| 创建设备 | `get_pointer(wl_pointer)` | 返回非 NULL 的 `wp_cursor_shape_device_v1` | P |
| 设置形状 | `set_shape(serial=0, DEFAULT)` | 无协议错误 | P |
| **生产回读** | — | setup 通过 `wl_signal_add` 捕获 `wlr_cursor_shape_manager_v1::events.request_set_shape`，断言捕获的 `shape` == DEFAULT(1) | **E** |

## 生产结果

测试观察 Treeland 通过 `wlr_cursor_shape_manager_v1_create` 提供的游标形状服务。
在真实 `wl_pointer` 上创建 cursor-shape 设备并 `set_shape(DEFAULT)`，setup 通过
`wl_signal_add` 捕获 `wlr_cursor_shape_manager_v1::events.request_set_shape` 事件，
回读真实生产对象中的 `shape` 值与客户端请求一致，证明游标形状请求到达真实 seat 管道。

## 已知边界 / 下一项结果

通过 `request_set_shape` 信号间接验证；未捕获实际光标像素（V 级）。未验证 `cursor_shape` table 完整枚举与错误 shape 值。
