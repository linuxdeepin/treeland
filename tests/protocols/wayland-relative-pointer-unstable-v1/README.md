# `wayland-relative-pointer-unstable-v1` 测试规范

## 范围

- XML / interface：`zwp_relative_pointer_manager_v1` / `zwp_relative_pointer_v1`（version 1）
- 测试源码：`tests/protocols/wayland-relative-pointer-unstable-v1/`
- Fixture：headless output fixture + 映射 xdg-toplevel + setup 显式声明 seat 指针能力
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 声明能力 | setup 中 `WInputDevice::Type::Pointer` + `attachInputDevice` | 客户端 `capabilities` 事件含指针位 | P |
| 绑定全局 | 绑定 `zwp_relative_pointer_manager_v1` | 资源创建成功 | P |
| 创建 relative pointer | `get_relative_pointer(wl_pointer)` | 返回非 NULL `zwp_relative_pointer_v1` | P |
| **生产回读** | — | 回读真实 `wlr_relative_pointer_manager_v1::relative_pointers` 列表 `wl_list_length` >= 1 | **E** |

## 生产结果

测试观察 Treeland 通过 `WRelativePointerManagerV1` 提供的相对指针服务。
在真实 `wl_pointer` 上创建 relative pointer 后，通过 `invoke_on_server_thread` 回读真实
`wlr_relative_pointer_manager_v1::relative_pointers` 列表长度，断言 >= 1，
证明 relative pointer 资源被注册到生产管理器列表中。

## 已知边界 / 下一项结果

仅验证列表非空；未验证 `relative_motion` 事件 payload（需要真实指针运动）。
