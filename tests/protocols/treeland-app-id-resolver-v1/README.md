# `treeland-app-id-resolver-v1` 测试规范

## 范围

- 协议/资源级测试：`tests/protocols/treeland-app-id-resolver-v1/`。
- 覆盖等级：**P / E**。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| 创建 resolver | `get_resolver` | 创建可用 resolver 资源 |
| 正常解析 | 对真实 pidfd 的 `identify` 应答 | 生产回调启动、收到应答并记录 app-id |
| 空解析 | 以空 app-id 应答 | 生产回调记录空结果 |
| 无效应答 | 在请求失效后应答 | manager 发送规定的协议错误 |

## 已证明的生产链路（E）

桌面测试启动真实 `Helper`、`ShellHandler`、`Workspace` 和 headless output。它先用
同一 Wayland 客户端创建 app-id 为
`org.deepin.treeland.protocol.app-id-resolver` 的 prelaunch splash，确认生产
`SurfaceWrapper` 已加入 workspace；这满足 `ShellHandler` 对随后 xdg toplevel 走
app-id resolver 异步路径的业务前提。

随后客户端先创建并提交**未配置**的 xdg toplevel。此时生产端从该 xdg 客户端的
credentials 取得 pidfd，并通过 app-id-resolver 向同一客户端发送
`identify_request(request_id, pidfd)`。测试确认收到非零 request id 和有效 pidfd，
再响应：

```text
respond(request_id,
        "org.deepin.treeland.protocol.app-id-resolver",
        "org.deepin.Sandbox")
```

这个两阶段顺序是协议语义的一部分：在 resolver 回包前，`ShellHandler` 有意延后
xdg wrapper 创建，因此测试不会错误地以固定时延或过早的 `xdg_surface.configure`
要求代替 resolver 结果。

生产调用链如下：

```text
xdg toplevel（真实客户端 pidfd）
  → ShellHandler::onXdgToplevelSurfaceAdded
  → AppIdResolverManager::resolvePidfd
  → identify_request / respond
  → ShellHandler::ensureXdgWrapper(surface, appId)
  → 原 SplashScreen SurfaceWrapper::convertToNormalSurface
```

| 阶段 | 客户端动作 | 必须观察到的生产业务结果 |
| --- | --- | --- |
| resolver 请求 | 提交 pending xdg toplevel | 收到针对该真实客户端的 `identify_request` 与 pidfd |
| resolver 应答 | 以测试 app-id 调用 `respond` | 生产端允许 xdg surface 获得 configure，并完成 map |
| wrapper 转换 | 读取先前记录的 splash wrapper | **同一** wrapper 的类型从 `SplashScreen` 变为 `XdgToplevel` |
| app-id 写入 | 查询生产 wrapper | `wrapper.appId()` 等于 responder 返回的 app-id |
| workspace 连续性 | 查询 `Workspace::surfaces()` | 转换后的 wrapper 仍在 workspace，且只有这一个测试窗口对象 |

## 生产结果

基础测试证明请求/应答和 pidfd 传输进入生产 resolver 模块。桌面测试进一步证明
resolver 的返回值驱动了实际窗口 wrapper 的创建/转换与 app-id 写入。

## 已知边界 / 下一项结果

当前 resolver client 仍是测试客户端，而非外部真实应用进程；但它使用真实 Wayland
连接、真实 pidfd 和生产 `ShellHandler`。尚未覆盖 sandbox 名称在后续策略中的使用、
resolver 断开时 pending xdg 的空 app-id fallback，以及 XWayland 路径。
