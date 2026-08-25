# `wayland-xdg-foreign-unstable-v2` 测试规范

## 范围

- XML / interface：`zxdg_exporter_v2` / `zxdg_exported_v2` / `zxdg_importer_v2` / `zxdg_imported_v2`（version 1）
- 测试源码：`tests/protocols/wayland-xdg-foreign-unstable-v2/`
- Fixture：headless output fixture
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zxdg_exporter_v2` + `zxdg_importer_v2`（v1） | 资源创建成功 | P |
| 导出表面 | `export_toplevel(surface)` 后 roundtrip | 收到 `handle` 事件（非空 handle 字符串） | P |
| 导入表面 | `import_toplevel(handle)` | 返回非 NULL 的 `zxdg_imported_v2` | P |

## 生产结果

测试观察 Treeland 通过 `wlr_xdg_foreign_v2_create` 提供的跨进程 surface 句柄导出/导入服务。
导出真实 surface 后获得非空 handle，再以该 handle 成功导入，证明 xdg-foreign 导出/导入往返路径完整。

## 已知边界 / 下一项结果

未验证 `import_toplevel` 后 `attach` 到本进程 surface 的实际父子关系建立（需 E 级 mapped 窗口），
也未验证无效 handle 的 `destroyed` 事件语义。
