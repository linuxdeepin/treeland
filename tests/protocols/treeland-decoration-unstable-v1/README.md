# decoration-unstable-v1

覆盖等级：E。

测试创建并映射真实 xdg toplevel，创建 decoration context 后发送
`set_corner_radius(12)`，再在 compositor 线程回读同一 `SurfaceWrapper::radius()`。
因此断言的是生产窗口状态，而非 request 仅能派发。shadow、border、titlebar 和协议错误
路径仍未覆盖。
