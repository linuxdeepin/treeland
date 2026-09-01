# `wayland-xdg-foreign-unstable-v2` 测试规范

## 范围

- XML / interface：`zxdg_exporter_v2` / `zxdg_exported_v2` / `zxdg_importer_v2` / `zxdg_imported_v2`（version 1）
- 测试源码：`tests/protocols/wayland-xdg-foreign-unstable-v2/`
- Fixture：headless output fixture + ShellHandler 捕获两个 toplevel SurfaceWrapper
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zxdg_exporter_v2` + `zxdg_importer_v2`（v1） | 资源创建成功 | P |
| 导出表面 | `export_toplevel(surface)` 后 roundtrip | 收到 `handle` 事件（非空 handle 字符串） | P |
| 导入表面 | `import_toplevel(handle)` | 返回非 NULL 的 `zxdg_imported_v2` | P |
| 设置父级 | `set_parent_of(imported, child_surface)` 后回读 | 真实 `WXdgToplevelSurface::parentXdgSurface()` 在子窗口上非空 | E |

## 生产结果

测试创建两个 mapped xdg_toplevel（父窗口 + 子窗口），通过 `export_toplevel`
导出父窗口获得非空 handle，以该 handle `import_toplevel` 成功导入，
然后调用 `set_parent_of(imported, child_surface)` 设置子窗口的父级。

`set_parent_of` 触发 wlroots 的 `wlr_xdg_toplevel_set_parent`，在真实的
`wlr_xdg_toplevel::parent` 上建立父子关系并发出 `events.set_parent` 信号。
`WXdgToplevelSurface` 监听该信号，`parentXdgSurface()` 直接读取
`handle()->parent`。测试回读子窗口的 `parentXdgSurface()`，断言非空，
证明 xdg-foreign-v2 的 `set_parent_of` 请求到达真实合成器并在真实 toplevel
对象上建立了父子关系。

`wlr_xdg_foreign_registry` 为 `Helper::init()` 中的局部变量，未存储为成员，
但协议流经 `wlr_xdg_foreign_v2` → `wlr_xdg_toplevel_set_parent` →
`WXdgToplevelSurface` 链路，父子关系可直接从 `WXdgToplevelSurface` 回读。

## 已知边界 / 下一项结果

未验证无效 handle 的 `destroyed` 事件语义，也未验证父窗口 unmap 后子窗口父级被清除的路径。
