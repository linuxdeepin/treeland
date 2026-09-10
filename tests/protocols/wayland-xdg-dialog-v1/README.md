# `wayland-xdg-dialog-v1` 测试规范

## 范围

- XML / interface：`xdg_wm_dialog_v1` / `xdg_dialog_v1`（version 1，无事件）
- 测试源码：`tests/protocols/wayland-xdg-dialog-v1/`
- Fixture：headless output fixture + xdg-toplevel-client（`create_pending` → `complete_map`）
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `xdg_wm_dialog_v1`（v1） | 资源创建成功 | P |
| 关联对话框 | `get_xdg_dialog(toplevel)` | 返回非 NULL 的 dialog | P |
| 真实模态状态翻转 | 映射 toplevel 后回读 `SurfaceWrapper`，再 `set_modal` + roundtrip 后回读 | 回读的真实 `SurfaceWrapper::modal()` 由 `false` 翻转为 `true` | E |

## 生产结果

测试创建并映射一个真实 xdg_toplevel，捕获其生产 `SurfaceWrapper`，先回读确认 `modal()` 为
`false`；随后通过 `xdg_dialog_v1.set_modal` 请求并在 roundtrip 后再次回读，断言真实
`SurfaceWrapper::modal()` 翻转为 `true`。Treeland 经 `WXdgDialogManagerV1::surfaceModalChanged`
信号将该请求接入 `SurfaceWrapper::setModal()`，因此该断言验证请求确实到达真实合成器对象，
而非仅资源存活无协议错误。

## 已知边界 / 下一项结果

未验证模态对话框对其它窗口的实际焦点/输入抑制效果，以及 `set_unmodal` 的反向翻转（V 级像素读回亦未覆盖）。
