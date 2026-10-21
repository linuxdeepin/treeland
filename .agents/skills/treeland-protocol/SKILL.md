---
name: treeland-protocol
description: "用于 treeland 私有 Wayland 服务端协议集成。触发场景：treeland 自有 xml、`treeland_*` 接口、`src/modules/*`、QtWayland scanner 输出、`QtWaylandServer::*`、`waylib_generate_qtwayland_server_protocol(...)`、`globalRemove()`、`destroy_resource(...)`、把私有协议接到 `WaylandServer::init` 启动路径。"
---

# Treeland 私有协议集成

> **铁律**：`destroy(...)` 只做 `wl_resource_destroy(resource->handle)`；真正的业务清理只放 `destroy_resource(...)`。

## 范围
本 skill 只用于 treeland 私有协议，不用于上游 wlroots/waylib 协议。

出现以下任一情况即适用：

- 协议 xml 由 treeland 维护
- 接口名通常是 `treeland_*`
- 代码位于 `src/modules/*`
- CMake 使用 `waylib_generate_qtwayland_server_protocol(...)`
- 生成类型是 `QtWaylandServer::*`

若协议是 standard/ext/unstable/wlr 上游协议，停下来改用上游协议 skill。

## 启动装配
`WaylandServer` 既是 QML singleton，也是 C++ 侧桌面入口。其 `init()`（`src/core/waylandserver.cpp`）负责组装启动流程；`src/core/waylandserver.cpp` 持有 Wayland 运行时与核心 global。各功能组件自己持有协议回调与监听状态。保持启动顺序，策略回调留在所属功能组件内。旧的 `Helper` 兼容类已被移除。

## 目标
把下面四件事做对：

- 生成服务端协议代码
- 实现 manager 与 resource 类
- 通过 `WServerInterface` 集成
- 接到 `WaylandServer::init` 的启动路径上

## 先读
1. 目标模块的 `CMakeLists.txt`
2. `waylib/tools/CMakeLists.txt`
3. 构建目录中的生成文件
4. `src/core/waylandserver.cpp` 与相关功能控制器

先看生成文件（在对应模块的构建目录里，不在源码树）：

- `build/<模块目录>/qwayland-server-<basename>.h/.cpp`
- `build/<模块目录>/wayland-<basename>-server-protocol.h/.c`

不要猜生成类名、`Resource` 回调签名、`init(...)` 重载或 `interfaceName()`。

注意：`treeland_*.xml` 协议文件由外部 CMake 包 `TreelandProtocols` 提供（通过 `find_package(TreelandProtocols REQUIRED)`），不在 treeland 源码树中。通过 `${TREELAND_PROTOCOLS_DATA_DIR}/treeland-*.xml` 访问。不要在 `src/modules/` 下搜索 XML 文件。

从生成的 `qwayland-server-*.h/.cpp` 中直接确认至少这些事实：

- 生成了哪些 helper API，例如 `resourceMap()`、`resource()`、`isGlobalRemoved()`、`interfaceVersion()`
- `destroy(...)`、`destroy_resource(...)`、`bind_resource(...)` 的默认行为是什么
- `handle_destroy(...)` 只是把销毁请求转发给你重写的 `destroy(Resource *resource)`；真正销毁通常仍需 `wl_resource_destroy(resource->handle)`
- `destroy_func` 会在调用你的 `destroy_resource(...)` 之前把 resource 从 `m_resource_map` 中移除
- `destroy_resource(...)` 返回后，生成代码会删除生成的 `Resource` 包装对象；这不代表你自己的 QObject/私有对象也被自动删除
- `globalRemove()` 不会立即调用 `wl_global_destroy()`；它先调用 `wl_global_remove(...)`，再延迟销毁

这些细节决定实现边界：

- `destroy_resource(...)` 负责清理业务对象与业务状态，不负责删除生成的 `Resource` 包装对象
- 调用 `globalRemove()` 之后更准确的含义是“global 移除已安排”，而不是“已同步销毁”
- 如果 xml 中 manager 接口没有 destructor 请求，生成代码不会为该 manager 提供 `handle_destroy(...)` 或 `virtual void destroy(Resource *resource)`；此时不要假设可以重写 manager 的 destroy

## 构建路径
优先使用已有 helper（定义在 `waylib/tools/CMakeLists.txt`）：

`waylib_generate_qtwayland_server_protocol(<target> PROTOCOL <xml> BASENAME <basename> [PREFIX <prefix>])`

第一个参数是持有生成源码的 CMake target（通常是 `libtreeland`）；`PROTOCOL` 是 xml 路径，`BASENAME` 决定生成文件名。可选的 `PREFIX` 设置生成的 C++ 类型/命名空间前缀（例如 `src/modules/personalization/` 中的 `PREFIX "/"`）；默认空前缀时可省略。

典型产物：

- `wayland-<basename>-server-protocol.c`
- `qwayland-server-<basename>.h/.cpp`

至少确认：

1. target 源码列表包含 `wayland-<basename>-server-protocol.c`
2. include 路径包含 `${CMAKE_CURRENT_BINARY_DIR}`
3. `BASENAME`、生成文件名与 C++ 类型名一致
4. **`BASENAME` 必须等于 `<protocol name>` 把下划线换成短横线**（例如 `<protocol name="treeland_screensaver_unstable_v2">` → `BASENAME treeland-screensaver-unstable-v2`）。生成的 `.cpp` 内部 `#include` 由 `<protocol name>` 推导（下划线→短横线），而生成文件名由 `BASENAME` 推导；两者不一致时生成的 `.cpp` 找不到自己的头文件，构建失败。因此即使只改 `<protocol name>`（接口名不变），也必须同步更新 `BASENAME` 以及源码中每一处 `#include "qwayland-server-<new-basename>.h"`。

## 推荐代码结构
- public 类：`QObject + WServerInterface`
- private manager：继承 `QtWaylandServer::treeland_xxx_manager_v1`
- private resource：继承 `QtWaylandServer::treeland_xxx_v1`

把 `QtWaylandServer::*` 继承留在 `.cpp` 的私有实现类里，不要泄漏进 public 头文件。

判定规则：只有业务代码真的需要业务 API 时才创建 public 类，纯协议细节留在 `.cpp` 私有类中。不要因为协议里有某个子对象就为它建 public 头文件。

需要各层职责清单、以及「何时把子对象提升为 public 封装」时，读 `references/layering.md`。

## 请求处理器的参数校验
libwayland 在请求处理器执行前会校验每个对象参数：非 nullable 参数永远不为 NULL，未知 id 与类型不匹配会让分发以协议错误失败，已销毁的 resource 不再可解析，只有标记 `allow-null="true"` 的参数才可能为 NULL。因此请求处理器无需校验传入对象是否符合协议声明类型或是否仍存活 —— 这类检查是不可达的死代码。

分发层无法知道、处理器仍必须检查的内容：跨客户端所有权（`wl_resource_get_client`）、inert resource（`wlr_*_from_resource` 返回 NULL）、缺失的封装（`WSurface::fromHandle` / `WOutput::fromHandle` 返回 nullptr）、协议取值规则（正数尺寸、枚举范围），以及 xml 标记 `allow-null="true"` 时的 NULL。

## Manager 接口接线
public 类通常实现：

- `create(WServer *server)`
- `destroy(WServer *server)`
- `global() const`
- `interfaceName() const`

推荐写法：

```cpp
// WServer::handle() 返回原始 wl_display*。直接传给生成 manager 的 init()。
void Manager::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void Manager::destroy(WServer *server)
{
    Q_UNUSED(server);
    d->globalRemove();
}

// 在私有 manager 类中（.cpp）：
// m_global 在生成基类中是 protected。public 类无法直接访问
// d->m_global（protected 不适用于外部指针访问）。
// 通过私有子类中的小访问器暴露出来：
wl_global *globalHandle() const { return m_global; }

// public 类中：
wl_global *Manager::global() const
{
    return d->globalHandle();
}

QByteArrayView Manager::interfaceName() const
{
    return QtWaylandServer::treeland_foo_manager_v1::interfaceName();
}
```

规则：

- 在 public manager 类中定义 `static constexpr int InterfaceVersion = N;`，并在 `create()` 中使用
- 在 `destroy()` 中直接调用 `globalRemove()`
- 不要用多余的 `if (m_global)` 包一层
- 子接口/resource 版本不绑定 `resource->version()` 时，同样使用类作用域常量

### 常见 manager 变体
不要把 manager 只理解成“创建 global 的东西”。

当前代码中 manager 还经常：

- 在 `bind_resource(Resource *resource)` 中发送初始状态
- 在请求处理器里用 `wl_resource_create(...)` 手动创建子 resource
- 用 `resource->version()` 作为子 resource 版本，而不是另造硬编码版本

新私有协议应默认带 manager `destroy` 请求，并实现：

```cpp
void destroy(Resource *resource) override
{
    wl_resource_destroy(resource->handle);
}
```

少数已有 manager 缺 `destroy` 请求属于历史设计问题，当作兼容例外，不要作为新协议的模板。

该请求本身属于协议 xml，而 xml 在外部 `TreelandProtocols` 包中（见“先读”中的说明）。添加它意味着在该包发布新的 xml 版本，本仓库无法声明它。

## 资源销毁
这是最容易出错的部分，但分工只有一条：`destroy(...)` 只做 `wl_resource_destroy(resource->handle)`，业务清理只放 `destroy_resource(...)`。

判定规则：先问「这个 C++ 对象是否只为该 resource 创建、并应随它消亡？」

- 是：`destroy_resource(...)` 删除该对象；回调可能重入时用 `deleteLater()` 而不是 `delete this`
- 否：只清理该 resource 绑定的状态（从 map/列表移除 `resource->handle`，清 owner/watch/callback 记录）

`destroy_resource(...)` 返回后生成代码会删除生成的 `Resource` 包装对象，但这不代表你自己的 QObject 被删除。完整的三种场景、manager 按客户端 resource 的处理、子 resource 创建模板（`wl_resource_create` + `wl_client_post_no_memory`）见 `references/lifetime.md`。

## 通过 `WaylandServer::init` 接线
最终必须接回 `src/core/waylandserver.cpp`。

检查：

1. 是否通过 `m_server->attach<...>()` 初始化
2. 需要时是否存成 `WaylandServer` 成员
3. attach 之后是否立即写业务 `connect(...)`
4. 是否接入 `ShellHandler`、`RootSurfaceContainer`、`QmlEngine` 或其他既有对象

不要把“注册点必须在 `WaylandServer::init`”理解得过死。

仓库中也有由更高层对象封装协议初始化的情形，例如：

```cpp
m_shellHandler->init(m_server, m_seat);
```

这同样算协议注册入口。真正的判断标准是：

- 启动流程是否仍然从 `WaylandServer::init` 进入
- 更高层对象是否干净地封装了某个协议族的初始化
- 注册入口与生命周期所有权是否依然易于定位

若某个协议既没有从 `WaylandServer::init` 直接注册，也没有通过 `ShellHandler` 这类更高层对象初始化，那它通常仍未完成集成。

## 反面样例
`capture` 使用 `ws_generate_local`（裸 wayland-scanner）而不是首选的 `waylib_generate_qtwayland_server_protocol(...)` 路径，且完全没有使用 `QtWaylandServer::*`。不要把它当模板。

`foreign-toplevel` 已完全迁移到 `QtWaylandServer::*` + `waylib_generate_qtwayland_server_protocol(...)` 路径。它是需要 public 子对象封装（`ForeignToplevelHandleV1`、`DockPreviewContextV1`）的合法参考，但体量和复杂度不适合作为简单新协议的起点。

新私有协议优先参考 `appidresolver`、`prelaunch-splash` 或 `screensaver`。

## 参考起点
- `src/modules/app-id-resolver/appidresolver.cpp`
- `src/modules/prelaunch-splash/prelaunchsplash.cpp`
- `src/modules/screensaver/screensaverinterfacev2.cpp`
- `waylib/tools/CMakeLists.txt`
- `src/core/waylandserver.cpp`

## 引用 wlr_* 接口的协议
如果 treeland xml 的对象参数使用了 wlroots 协议定义的接口（例如 `treeland-output-uuid-head-unstable-v1.xml` 接收 `zwlr_output_head_v1`），生成的服务端代码会引用该接口的 `wl_interface` 符号，例如 `zwlr_output_head_v1_interface`。

waylib-wlroots 只导出 `wlr_*`、`_wlr_*` 与 `waylib_*`（`3rdparty/wlroots/wlroots.syms`），因此所有协议符号对 `libwaylib-wlroots.so` 都是局部的，无法从 libtreeland 解析。需要在 libtreeland 中也生成该 wlr 协议，在消费模块里写：

```cmake
waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${CMAKE_SOURCE_DIR}/3rdparty/wlroots/protocol/wlr-output-management-unstable-v1.xml
    BASENAME wlr-output-management-unstable-v1
)
```

容易漏掉的原因：共享库默认允许未定义符号，所以 `--target libtreeland` 会成功。错误只在可执行文件链接 libtreeland 时才暴露（`undefined reference to <iface>_interface`），而损坏的 `libtreeland.so` 在运行时加载也会失败。

## 输出要求
用本 skill 处理真实任务时，先明确这几点：

1. xml 与生成文件在哪里
2. manager 的 `create/destroy/global/interfaceName` 应如何实现
3. `destroy(...)` 与 `destroy_resource(...)` 的职责如何划分
4. 协议从 `WaylandServer::init` 的哪个位置进入启动路径
5. 请求处理器不再添加分发层已经完成的参数校验（见“请求处理器的参数校验”）
