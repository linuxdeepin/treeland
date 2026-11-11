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
| 重复绑定 | 第二个客户端 bind manager | 收到 `wl_display.error`，错误接口为 manager、错误码为 `binding_rejected`；原客户端绑定与已建 splash 不变 |
| 拒绝后原客户端可用 | 被拒绝期间原客户端 `create_splash` | 生产 handler 仍收到请求 |
| 释放后重绑 | 原客户端销毁最后一个 manager 后其他客户端 bind | 绑定成功 |
| 断开释放 | 持 manager 的客户端断开后其他客户端 bind | 绑定成功 |
| 会话隔离 | 两个 Session 各自 bind 并创建 splash | 各自成功、互不影响（`session-isolation` target 覆盖） |

## 已证明的生产链路（E）

桌面测试启动真实 `WaylandServer`、`ShellHandler`、`Workspace`、root output/container 和
headless output。fixture 在全局 DConfig 初始化完成后明确启用
`enablePrelaunchSplash`，因此测试不会被机器上遗留的全局关闭值误判。

客户端向真实 `treeland_prelaunch_splash_manager_v2` 发送：

```text
create_splash(
  app_id = "org.deepin.treeland.protocol.splash",
  instance_id = "protocol-instance",
  sandbox = "org.deepin.Sandbox",
  icon = 2×2 wl_shm buffer)
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
| 图标锁住 | 发送 `create_splash` 后立即销毁客户端 `wl_buffer` | wrapper 的 `appIcon()` 仍可读，尺寸与提交的 `2 × 2` 一致：客户端已不再持有该 buffer，读到的指针仍然有效 |
| 关闭 | 销毁客户端 splash 资源 | `ShellHandler::handlePrelaunchSplashClosed` 经 root container 销毁 wrapper；wrapper 离开 workspace 且发出 destroyed |

## 生产结果

基础测试使用真实 `wl_buffer`，并观察生产 splash request/close 信号。桌面测试进一步
证明该信号已进入生产 `ShellHandler`，实际生成并管理 splash wrapper，并在客户端释放
`wl_buffer` 之后仍能通过 `SurfaceWrapper::appIcon()` 读到图标。

## 已知边界 / 下一项结果

本目录的断言都发生在 splash 阶段内部：此时 splash QML item（`PrelaunchSplash.qml` 内的
`BufferItem`）自己也持有一份 buffer 锁，所以「客户端释放后指针仍有效」只能说明至少还有一个
持有者，不能证明是 wrapper 的锁在保它，也没有覆盖「splash item 被销毁之后」这一真正目标场景。

跨阶段保留与 dock preview 实际绘制由
[treeland-prelaunch-splash-desktop-v2](../treeland-prelaunch-splash-desktop-v2/README.md)
覆盖：它在 splash item 已销毁、客户端 `wl_buffer` 已销毁之后读取 `appIcon()`，并把该 buffer
读回为图标纹理像素。splash 自身的可见性与像素仍未验证。
