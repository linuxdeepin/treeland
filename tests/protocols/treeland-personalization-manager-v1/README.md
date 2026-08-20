# `treeland-personalization-manager-v1` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-personalization-manager-v1/`、`tests/protocols/treeland-personalization-desktop-v1/`
- Fixture：协议 fixture；带 headless output 和 mapped xdg-toplevel 的 desktop fixture。
- 覆盖等级：窗口个性化为 **E**；cursor、font、appearance context 为 **P/I**。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| 窗口样式 | 设置 BLUR、radius `12`、shadow `(8,2,3,10,20,30,40)`、border `(2,100,150,200,255)`、禁用 titlebar | 真实 wrapper 和其附着 `Personalization` 保留全部请求值 |
| context API | 创建 window/cursor/font/appearance context | 有效 context 接受契约请求，无效 cursor context 被拒绝 |
| font 回读 | 依次设置 `TestFont`、`TestMonoFont`、size `37` 后调用三个 `get_*` | production `TreelandUserConfig` 经 `requestFont` / `requestMonoFont` / `requestFontSize` 返回同一值事件 |
| appearance 回读 | 设置 radius、icon theme、active color、opacity、theme type、titlebar height 后调用六个 `get_*` | 生产 appearance context 从配置读取并再次发送完全相同的事件值 |
| 配置恢复 | 用例结束 | 恢复测试开始前读取的 cursor、font、appearance 配置，并在恢复后 roundtrip，避免测试残留用户机器参数 |

## 已证明的生产链路

客户端创建并 map xdg-toplevel，使用其真实 `wl_surface` 调用
`get_window_context`。随后发送 `set_blend_mode(BLUR)`、
`set_round_corner_radius(12)`、`set_shadow(8,2,3,10,20,30,40)`、
`set_border(2,100,150,200,255)` 和 `set_titlebar(DISABLE)`。

round-trip 后，fixture 不是读取 context 资源，而是读取 `ShellHandler` 创建并加入
`Workspace` 的 `SurfaceWrapper`：`radius()==12`、`blur()==true`、
`noTitleBar()==true`。它再通过 wrapper 的子对象找到生产 `Personalization`，断言
background type 为 BLUR、`noTitlebar()==true`，以及 shadow/border 的每一个半径、偏移
和 RGBA/宽度值均与请求完全一致。

非窗口 context 使用同一个生产 `TreelandUserConfig`。测试在任何 setter 之前经 server
callback 保存 cursor theme/size、font/mono-font/size、window radius、icon theme、active
color、opacity、theme type 与 titlebar height；每个 setter 后，再由对应 `get_*` request
触发 production `request...` 连接读取该 config 并向客户端回发事件。getter 断言的是第二次
收到的测试值，不把初始 context 同步事件误作回读结果。cleanup 通过同一组 production setter
恢复快照，并完成一轮 Wayland roundtrip 后才断开连接。

## 已知边界 / 下一项结果

未检查 `SurfaceWrapper` 的 QML item 或像素，不能证明圆角、模糊、阴影、边框已可见。

测试进程若被强制杀死（例如断电或外部 `SIGKILL`），进程内 cleanup 无法执行；此类异常情形
仍可能留下已写入的 DConfig 值，应手动恢复用户配置。正常通过和普通断言失败路径都会执行恢复。

XML 的 manager `destroy` 是 version 2 请求，但当前生产
`PersonalizationManagerInterfaceV1::InterfaceVersion` 仍为 1，client 无法合法发送它。因此
本轮补齐 7 个 getter 后，当前已发布 version 的 request 覆盖为 36 条；若要计入 XML 的第 37
条，须先把生产 global 升级到 version 2 并实现/验证 manager resource 的 destroy 生命周期。
