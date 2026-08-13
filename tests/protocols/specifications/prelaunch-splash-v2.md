# `treeland-prelaunch-splash-v2` 测试规范

## 范围

- 协议/资源级测试：`tests/protocols/treeland-prelaunch-splash-v2/`。
- 桌面生产链路测试：`tests/protocols/treeland-prelaunch-splash-desktop-v2/`。
- 覆盖等级：**I / E**。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| 创建 splash | 用空 icon 和真实 `wl_shm` icon buffer 创建 | 生产 handler 每次均发出请求，并保留 app/instance/icon 信息 |
| 同 app | 用相同 app-id 再创建 | 请求不会被错误去重 |
| 关闭 | 销毁 splash 资源 | 生产 close 请求携带对应 app-id 与 instance-id |

## 已证明的生产链路（E）

桌面测试启动真实 `Helper`、`ShellHandler`、`Workspace`、root output/container 和
headless output。fixture 在全局 DConfig 初始化完成后明确启用
`enablePrelaunchSplash`，因此测试不会被机器上遗留的全局关闭值误判。

客户端向真实 `treeland_prelaunch_splash_manager_v2` 发送：

```text
create_splash(
  app_id = "org.deepin.treeland.protocol.splash",
  instance_id = "protocol-instance",
  sandbox = "org.deepin.Sandbox",
  icon = null)
```

测试观察的不是 fixture 手工创建的对象，而是以下生产调用链：

```text
create_splash
  → PrelaunchSplash::splashRequested
  → ShellHandler::handlePrelaunchSplashRequested
  → WindowConfigStore::withSplashConfigFor
  → ShellHandler::createPrelaunchSplash
  → new SurfaceWrapper(... SplashScreen ...)
  → Workspace::addSurface
```

`WindowConfigStore` 的每应用配置可能异步完成。测试只等待两个精确的生产事件，
不以固定延时或轮询推测服务端状态：创建阶段等待 `Workspace` 发出
`SurfaceContainer::surfaceAdded`；销毁阶段等待该 wrapper 的 `QObject::destroyed`。

| 阶段 | 客户端动作 | 必须观察到的生产业务结果 |
| --- | --- | --- |
| 创建 | 发送上述 `create_splash` | workspace 收到 app-id 匹配的 `SurfaceWrapper` |
| wrapper 语义 | 读取该生产 wrapper | `type == SplashScreen`；`appId` 与请求相同；已存在 `prelaunchSplash()` QML item |
| 配置结果 | 读取 wrapper 的 implicit size | 应用配置默认的 `800 × 600` 已写入生产 wrapper |
| 加入桌面 | 查询真实 workspace | wrapper 确实包含在 `Workspace::surfaces()`，不是独立的测试对象 |
| 关闭 | 销毁客户端 splash 资源 | `ShellHandler::handlePrelaunchSplashClosed` 经 root container 销毁 wrapper；wrapper 离开 workspace 且发出 destroyed |

## 生产结果

基础测试使用真实 `wl_buffer`，并观察生产 splash request/close 信号。桌面测试进一步
证明该信号已进入生产 `ShellHandler`，实际生成并管理 splash wrapper。

## 已知边界 / 下一项结果

已验证 QML splash item 被创建，但尚未验证它在某个 `WOutputRenderWindow` 的最终可见性
或像素内容；图标 buffer 的实际纹理/绘制也尚未读回。若要升级到 **V**，应在
`RenderedOutputFixture` 中提交颜色明确的图标或背景，并读取对应 output 的像素。
