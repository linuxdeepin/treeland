---
name: treeland-protocol-upstream
description: "用于在 wlroots、waylib、treeland 中集成或封装上游 Wayland 协议。触发场景：standard/ext/unstable/wlr 协议、wlroots 已实现的协议、waylib `WServerInterface` 封装、`m_server->attach<W...>()`、native handle 生命周期、把上游协议接到 `WaylandServer::init` 启动路径。"
---

# 上游协议封装与集成

> **铁律**：先确认 waylib 是否已有 `W...` 封装；销毁方式必须与 native handle 所有权一致，不要照抄 `globalRemove()`。

## 范围
本 skill 只用于上游协议，不用于 treeland 私有协议。

出现以下任一情况即适用：

- 协议是 standard/ext/unstable/wlr 协议
- wlroots 已实现该 native 协议
- 目标是 waylib `WServerInterface` 封装
- treeland 通过 `m_server->attach<W...>()` 消费它

若任务涉及 treeland 自有 xml、`QtWaylandServer::*`，或在 `src/modules/*` 下新增私有模块，停下来改用 treeland 私有协议 skill。

## 启动装配
`WaylandServer` 既是 QML singleton，也是 C++ 侧桌面入口。其 `init()`（`src/core/waylandserver.cpp`）负责组装启动流程，并持有 Wayland 运行时与核心 global。各功能组件自己持有协议回调与监听状态。保持启动顺序，策略回调留在所属功能组件内。旧的 `Helper` 兼容类已被移除。

## 目标
- 确认 waylib 是否已有封装
- 需要时新增 `WServerInterface` 封装
- 把 native handle 的所有权与销毁写清楚
- 把结果接到 `WaylandServer::init` 的启动路径上

## 先读
1. `waylib/src/server/protocols`
2. `waylib/src/server/kernel`
3. `3rdparty/wlroots/include/wlr`（wlroots 原始 C API 头文件 —— 读其中的 native `wlr_*` 类型与 `wl_signal` 事件；包含方式统一用 `<wlr_all.h>`）
4. `src/core/waylandserver.cpp` 与相关功能控制器

先搜索已有封装，不要假设 treeland 应该自己在本地重新实现一个 manager。

## 第一原则
上游协议优先复用 wlroots 的 native 实现，由 waylib 提供 Qt/C++ 封装层。treeland 侧通常只负责 attach 封装并连接业务行为。

treeland 侧的首选形态通常是：

```cpp
auto *iface = m_server->attach<W...>();
connect(iface, ...);
```

## 最小封装要求
提供 `WServerInterface` 子类，至少实现：

- `interfaceName() const`
- `create(WServer *server)`
- `destroy(WServer *server)`
- `global() const`

`create`、`destroy`、`global` 在 `WServerInterface` 上是 `protected` 虚函数（见 `waylib/src/server/kernel/wserver.h`）；`interfaceName()` 是唯一的 public 纯虚函数。

如果封装需要注册 native `wl_signal` 监听，它必须同时继承 `WObject`，即 `class WFoo : public QObject, public WServerInterface, public WObject` —— 只继承 `WServerInterface` 是没有 `listeners()` 和 `teardown()` 的。

### 最小骨架
照抄邻近封装，不要自创第三种形态：

```cpp
// wfoo.h
class WFoo : public QObject, public WServerInterface, public WObject
{
    Q_OBJECT
public:
    ~WFoo() override;

    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};
```

```cpp
// wfoo.cpp
WFoo::~WFoo()
{
    // 注册了 wl_signal 监听就必须调 teardown()；~WObject 在还有监听组残留时会 abort。
    teardown();
}

void WFoo::create(WServer *server)
{
    // WServer::handle() 返回原始 wl_display*，直接交给 wlroots 的 C 构造函数；
    // 返回的指针存进 m_handle（void*）。
    m_handle = wlr_foo_manager_v1_create(server->handle(), FOO_MANAGER_V1_VERSION);
}

void WFoo::destroy([[maybe_unused]] WServer *server)
{
    // 该类型没有 wlr_foo_manager_v1_destroy()：native 对象随 wl_display 回收，
    // 只需清空句柄，使重启后能重新创建 global。
    m_handle = nullptr;
}

wl_global *WFoo::global() const
{
    if (!m_handle)
        return nullptr;
    // nativeInterface<T>() 把 m_handle 重新解释成 T*；wlroots C 结构体直接暴露 global 字段。
    return nativeInterface<wlr_foo_manager_v1>()->global;
}

QByteArrayView WFoo::interfaceName() const
{
    // 与邻近封装一致，直接返回 wl_interface 名（不是 global 名，也不是 xml 文件名）。
    return "zwlr_foo_manager_v1";
}
```

## 如何判断销毁
不要机械照抄 treeland 私有协议里的 `globalRemove()`。

native handle 是存在 `m_handle`（`void*`）里的 wlroots 原始 C 指针。销毁方式取决于 native 所有权：

- 如果 `wlr_*_create(...)` 返回的对象没有公开的 destroy 函数，`destroy()` 通常只需清空 `m_handle = nullptr` —— native 对象会在 `WServer::stop()` 销毁 `wl_display` 时被回收
- 如果该 wlroots 类型提供显式 `wlr_*_destroy(...)` / reset / listener 清理 API，就在 `destroy()` 里调用它
- waylib 的 `WObject` 作用域监听列表（`listeners()->add(...)`）会在 `destroy()` 之前自动摘除：`WServer::stop()` 和 `WServer::detach()` 会对属于 `WObject` 的接口调用 `WObject::teardown()`。不要释放 display teardown 已经负责的 native 状态。`~WObject` 在还有监听组残留时会 abort，所以注册了监听的封装必须同时在自己的析构函数里调用 `teardown()`（见 `waylib/src/server/kernel/wglobal.h` 中 `WObject::teardown()` 的注释）。

判断方法：

1. 读同目录下邻近的 `W...` 封装
2. 读 `3rdparty/wlroots/include/wlr` 中对应 `wlr_*` 类型是否提供显式 `destroy()` 之类 API
3. 确认 `create()` 是否被 `if (!m_handle)` 保护；如果是，`destroy()` 必须清空 `m_handle`，否则重启后无法重新创建 global

核心规则：销毁方式必须与 native handle 所有权一致。

## 通过 `WaylandServer::init` 接线
最终仍要接回 `src/core/waylandserver.cpp`：

1. `m_server->attach<W...>()`
2. 需要时把对象存成成员或局部变量
3. 紧接着写业务 `connect(...)`
4. 接入 `ShellHandler`、surface/output 管理或 QML 层

不要把“必须在 `WaylandServer::init` 里直接 attach”当成唯一合法形式。

有些协议可以通过更高层的对象初始化，例如：

```cpp
m_shellHandler->init(m_server, m_seat);
```

这同样算有效集成。真正的判断标准是：

- 启动流程是否仍然从 `WaylandServer::init` 进入
- 更高层对象是否干净地封装了某个 shell 或协议族的初始化
- 注册入口与生命周期所有权是否依然清晰

若某个封装既没有从 `WaylandServer::init` 直接 attach，也没有通过更高层对象初始化，那这个协议对 treeland 来说仍未真正可用。

## 参考起点
- `waylib/src/server/protocols/wforeigntoplevelv1.cpp`
- `waylib/src/server/protocols/wextforeigntoplevellistv1.cpp`
- `waylib/src/server/kernel/wserver.h`
- `src/core/waylandserver.cpp`

## 输出要求
用本 skill 处理真实任务时，先明确这几点：

1. wlroots 是否已实现该协议
2. waylib 是否已有 `W...` 封装
3. 若没有，native handle 归谁所有、由谁负责销毁
4. treeland 从 `WaylandServer::init` 的哪个位置进入启动路径，业务 `connect(...)` 应该放在哪里
