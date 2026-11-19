# decoration-unstable-v1

覆盖等级：E。

测试创建并映射真实 xdg toplevel，创建 decoration context 后发送
`set_corner_radius(12)`，再在 compositor 线程回读同一 `SurfaceWrapper::radius()`。
因此断言的是生产窗口状态，而非 request 仅能派发。

同一 toplevel 随后在最大化状态下切换 SSD 标题栏：SSD 标题栏出现时客户端收到的
configure 必须是最大化矩形减去标题栏高度，通过 `treeland-decoration-unstable-v1`
隐藏标题栏后必须回到整个最大化矩形，再恢复显示标题栏后又必须缩回一个标题栏。
SSD 标题栏在最大化之后出现、消失或恢复，都不能留下按旧 padding 计算的 configure，
否则窗口会正好高/矮一个标题栏。
Tiling / Fullscreen 状态、shadow、border 和协议错误路径仍未覆盖。
