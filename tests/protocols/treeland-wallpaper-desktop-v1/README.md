# Wallpaper manager + shell 桌面联合路径测试规范

## 状态

- 测试源码：`tests/protocols/treeland-wallpaper-desktop-v1/`
- Fixture：完整生产 `Helper` 与默认 headless output。
- 覆盖等级：**P / E**。

这不是新增 XML；它把 `treeland-wallpaper-manager-unstable-v1` 和
`treeland-wallpaper-shell-unstable-v1` 串成一条实际业务链。两份各自的基础测试仍保留其
资源、事件和错误覆盖。

## 输入与顺序

测试先等待真实 `TreelandUserConfig` 初始化完成。该生产信号先驱动
`WallpaperManager::updateWallpaperConfig()`，为现有 output 建立 workspace wallpaper
配置；随后 desktop runner 才启动 client。这样 `set_image_source(DESKTOP)` 进入
`WallpaperManager::setOutputWallpaper()` 时必有可更新的 output 配置，而不是依赖任意延时。

客户端只创建一个无 role 的 `wl_surface`，并按以下严格顺序使用它：

1. 绑定实际存在的 `wl_output`、`treeland_wallpaper_manager_v1` 和 version 2 的
   `treeland_wallpaper_shell_v1`。
2. 调用 `manager.get_treeland_wallpaper(output, surface)`，然后发送
   `wallpaper.set_image_source("/tmp/treeland-protocol-wallpaper-red", DESKTOP)`。
   当前实现把该 source 写进这个 output 当前 workspace 的生产
   `WallpaperManager` 配置。
3. 用**同一个** `wl_surface` 调用
   `shell.get_treeland_wallpaper_surface(surface, source)`，从而创建真正的
   `TreelandWallpaperSurfaceInterfaceV1`，而不是建立独立的测试 surface。
4. 创建 `64×64`、`wl_shm/ARGB8888` buffer，并将其 attach/damage/commit 到该 wallpaper
   surface，最后发送 `wallpaper_surface.ready()`。
5. 通过 `wl_display_roundtrip()` 确认服务端已按请求顺序处理；测试不以时间延迟判断状态。

这里的路径标识符不是待解码 JPEG 文件。它用于验证 manager 配置与 wallpaper shell 的资源
关联。`TreelandWallpaperSurfaceInterfaceV1` 创建的 surface 没有 xdg/layer shell role，
因此本测试不会把 buffer commit 当成 mapped、ready 或 QML 可见的证据；映射必须由实际的
wallpaper owner 负责，不能由该协议资源补做。

## 必须观察到的生产结果

服务端同步读取生产对象时，以下结果必须同时成立：

| 检查 | 生产对象/结果 |
| --- | --- |
| shell 注册 | `TreelandWallpaperSurfaceInterfaceV1::get(source)` 返回刚创建的对象 |
| manager 关联 | `TreelandWallpaperInterfaceV1::getReferenceWallpaperInterfaceFromSurface(surface)` 返回对象，并能得到同一个真实 `WOutput` |
| 配置写入 | `Helper::currentWorkspaceWallpaper(output)` 等于客户端发送的 source |

因此通过条件是 manager 写入配置且与同一 shell surface/output 建立生产关联；不把 role-less
surface 的映射或最终 QML 内容当作该协议的行为。

## 已知边界

- 尚未覆盖实际 wallpaper owner 对该 surface 的映射、`ready` 完成、QML 接入或最终 output
  backing buffer，因此尚未达到 V 级。
- 未覆盖 JPEG/视频文件校验、解码失败、notifier 的真实来源，也未覆盖锁屏 role、跨 workspace
  切换和淡入淡出动画完成。
