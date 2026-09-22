# layer-shell-extension-unstable-v1

覆盖等级：P。

测试在真实 layer-shell surface 上创建 extension object，并以无效 pointer serial 发起
`begin_resize`，断言收到 `resize_rejected(bad_serial)`。成功 resize、尺寸 clamp 和
`resizing` 生命周期需要可控的真实 pointer button serial，当前未覆盖。
