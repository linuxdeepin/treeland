# `wayland-ext-foreign-toplevel-list-v1` 测试规范

## 范围

- XML / interface：`ext_foreign_toplevel_list_v1` / `ext_foreign_toplevel_handle_v1`（version 1）
- 测试源码：`tests/protocols/wayland-ext-foreign-toplevel-list-v1/`
- Fixture：headless output fixture + 映射 xdg-toplevel
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `ext_foreign_toplevel_list_v1` | 资源创建成功 | P |
| 映射 toplevel | xdg-toplevel map | 收到 `toplevel` 事件 + 非 NULL handle | P |
| 停止 | `stop` | 收到 `finished` 事件 | P |
| **生产回读** | — | 回读真实 `wlr_ext_foreign_toplevel_list_v1::toplevels` 列表 `wl_list_length` >= 1 | **E** |

## 生产结果

测试观察 Treeland 通过 `wlr_ext_foreign_toplevel_list_v1_create` 提供的外部 toplevel 列表服务。
映射 xdg-toplevel 后，通过 `invoke_on_server_thread` 回读真实
`wlr_ext_foreign_toplevel_list_v1::toplevels` 列表长度，断言 >= 1，证明生产列表管理器
收到了真实 toplevel 注册。

## 已知边界 / 下一项结果

仅验证列表非空；未验证 handle 的 `title/app_id/closed` 事件 payload。
