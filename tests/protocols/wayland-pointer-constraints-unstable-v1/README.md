# `wayland-pointer-constraints-unstable-v1` 测试规范

## 范围

- XML / interface：`zwp_pointer_constraints_v1` / `zwp_locked_pointer_v1` / `zwp_confined_pointer_v1`（version 1）
- 测试源码：`tests/protocols/wayland-pointer-constraints-unstable-v1/`
- Fixture：headless output fixture + 映射 xdg-toplevel + setup 显式声明 seat 指针能力
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 声明能力 | setup 中 `WInputDevice::Type::Pointer` + `attachInputDevice` | 客户端 `capabilities` 事件含指针位 | P |
| 绑定全局 | 绑定 `zwp_pointer_constraints_v1` | 资源创建成功 | P |
| 创建约束 | `lock_pointer(surface, pointer)` | 返回非 NULL `zwp_locked_pointer_v1` | P |
| **生产回读** | — | 回读真实 `wlr_pointer_constraint_v1::type` == Locked(0) | **E** |

## 生产结果

测试观察 Treeland 通过 `WPointerConstraintsV1` 提供的指针约束服务。
在真实 `wl_pointer` 上创建 locked 指针约束后，setup 通过
`WPointerConstraintsV1::newConstraint` Qt 信号捕获 `wlr_pointer_constraint_v1*`，
回读其 `type` 字段断言为 Locked，证明约束创建到达生产指针约束管道。

## 已知边界 / 下一项结果

仅验证 Locked 类型；未验证 Confined 类型与 `region` / `cursor_hint` 设置。
