# Treeland 项目日志指南

本文档提供了 Treeland 项目的日志指南，涵盖了 treeland 和 waylib 模块。

## 概述

Treeland 项目使用 Qt 的日志系统并自定义了日志类别。treeland 和 waylib 模块采用了两种不同的方式：

- **treeland**：使用在 `treelandlogging.h/.cpp` 中集中定义的日志类别
- **waylib**：在各个文件中本地定义日志类别

## 日志类别

### Treeland 模块类别

所有 treeland 模块的日志类别都集中定义在 `src/common/treelandlogging.h` 中：

```cpp
// 核心功能
Q_DECLARE_LOGGING_CATEGORY(treelandCore)
// 输入设备处理
Q_DECLARE_LOGGING_CATEGORY(treelandInput)
// 输出/显示管理
Q_DECLARE_LOGGING_CATEGORY(treelandOutput)
// 插件系统
Q_DECLARE_LOGGING_CATEGORY(treelandPlugin)
// 更多...
```

### Waylib 模块类别

Waylib 模块在各自的文件中定义类别。类别命名遵循以下模式：
`waylib.<模块>.<组件>[.<子组件>]`

来自 wcursor.cpp 的示例：

```cpp
// 光标管理和移动
Q_LOGGING_CATEGORY(waylibCursor, "waylib.server.cursor", QtInfoMsg)
// 光标输入事件
Q_LOGGING_CATEGORY(waylibCursorInput, "waylib.server.cursor.input", QtDebugMsg)
// 光标手势事件
Q_LOGGING_CATEGORY(waylibCursorGesture, "waylib.server.cursor.gesture", QtDebugMsg)
```

## 日志级别

根据消息的重要性使用适当的日志级别：

- **Debug (qCDebug)**：调试信息，用于开发调试
- **Info (qCInfo)**：一般操作信息
- **Warning (qCWarning)**：潜在的有害情况
- **Critical (qCCritical)**：需要立即关注的严重错误

示例：
```cpp
qCDebug(waylibCursor) << "正在处理光标移动，位置：" << position;
qCWarning(waylibCursorInput) << "无效的按钮代码：" << code;
qCCritical(treelandCore) << "核心组件初始化失败";
```

## 最佳实践

1. **清晰和上下文相关的消息**
   - 在日志消息中包含相关上下文
   ```cpp
   // 好的做法
   qCDebug(waylibCursor) << "光标从" << oldPos << "移动到" << newPos;
   
   // 不好的做法
   qCDebug(waylibCursor) << "光标移动";
   ```

2. **适当的日志级别**
   - Debug：开发和故障排除
   - Info：正常操作
   - Warning：意外但可恢复的情况
   - Critical：严重错误

3. **类别组织**
   - 使用特定类别以便更好地过滤
   ```cpp
   // 不要这样
   qCDebug(waylibCursor) << "收到触摸事件";
   
   // 应该这样
   qCDebug(waylibCursorTouch) << "收到触摸事件";
   ```

4. **状态变化**
   - 记录重要的状态变化，包含前后的值
   ```cpp
   qCDebug(waylibCursor) << "可见性从" << oldVisible << "变更为" << newVisible;
   ```

5. **错误处理**
   - 包含错误详情和潜在影响
   ```cpp
   qCWarning(waylibCursor) << "设备" << device->name() 
                          << "附加失败 - 不是指针设备";
   ```

## 实现示例

### Treeland 模块

```cpp
#include "treelandlogging.h"

void ExampleClass::processEvent()
{
    qCDebug(treelandCore) << "正在处理事件：" << eventType;
    if (error) {
        qCWarning(treelandCore) << "事件处理失败：" << errorDetails;
    }
}
```

### Waylib 模块

```cpp
// 本地类别定义
Q_LOGGING_CATEGORY(waylibExample, "waylib.module.example", QtInfoMsg)

void ExampleClass::processEvent()
{
    qCDebug(waylibExample) << "正在处理事件：" << eventType;
    if (error) {
        qCWarning(waylibExample) << "事件处理失败：" << errorDetails;
    }
}
```

## 调试技巧

1. **启用/禁用类别**
   ```bash
   export QT_LOGGING_RULES="waylib.server.cursor.debug=true"
   export QT_LOGGING_RULES="waylib.*.debug=false"
   ```

2. **日志输出到文件**
   ```bash
   export QT_LOGGING_TO_FILE=1
   export QT_LOGGING_OUTPUT=/path/to/logfile.txt
   ```

## 常见错误避免

1. **不要暴露敏感信息**
   ```cpp
   // 错误
   qCDebug(treelandCore) << "用户密码：" << password;
   ```

2. **不要混用类别**
   ```cpp
   // 错误
   qCDebug(treelandInput) << "光标位置改变"; // 应该使用光标类别
   ```

3. **不要过度使用 Critical 级别**
   ```cpp
   // 错误
   qCCritical(waylibCursor) << "轻微的位置调整失败";
   ```

## 贡献指南

在为项目贡献代码时：

1. 遵循既定的类别命名模式
2. 使用适当的日志级别
3. 提供清晰和上下文相关的消息
4. 通过正确的方式添加新类别（treeland 集中定义，waylib 本地定义）

如有关于日志的问题，请参考本指南或联系维护者。
