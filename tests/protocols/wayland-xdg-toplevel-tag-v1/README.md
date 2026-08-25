# `wayland-xdg-toplevel-tag-v1` 测试规范

## 范围

- XML / interface：`xdg_toplevel_tag_manager_v1`（version 1，仅有请求、无事件/无资源）
- 测试源码：`tests/protocols/wayland-xdg-toplevel-tag-v1/`
- Fixture：headless output fixture + xdg-toplevel-client（`create_pending` → `complete_map`）
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `xdg_toplevel_tag_manager_v1`（v1） | 资源创建成功 | P |
| 设置标签 | `set_toplevel_tag(toplevel, "treeland-tag-test")` | 请求被接受、无协议错误 | P |
| 设置描述 | `set_toplevel_description(toplevel, ...)` + 映射 + roundtrip | 资源存活 | P |

## 生产结果

测试观察 Treeland 通过 `wlr_xdg_toplevel_tag_manager_v1_create` 提供的 toplevel 标签服务。
该接口仅含请求、无事件，故以真实 toplevel 上 `set_toplevel_tag` / `set_toplevel_description`
请求被接受且映射后无协议错误为协议级证据。

## 已知边界 / 下一项结果

未验证 tag/description 在 Treeland 任务栏/窗口管理中的实际路由与展示（E 级业务断言）。
