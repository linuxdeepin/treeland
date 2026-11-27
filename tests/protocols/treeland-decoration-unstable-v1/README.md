# decoration-unstable-v1

覆盖等级：E。

测试创建并映射真实 xdg toplevel，创建 decoration context 后发送
`set_corner_radius(12)`，再在 compositor 线程回读同一 `SurfaceWrapper::radius()`；
随后发送 `set_corner_radius(0)` 并断言回读值为 0（0 即无圆角，不回退默认值）。
因此断言的是生产窗口状态，而非 request 仅能派发。

关闭 DConfig 的圆角、阴影和描边开关后，再发送非零半径、阴影和描边请求。
回读的有效半径（含 `frameGeometry` 与 `presentationFrameGeometry` 半径）为 0，阴影和描边都隐藏。
随后分别单独开启圆角、阴影、描边三个开关，确认互不串扰；最后全部重新开启后客户端请求恢复生效。

同一 toplevel 随后在最大化状态下切换 SSD 标题栏：SSD 标题栏出现时客户端收到的
configure 必须是最大化矩形减去标题栏高度，通过 `treeland-decoration-unstable-v1`
隐藏标题栏后必须回到整个最大化矩形，再恢复显示标题栏后又必须缩回一个标题栏。
SSD 标题栏在最大化之后出现、消失或恢复，都不能留下按旧 padding 计算的 configure，
否则窗口会正好高/矮一个标题栏。
Tiling / Fullscreen 状态、阴影和描边参数的具体渲染、协议错误路径仍未覆盖。
