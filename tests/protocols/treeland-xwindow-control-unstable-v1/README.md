# `treeland-xwindow-control-unstable-v1` 测试规范

## 范围

- XML / interface：`treeland_xwindow_control_v1`（v1）
- 测试源码：`tests/protocols/treeland-xwindow-control-unstable-v1/`
- Fixture：`protocol_test_setup` 注册生产 `XWindowControlInterfaceV1` 全局并创建
  headless output（仅协议/资源级，无需 mapped 窗口）
- 覆盖等级：**P**

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 初始化 | bind `treeland_xwindow_control_v1` | manager 非空 | P |
| 请求（失败） | `set_xwindow_position_relative`，anchor 为无对应 `SurfaceWrapper` 的裸 `wl_surface`、wid=0 | `wl_callback.done(1)` | P |
| 资源销毁 | `destroy` | 资源销毁，无协议错误 | P |

## 生产结果

`set_xwindow_position_relative` 在服务端解析 anchor `wl_surface` 为 `WSurface`，再交给
`Helper::setXWindowPositionRelative(wid, wsurface, dx, dy)`。失败路径测试证明：当 anchor
`wl_surface` 没有 `SurfaceWrapper`（裸 surface）且 wid 为 0 时，`Helper` 返回 false，
服务端把结果 `1` 通过新建的 `wl_callback` 回传并销毁该 callback。

`XWindowControlInterfaceV1Private` 在 `wl_resource_create` 分配 callback 失败时会调用
`wl_client_post_no_memory` 并中止该请求，避免对 null callback 解引用导致合成器崩溃；该
OOM 分支不在测试中强制触发（无法稳定注入 Wayland 内存分配失败）。

## 已知边界 / 下一项结果

- **成功路径（callback.done(0)）未覆盖**：`Helper::setXWindowPositionRelative` 要求 anchor
  拥有真实 `SurfaceWrapper`，且 wid 匹配一个已映射的 `Type::XWayland` `SurfaceWrapper`。
  生成一个真实的 XWayland 窗口需要可用的 Xwayland 实例；treeland 协议测试在
  `WLR_BACKENDS=headless`、`WLR_RENDERER=pixman` 下运行，Xwayland 启动时 glamor 需要
  GBM Wayland 接口，无 GPU 环境下无法启动，因此无法在当前 CI 中产出 XWayland
  `SurfaceWrapper`。成功路径（wid 匹配、目标位置 == anchor.topLeft + (dx,dy)）作为下一项
  待验证结果，前提是具备可运行的 Xwayland 测试环境（GPU 或软件 glamor）。
- 无效 anchor（客户端传入 null 或非 `wl_surface` 对象）已被服务端守卫为失败结果
  （`callback.done(1)`），但未单列为测试用例；可在具备成功路径环境后补充错误注入用例。
