# `wayland-xdg-dialog-v1` 测试规范

## 范围

- XML / interface：`xdg_wm_dialog_v1` / `xdg_dialog_v1`（version 1，无事件）
- 测试源码：`tests/protocols/wayland-xdg-dialog-v1/`
- Fixture：headless output fixture + xdg-toplevel-client（`create_pending` → `complete_map`）
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `xdg_wm_dialog_v1`（v1） | 资源创建成功 | P |
| 关联对话框 | `get_xdg_dialog(toplevel)` | 返回非 NULL 的 dialog | P |
| 设为模态 | `set_modal` + 映射 + roundtrip | 资源存活、无协议错误 | P |

## 生产结果

测试观察 Treeland 通过 `wlr_xdg_dialog_v1_create` 提供的对话框语义服务。该接口无事件，
故以真实 toplevel 上 `get_xdg_dialog` + `set_modal` 请求被接受且映射后资源存活为协议级证据，
证明对话框管理器接入 xdg-shell。

## 已知边界 / 下一项结果

未验证模态对话框对其它窗口的实际焦点/输入抑制效果（E 级业务断言）。
