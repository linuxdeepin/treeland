# `treeland-wallpaper-shell-unstable-v1` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-wallpaper-shell-unstable-v1/`
- Fixture：协议 fixture。
- 覆盖等级：**I/P**。

## 实际请求与预期结果

| 场景 | 客户端发送 | 生产业务逻辑与断言 |
| --- | --- | --- |
| shell surface | 用 `wl_surface` 创建 wallpaper surface | production shell 跟踪资源创建与销毁 |
| 生命周期事件 | 驱动 failed/play/pause/slow-down 路径 | 客户端收到相应资源事件 |
| notifier | 订阅并驱动 add/remove | 收到 notifier 的 payload 与生命周期事件 |

## 已证明的生产链路

覆盖 wallpaper shell 与 notifier 的生产资源实现。

`wallpaper_surface.ready()` 的完成条件属于已有的生产生命周期：surface 已 mapped 或已有
committed buffer 时，`ready()` 立即置位并发出 `ready`；否则它等待后续
`WSurface::commit`。该协议对象不会为无 shell role 的 client surface 主动调用 `map()`。

## 未覆盖

基础测试覆盖无 buffer 时由下一次 commit 完成 `ready` 的时序，但没有 output 上的 mapped
wallpaper surface 或渲染读回。无 role client 的空/带 buffer commit 不应被当作 map 的替代。
事件不代表壁纸已经可见或播放。与 manager 的资源关联由
[`wallpaper-desktop-v1`](../treeland-wallpaper-desktop-v1/README.md) 验证；实际 wallpaper owner 的映射、
QML 接入、最终 output 像素与媒体播放仍未覆盖。
