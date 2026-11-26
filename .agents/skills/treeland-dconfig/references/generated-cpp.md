# 生成的 DConfig C++ 封装

本文件记录 `dtk_add_config_to_cpp` / `dconfig2cpp` 生成的头文件长什么样、能依赖哪些 API。
结论以 `build/src/` 下的实际生成文件为准；这里只是让你不用每次重新推导。

不要在源码树里搜索生成的配置头文件，它们由 CMake/dconfig2cpp 生成到 `build/src/`。

执行 `cmake --preset default` 后直接检查生成头文件，例如：

```bash
sed -n '1,260p' build/src/treelanduserconfig.hpp
```

当前生成头文件呈现这些 API 与约定：

- 头文件注释记录 `dconfig2cpp` 命令行、输出路径、类名、源 JSON、JSON 版本与工具版本。
- include guard/类宏前缀由 `CLASS_NAME` 推导，例如 `TREELANDUSERCONFIG_DCONFIG_FILE_VERSION_MAJOR` 与 `TREELANDUSERCONFIG_DCONFIG_FILE_VERSION_MINOR`。
- 生成的 `*_DCONFIG_FILE_VERSION_MINOR` 宏反映生成器 API 兼容性；即使 `Q_CLASSINFO("DConfigFileVersion", "1.0")` 来自 JSON 描述文件，当前生成头文件也把它定义为 `1`。新代码不需要旧 DTK 兼容分支。
- 每个键生成 `#define <CLASS>_DCONFIG_FILE_<key>`。
- 每个键生成带 getter、setter、`*Changed` 信号与 `reset*` 方法的 `Q_PROPERTY`。
- JSON 整数生成为 `qlonglong`；JSON 浮点生成为 `double`；字符串为 `QString`；布尔为 `bool`。
- 类提供 `keyList()`、`isDefaultValue(key)`、按键的 `*IsDefaultValue()`、`config()`、`isInitializeSucceeded()`、已废弃的 `isInitializeSucceed()`、`isInitializeFailed()`、`isInitializing()`。
- 类同时发出具体的 `keyChanged()` 信号与通用的 `valueChanged(const QString &key, const QVariant &value)`。
- setter 先更新缓存的属性存储，再在底层 `DConfig` 对象存在时异步调用 `DConfig::setValue(...)`。
- 初始化在 `DTK_CORE_NAMESPACE::DConfig::globalThread()` 上异步进行。构造函数发出 `configInitializeSucceed(DConfig *)` 或 `configInitializeFailed()`。

每个类生成的创建 helper：

```cpp
static ClassName *create(const QString &appId = {},
                         const QString &subpath = {},
                         QObject *parent = nullptr,
                         QThread *thread = DTK_CORE_NAMESPACE::DConfig::globalThread());

static ClassName *createByName(const QString &name,
                               const QString &appId = {},
                               const QString &subpath = {},
                               QObject *parent = nullptr,
                               QThread *thread = DTK_CORE_NAMESPACE::DConfig::globalThread());
```

生成的描述文件名已足够时用 `create(...)`；现有 treeland 代码显式传入描述文件名与 app id 时（用户、output、seat 配置路径）用 `createByName(...)`。


用户级配置通过 `createByName(DConfigBackend *backend, name, appId, subpath, parent, thread)` 注入 `UserConfigBackend`；backend 由 DConfig 持有，创建后将生成类的 `configInitializeSucceed` 连接到 `UserConfigBackend::subscribe`。后端与 DConfig 均使用默认全局工作线程，不能为该后端指定另一条生成类工作线程。
