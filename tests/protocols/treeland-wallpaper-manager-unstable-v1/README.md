# `treeland-wallpaper-manager-unstable-v1` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-wallpaper-manager-unstable-v1/`
- Fixture：启动 headless backend、创建真实 `wl_output` 的协议 fixture。
- 覆盖等级：**I/P**。

## 实际请求与预期结果

| 场景 | 客户端发送 | 生产业务逻辑与断言 |
| --- | --- | --- |
| 创建资源 | 为真实 output 与 `wl_surface` 创建 wallpaper 对象 | production manager 记录两个 wallpaper 资源 |
| 状态事件 | 进入 failed/changed 服务端路径 | 客户端收到相应协议事件 |

## 已证明的生产链路

覆盖真实 output 绑定和 production manager 的 wallpaper 资源生命周期。

## 未覆盖

基础测试没有发送 image/video source，也没有 wallpaper item 或 output 像素断言；它本身
不能证明壁纸真的被应用。manager 的 `set_image_source` 与 shell 的实际应用路径已由
[`wallpaper-desktop-v1`](../treeland-wallpaper-desktop-v1/README.md) 通过；仍未覆盖文件解码与最终 output
像素。
