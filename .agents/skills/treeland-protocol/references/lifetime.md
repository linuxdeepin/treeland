# 资源生命周期（destroy / destroy_resource）

由 `SKILL.md` 按需加载：需要判断「对象该不该在 `destroy_resource(...)` 里删除」、处理 manager 的按客户端 resource、或创建子 resource 时读本文件。

## 客户端显式销毁
在 `destroy(Resource *resource)` 中只做：

```cpp
wl_resource_destroy(resource->handle);
```

不要把真正的业务清理放这里。

## 最终清理
真正的清理路径优先放在 `destroy_resource(Resource *resource)`。在这里：

- 更新存活状态
- 发出断开/关闭相关信号
- 清理回调与连接
- `delete this` 或 `deleteLater()`

典型写法：

```cpp
void destroy_resource(Resource *) override
{
    m_alive = false;
    if (onDisconnected)
        onDisconnected();
    delete this;
}
```

如果回调可能运行在 Qt 信号分发或其他可重入路径中，优先用 `deleteLater()`。

不要假设 `destroy_resource(...)` 一定会删除对象本身。当前代码中也存在这些模式：

- manager 的 resource 只清理与该 `resource->handle` 关联的 map、watch 列表或 owner map
- QObject 生命周期由 parent、外层封装对象或显式容器持有

也就是说，`destroy_resource(...)` 的核心职责是"干净地结束这个 resource 的生命周期"，而不必然是"立即删除所有相关对象"。

## 何时删除对象
如果 QObject/私有对象是专为单个 Wayland resource 创建、且没有其他持有者，`destroy_resource(...)` 就应结束该对象的生命周期。

典型特征：

- 构造函数直接接收 `wl_resource *` 或 `Resource *`
- 一个对象严格对应一个客户端 resource
- 对象存在的唯一目的就是包装这个协议对象
- 没有 parent、容器或 manager 继续持有它

典型实现：

```cpp
void destroy_resource(Resource *) override
{
    delete this;
}
```

若存在重入风险，用：

```cpp
void destroy_resource(Resource *) override
{
    deleteLater();
}
```

## 何时不删除对象
如果 QObject 不是由单个 resource 独占，只是借助 resource 跟踪某个客户端的状态，`destroy_resource(...)` 就只应清理该 resource 相关状态。

典型特征：

- 对象本身是 manager，跟随 global 或外层接口的生命周期
- 对象有明确的 QObject parent，由 parent 负责销毁
- 对象仍被静态列表、管理容器或业务代码持有
- 一个对象管理多个 resource，而不是严格一对一

这种情况下通常只做：

- 从 map/列表/集合中移除 `resource->handle`
- 清理 owner、watch、callback 记录
- 更新连接状态或可用性标记

不要因为一个客户端 resource 被销毁就删除仍在服务其他状态的 manager 或共享对象。

## 判定规则
先问一个问题：

"这个 C++ 对象是否只为该 resource 创建、并应随它消亡？"

- 是：`destroy_resource(...)` 应删除该对象
- 否：`destroy_resource(...)` 只应清理与该 resource 绑定的状态

## Manager 的按客户端 resource
如果 manager 只有一个轻量的按客户端代理 resource，`destroy(Resource*)` 里调用 `wl_resource_destroy(...)` 通常就够。不要把业务状态清理堆进去。

## 子 resource 创建
很多现有私有协议采用"manager 手动创建子 resource，再交给私有封装对象"的模式，例如：

```cpp
auto *child = wl_resource_create(resource->client(),
                                 &treeland_foo_v1_interface,
                                 resource->version(),
                                 id);
if (!child) {
    wl_client_post_no_memory(resource->client());
    return;
}
new FooContext(child);
```

该模式下要确认：

- 子 resource 版本跟随 `resource->version()`
- 失败时用 `wl_client_post_no_memory(...)`
- 需要时在子对象销毁时把它从静态列表、map 或 owner 结构中移除
