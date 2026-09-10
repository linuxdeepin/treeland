# `wayland-xdg-toplevel-tag-v1` 测试规范

## 范围

- XML / interface：`xdg_toplevel_tag_manager_v1`（version 1，仅有请求、无事件/无资源）
- 测试源码：`tests/protocols/wayland-xdg-toplevel-tag-v1/`
- Fixture：headless output fixture + xdg-toplevel-client（`create_pending` → `complete_map`）
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `xdg_toplevel_tag_manager_v1`（v1） | 资源创建成功 | P |
| 真实标签落库 | `set_toplevel_tag(toplevel, "treeland-tag-test")` + 映射 + roundtrip 后回读 | 回读的真实 `WXdgToplevelSurface::tag()` 等于客户端设置值 | E |

## 生产结果

测试创建并映射一个真实 xdg_toplevel，捕获其生产 `SurfaceWrapper`，通过 `set_toplevel_tag`
设置标签 `"treeland-tag-test"`，并在 roundtrip 后经服务端桥回读真实
`WXdgToplevelSurface::tag()`（Treeland 的 `WXdgToplevelTagManagerV1` 经 `setTag()` 写入该对象），
断言其等于客户端设置值，证明请求确实写入真实合成器对象，而非仅被接受无协议错误。

## 已知边界 / 下一项结果

未验证 tag 在 Treeland 任务栏/窗口管理中的实际路由与展示，以及 `set_toplevel_description` 的回读（V 级像素读回亦未覆盖）。
