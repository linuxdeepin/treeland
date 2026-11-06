---
name: treeland-dconfig
description: "触发场景：新增/修改/评审/调试 treeland 的 DConfig 配置文件、生成的配置封装类、运行时 DConfig CLI 检查、`misc/dconfig/*.json`、`dtk_add_config_to_cpp`、`treelandconfig.hpp` 等生成头文件、`TreelandConfig`、`TreelandUserConfig`、`OutputConfig`、`SeatUserDConfig`、`AppConfig`、`createByName`、`configInitializeSucceed`、DConfig 的 `magic/version/contents`、`serial`、`flags`、`permissions`、`visibility`、override、`dde-dconfig`、把设置迁入 dconfig。"
---

# Treeland DConfig 配置

> **铁律**：用户级设置必须使用 `"/" + userName` subpath，并通过 `WaylandServer::config()` 或对应的用户级配置入口访问（动画开关为 `WaylandServer::animationConfig()`）；不要因为全局模式以 `dde` 运行就写进 `TreelandConfig`。

## 范围
本 skill 用于由 Desktop Spec Group 配置元数据描述、并通过 DTK DConfig 生成的 C++ 封装类消费的 treeland 配置。

主要仓库位置：

- `misc/dconfig/*.json`
- `misc/dconfig/CMakeLists.txt`
- `src/CMakeLists.txt` 中的 `dtk_add_config_to_cpp(...)` 调用
- `build/src/` 下的生成头文件，例如 `build/src/treelandconfig.hpp`、`build/src/treelanduserconfig.hpp`、`build/src/outputconfig.hpp`、`build/src/seatuserconfig.hpp`、`build/src/appconfig.hpp`
- `src/` 下的消费方

规范参考：

- Desktop Spec Group《配置文件规范》：`https://desktopspec.org/unstable/%E9%85%8D%E7%BD%AE%E6%96%87%E4%BB%B6%E8%A7%84%E8%8C%83.html`。
- 该规范处于 `unstable`，需要确认精确细节时以线上当前版本为准。本 skill 依据 1.0 版（2023.09.01 修订）编写。

## 目标
让 DConfig 改动同时满足 Desktop Spec Group 元数据格式与 treeland 的生成封装用法：

- JSON 描述文件合法且兼容
- 正确的生成 C++ 类暴露类型化 getter、setter 与变更信号
- 读写运行时值之前先处理异步初始化
- 公开设置在非大版本升级间保持兼容
- 安装与构建集成保持同步

## Treeland 运行模式
treeland 有两种运行模式，DConfig 归属必须同时考虑两者：

- 用户模式：用户登录后启动 treeland，行为类似普通窗口管理器，以该用户身份运行。
- 全局模式：首选模式。单个 treeland 进程管理所有用户，用户切换不重启合成器，进程以 `dde` 用户运行。

由于存在全局模式，**绝不要**用 treeland 进程的 uid 推断目标用户的设置。按归属划分：

- 全局配置：与当前登录用户无关的进程/合成器策略。通过 `WaylandServer::globalConfig()` 与 `TreelandConfig` 访问。
- 用户配置：跟随当前选中/登录用户的设置。通过 `WaylandServer::config()` 与 `TreelandUserConfig` 访问；`DConfigManager` 在 `UserModel::currentUserNameChanged` 触发时解析缓存对象；`WaylandServer` 替换当前配置指针并发出 `configChanged()`。

`DConfigManager` 持有生成的配置对象。在 `WaylandServer::WaylandServer` 中，桌面侧观察 `DConfigManager::initialUserConfig()` 与 `DConfigManager::globalConfig()`。

`DConfigManager` 解析用户与 seat 配置对象，并等待两者都初始化完成；用户切换后丢弃过期的完成通知。`initializeActiveUserConfig()` 观察用户变化；`stopActiveUserConfig()` 在桌面销毁时解绑桌面侧监听。其 `configReady` 信号驱动 `WaylandServer::applyCurrentUserConfig`，后者在当前指针变化时发出 `configChanged()`，更新个性化用户 id，并同步光标、输入、壁纸与工作区设置。`WaylandServer::config()` 与 `WaylandServer::globalConfig()` 直接暴露这些配置对象。用户级设置必须走这条路径，而不是 `m_globalConfig`；配置对象由 `DConfigManager` 复用，不由 `WaylandServer` 自己重建。

## 描述文件格式
DConfig 描述文件是 JSON 元数据文件。treeland 中它们位于 `misc/dconfig/`，顶层结构如下：

```json
{
    "magic": "dsg.config.meta",
    "version": "1.0",
    "contents": {
        "keyName": {
            "value": true,
            "serial": 0,
            "flags": ["global"],
            "name": "Display Name",
            "name[zh_CN]": "中文名称",
            "description": "What this setting controls",
            "description[zh_CN]": "此配置项的用途",
            "permissions": "readwrite",
            "visibility": "public"
        }
    }
}
```

顶层规则：

- 描述文件的 `magic` 恒为 `dsg.config.meta`。
- `version` 使用规范的 `major.minor` 格式；treeland 当前为 `1.0`。
- `contents` 是扁平对象，不要嵌套配置项。
- 配置项顺序无语义，便于评审时把相关设置放在一起。

## 选择文件
先确定该设置属于合成器全局还是当前用户所有。全局模式下这取决于语义归属，而不是 treeland 进程的 uid。

按归属与查找方式选择描述文件：

- `org.deepin.dde.treeland.json` → 不随当前用户变化的合成器全局设置；生成 `TreelandConfig` / `treelandconfig.hpp`；通常通过 `WaylandServer::globalConfig()` 访问。
- `org.deepin.dde.treeland.user.json` → 当前用户会话、个性化、工作区、光标、主题等设置；生成 `TreelandUserConfig` / `treelanduserconfig.hpp`；通过 `WaylandServer::config()` 访问，用户切换时从 DConfigManager 缓存中选取。
- `org.deepin.dde.treeland.animation.json` → 当前用户的动画开关；生成 `AnimationConfig` / `animationconfig.hpp`；由 `DConfigManager::animationConfig(userName)` 缓存，通过 `WaylandServer::animationConfig()` 访问并监听 `animationConfigChanged()`。键名直接用 `windowOpen`、`workspaceSwitch` 等效果名，不重复 `AnimationEnabled`。初始化完成前使用默认启用状态，初始化完成或用户切换时重新绑定并刷新效果。
- `org.deepin.dde.treeland.output.json` → 按 output 的设置；生成 `OutputConfig` / `outputconfig.hpp`；当前代码使用 `OutputConfig::createByName("org.deepin.dde.treeland.output", "org.deepin.dde.treeland", "/" + outputName, ...)`。
- `org.deepin.dde.treeland.user.seat.json` → 当前用户的输入/seat 设置；生成 `SeatUserDConfig` / `seatuserconfig.hpp`；当前用户变化时调用 `InputManager::setupSeatUserConfig(userName)`。
- `org.deepin.dde.treeland.app.json` → 按应用的设置；生成 `AppConfig` / `appconfig.hpp`；`WindowConfigStore` 使用 subpath `"/" + appId`。

新增描述文件前，先确认上述某个作用域已经合适。确实需要新文件时，按下面“新增描述文件”一节执行。

默认选择指引：

- 设置在选定任何用户之前就影响合成器基础设施 → 用 `TreelandConfig`。
- 用户期望设置跟随其账户跨登录/切换 → 用 `TreelandUserConfig` 或 `SeatUserDConfig`。
- 设置绑定物理/逻辑 output 名 → 用 `OutputConfig`。
- 设置绑定应用 id → 用 `AppConfig`。
- 不要因为全局模式以 `dde` 运行就把用户偏好塞进 `TreelandConfig`。

## 配置项字段
每个 treeland 配置项都应包含：

- `value`：默认值。使用常规 JSON 类型。该类型决定生成的 C++ 访问器类型。
- `serial`：单调递增整数，新键通常为 `0`。
- `flags`：数组。用户级设置用 `[]` —— 当前所有 treeland 描述文件都是这样。treeland 在全局模式下以 `dde` 用户运行，用户隔离靠描述文件名加 `"/" + userName` subpath，而不是进程 uid，因此这里不需要 `global`。规范对 `global` 的定义是“读写时忽略用户身份，所有用户拿到同一份数据”；只有当某项在所有用户间确实完全相同时才加，且绝不用它替代 subpath。需要禁止 OEM/域管覆盖时用 `["nooverride"]`。
- `name` 与 `name[zh_CN]`：简短的用户可见名称。
- `description` 与 `description[zh_CN]`：运维含义、有效范围、枚举值、单位与副作用。
- `permissions`：`readonly` 或 `readwrite`。
- `visibility`：`private` 或 `public`。

`flags` 选择对照：

| 场景 | `flags` |
| --- | --- |
| 用户级设置（当前全部描述文件都是这种） | `[]`，靠 `"/" + userName` subpath 区分用户 |
| 所有用户共享同一份数据 | `["global"]` |
| 不允许 OEM/域管覆盖 | `["nooverride"]`（可与其他标记组合） |

规范中的兼容性规则：

- `public` 设置属于外部 API。兼容版本中不得删除，也不得改变其含义、`permissions`、`visibility` 或 `flags`。
- `public` 键仅修改 `value`、`name` 或 `description` 不构成兼容性破坏。
- `private` 设置是 treeland 内部实现，可以更自由地演进。
- 当含义、校验或初始化语义变化、需要让旧存储值失效时，递增 `serial`。
- 省略 `serial` 时忽略存储值的 serial 校验。treeland 中保持显式 `serial`。

## 命名
键名使用小驼峰，因为 `dtk_add_config_to_cpp` 会据此生成小驼峰 getter 与 Pascal 风格变更信号：

- 键 `quickSwitchTimeout` → getter `quickSwitchTimeout()`、setter `setQuickSwitchTimeout(...)`、信号 `quickSwitchTimeoutChanged`
- 键 `keyboardNumLock` → getter `keyboardNumLock()`、setter `setKeyboardNumLock(...)`

除非周边协议或库名已在用缩写，否则避免缩写。单位不明显时把单位写进键名，例如 `prelaunchSplashTimeoutMs`。

## 取值与描述
选择在任何运行时服务就绪前都有效的默认值。

对受约束的值，把约束同时写进中英文描述：

- 数值范围与单位，例如毫秒、像素、开尔文，或 `-1.0` 到 `1.0`
- 枚举取值及其含义
- `0`、空字符串或 `false` 是否有特殊行为
- 该设置是仅启动时读取还是实时生效

如果消费方 C++ 路径也需要防御非法运行时值，不要只用文字描述校验规则。当非法 DConfig 值会影响合成器行为时，在消费边界补充或复用校验。

## 生成的 C++ 集成
treeland 在 `src/CMakeLists.txt` 中用 `dtk_add_config_to_cpp(...)` 生成类型化封装。

使用已有描述文件时：

1. 在 `.cpp` 消费方 include 生成头文件，尽量避免放进 public 头文件。需要在 Qt 属性中使用该类型的头文件用 `Q_MOC_INCLUDE`。
2. 使用生成的 getter、setter 与变更信号，不要直接写裸键名字符串。
3. 设置需要实时生效时连接变更信号。
4. 若设置在启动阶段读取，处理 DConfig 异步初始化。
5. 通过 `WaylandServer::config()` 监听当前用户设置时，记住该对象会在用户切换时被替换。连接 `WaylandServer::configChanged()` 并在新的 `TreelandUserConfig` 对象上重新连接各键信号。`WaylandServer::globalConfig()` 是 `CONSTANT`；`WaylandServer::config()` 不是。

新代码直接使用当前 DTK API：

```cpp
    if (config->isInitializeSucceeded()) {
        applyConfig();
    } else {
        connect(config,
                &TreelandConfig::configInitializeSucceed,
                this,
                [this] { applyConfig(); });
    }
```

既有代码可能仍保留为兼容旧 DTK 而写的 `#if *_DCONFIG_FILE_VERSION_MINOR > 0` 分支（调用已废弃的 `isInitializeSucceed()`）。不要把该模式复制到新代码。除非任务本身就要改动那段代码，也不要仅为删除它而产生无关改动。

除非周边代码已有明确回退且行为是有意为之，否则不要从未初始化的配置对象读取、写入或发出协议状态。

## 生成头文件事实
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

## 新增描述文件
优先扩展已有描述文件。确实需要新的 DConfig 描述文件时：

1. 按标准 `dsg.config.meta` 结构新增 `misc/dconfig/org.deepin.dde.treeland.<scope>.json`。
2. 把该文件加入 `misc/dconfig/CMakeLists.txt`，使安装输出落在 `${CMAKE_INSTALL_DATADIR}/dsg/configs/org.deepin.dde.treeland`。配置后用 `build/misc/dconfig/cmake_install.cmake` 确认。
3. 在 `src/CMakeLists.txt` 增加 `dtk_add_config_to_cpp(...)` 块：

   ```cmake
   dtk_add_config_to_cpp(
       NEW_SCOPE_CONFIG
       "${PROJECT_SOURCE_DIR}/misc/dconfig/org.deepin.dde.treeland.<scope>.json"
       OUTPUT_FILE_NAME "<scope>config.hpp"
       CLASS_NAME "<Scope>Config"
   )
   ```

4. 把生成的变量（例如 `${NEW_SCOPE_CONFIG}`）加入 `qt_add_qml_module(libtreeland SOURCES ...)` 列表。这一步是必需的，否则生成头文件不参与 target sources，moc/autogen 看不到该 `QObject` 类。
5. 执行 `cmake --preset default` 重新生成头文件。
6. 检查 `build/src/<scope>config.hpp`（不是源码树），确认：
   - `Q_CLASSINFO("DConfigFileName", "...")`
   - `Q_CLASSINFO("DConfigFileVersion", "1.0")`
   - 版本/键宏
   - `Q_PROPERTY` 类型
   - `create(...)` / `createByName(...)` 签名
   - `configInitializeSucceed` / `configInitializeFailed` 信号
7. 使用生成头文件与异步初始化模式新增 C++ 消费方。

## 作用域与 subpath 用法
Desktop Spec 支持 `subpath` 查找。treeland 中保持既有构造方式：

- 按邻近代码同样的方式使用 `create(...)` / `createByName(...)`
- 保持按应用、按 output 的查找名与 subpath 和既有消费方一致
- 未经确认描述文件安装与回退行为，不要自创新的 subpath 方案

规范中描述文件查找可以沿父 subpath 回退，但带 subpath 的运行时存储缓存不回退。处理按 output、按应用的设置时要记住这一差异。

当前 treeland 的 subpath 模式：

- 当前用户配置：`"/" + currentUserName`
- 用户选定前的初始/默认用户配置：`"/dde"`
- seat 用户配置：`"/" + userName + "/" + seatName`（`DConfigManager::userSeatConfig(...)`，由 `InputManager::setupSeatUserConfig(...)` 进入）
- output 配置：`"/" + outputName`
- 应用配置：`"/" + appId`

QML 与 C++ 的实时监听要注意 `WaylandServer` 上的实际属性：

```cpp
Q_PROPERTY(TreelandUserConfig* config READ config NOTIFY configChanged FINAL)
Q_PROPERTY(TreelandConfig* globalConfig READ globalConfig CONSTANT FINAL)
```

这意味着 `WaylandServer.config` 会在当前用户变化时改变，而 `WaylandServer.globalConfig` 恒定。用户级 QML 绑定应通过 `WaylandServer.config` 绑定，以便 QML 在 `configChanged()` 后重新求值。C++ 中保存 `TreelandUserConfig *` 或连接用户配置信号的代码，必须在 `WaylandServer::configChanged()` 后重新连接，否则可能一直监听旧用户的配置对象、错过新用户的变更。

## override 与可变性
规范允许 override 文件修改 `value` 与 `permissions`，除非该项带 `nooverride`。

对 treeland 的含义：

- 代码必须容忍 `permissions: readonly`；setter 可能失败或被忽略，取决于 DConfig 实现。
- 该状态可用时，UI 或协议写入路径不应展示可写控件。
- 不得被 OEM/域管策略覆盖的不变量使用 `nooverride`。
- 不要用 `global` 标志来区分用户。用户级描述文件保持 `flags: []` 并依赖 `"/" + userName` subpath；`global` 会让存储忽略用户身份。

## 评审清单
评审 DConfig 改动时逐项确认：

1. 描述文件有 `magic: "dsg.config.meta"`、`version: "1.0"`，且 `contents` 扁平。
2. 新键放在与其生命周期、作用域匹配的描述文件中。
3. 每个配置项都有 `value`、显式 `serial`、`flags`、`name`、`name[zh_CN]`、`description`、`description[zh_CN]`、`permissions`、`visibility`。
4. `public` 键没有通过改变含义、`permissions`、`visibility` 或 `flags` 破坏兼容性。
5. 需要让旧存储值失效时递增了 `serial`。
6. 默认值的 JSON 类型与预期的生成 C++ 类型、消费方假设一致。
7. 描述中写明了范围、单位、枚举值与特殊值。
8. 该设置在用户模式与全局模式下行为都正确。
9. 用户级设置走当前用户配置/subpath 路径，没有被错误地只存到 `dde` 进程用户名下。
10. 消费方 include 了正确的生成头文件并使用生成的类型化访问器。
11. 启动阶段读取等待了 DConfig 初始化，或沿用了既有的有意回退模式。
12. 需要实时生效的设置连接了生成的 `*Changed` 信号。
13. 新描述文件已从 `misc/dconfig/CMakeLists.txt` 安装，并通过 `dtk_add_config_to_cpp(...)` 接入。
14. 已构建或至少已 configure，使生成头文件反映新增键。

## 验证
改动涉及生成封装或消费方时，优先用仓库标准命令：

```bash
cmake --preset default
cmake --build --preset default
ctest --test-dir build --output-on-failure
```

仅改描述文件时，另外执行：

```bash
python3 -m json.tool misc/dconfig/<file>.json >/dev/null
```

若构建耗时过高，至少执行 `cmake --preset default` 并检查 `build/src/` 中的生成头文件，确认访问器、setter、信号、创建 helper、class info 与版本宏名称。

## 用 dde-dconfig 调试
`dde-dconfig` 读取的是**已安装**的配置元数据；只改源码树里的 `misc/dconfig/*.json` 不够，先安装项目（描述文件由 `misc/dconfig/CMakeLists.txt` 装到 `${CMAKE_INSTALL_DATADIR}/dsg/configs/org.deepin.dde.treeland`）。

最常用的三条：

```bash
dde-dconfig get   -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k <key>
dde-dconfig set   -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k <key> -v <value>
dde-dconfig watch -a org.deepin.dde.treeland -r org.deepin.dde.treeland.user -s /<username> -k <key>
```

参数映射（`-a`/`-r`/`-s`/`-k`/`-v`/`-u`）、按 output/app/seat 作用域的完整命令、`-m` 元数据查询与取值注意事项见 `references/dde-dconfig.md`。
