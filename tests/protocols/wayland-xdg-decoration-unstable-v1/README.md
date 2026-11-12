# `wayland-xdg-decoration-unstable-v1` 测试规范

## 范围

- XML / interface：`zxdg_decoration_manager_v1` / `zxdg_toplevel_decoration_v1`（version 2）
- 测试源码：`tests/protocols/wayland-xdg-decoration-unstable-v1/`
- Fixture：headless output fixture + pixman 软件渲染 + 映射 xdg-toplevel
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zxdg_decoration_manager_v1` | 资源创建成功 | P |
| 获取装饰 | `get_toplevel_decoration(toplevel)` | 返回非 NULL `zxdg_toplevel_decoration_v1` | P |
| 设置模式 | 分别在独立连接中 `set_mode(Client)` / `set_mode(Server)` | 收到对应 mode 的 `configure` 事件 | P |
| **生产回读** | — | 回读真实 `WXdgDecorationManager::modeBySurface(wsurface)` 返回 Client / Server | **E** |
| **代理装饰切换** | 两种模式分别映射后，由 fixture 将真实 `SurfaceProxy` 切换为 full proxy | 代理不因源窗口的装饰模式禁止显式请求的缩略图圆角 | **I** |
| **软件后端圆角** | `set_mode(Server)` 后映射窗口 | 真实 wrapper 有可见装饰和非空 frame、请求半径大于 0，但 `frameGeometry.radius == 0`，客户区未附加 containment mask | **E** |

## 生产结果

测试观察 Treeland 通过 `WXdgDecorationManager` 提供的 toplevel 装饰服务。
客户端分别请求 Client 和 Server 模式，断言 `configure` 载荷与生产管理器的模式回读一致。
Server 用例还从真实窗口的 `rendererInterface()` 确认软件后端，并检查 `SurfaceWrapper` 的
有效圆角和客户区 input mask，防止误用静态 `QQuickWindow::graphicsApi()` 启用纹理圆角裁剪。

## 已知边界 / 下一项结果

未验证已映射窗口的协议装饰模式切换、None / `unset_mode`、GPU 后端与窗口像素读回。
代理用例检查生产圆角开关与请求值，不将软件后端测试声称为 GPU 缩略图裁剪验证。
