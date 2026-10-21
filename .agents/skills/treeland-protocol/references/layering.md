# 分层模型（public 类 / private manager / private resource）

由 `SKILL.md` 按需加载：需要决定「哪些代码放 public 头文件、哪些留 `.cpp`」或「要不要为子对象建 public 封装」时读本文件。

## 分层模型
当前 treeland 私有协议的主流设计把业务 API 与协议实现细节分开。

### public 类
public 头文件暴露面向业务的 API，不暴露 `QtWaylandServer::*` 类型。

public 类通常：

- 继承 `QObject` 与 `WServerInterface`
- 向业务代码暴露方法、信号和少量查询 API
- 实现 `create/destroy/global/interfaceName`
- 用 `std::unique_ptr` 持有 private manager

`WaylandServer::init` 与其他 treeland 业务代码 attach、连接、调用的就是这个 public 类。

### private manager 类
定义在 `.cpp` 中，直接继承 `QtWaylandServer::treeland_xxx_manager_v1`。

它通常负责：

- 协议 global 生命周期
- manager 请求分发
- `bind_resource(...)` 中的初始状态同步
- 创建子 resource 或 context 对象

### private resource / context 类
定义在 `.cpp` 中，继承 `QtWaylandServer::treeland_xxx_v1`。

它通常对应一个具体协议对象，负责：

- 处理该 resource 的请求
- 在 `destroy_resource(...)` 中清理
- 把协议事件翻译成 public 类的信号、回调或业务动作

### 何时需要 public 子对象封装
不是每个子 resource 都需要自己的 public 头文件。

若子对象只是 manager 的内部细节、没有其他 treeland 代码需要的业务 API，就让它保持为 `.cpp` 中的私有 resource 类。

若子对象需要被 treeland 业务代码长期引用、需要 parent、需要发信号、需要额外方法，或被其他类持有，就把它做成 public QObject 封装（类似 foreign-toplevel 中的 handle/context 类），下面挂一个私有协议实现类。

## 设计偏好
规则：

- 只有业务代码真的需要业务 API 时才创建 public 类
- 纯协议细节留在 `.cpp` 私有类中

不要因为协议里有某个子对象就为它建 public 头文件。只有业务代码真的需要知道它时，才提升为 public 封装。
