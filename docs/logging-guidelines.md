# Logging Guidelines for Treeland Project

This document provides guidelines for logging in the Treeland project, covering both treeland and waylib modules.

## Overview

The Treeland project uses Qt's logging system with customized categories. There are two different approaches for treeland and waylib modules:

- **treeland**: Uses centralized logging categories defined in `treelandlogging.h/.cpp`
- **waylib**: Uses local logging category definitions in each file

## Logging Categories

### Treeland Module Categories

All logging categories for the treeland module are centrally defined in `src/common/treelandlogging.h`:

```cpp
// Core functionality
Q_DECLARE_LOGGING_CATEGORY(treelandCore)
// Input device handling
Q_DECLARE_LOGGING_CATEGORY(treelandInput)
// Output/display management
Q_DECLARE_LOGGING_CATEGORY(treelandOutput)
// Plugin system
Q_DECLARE_LOGGING_CATEGORY(treelandPlugin)
// And more...
```

### Waylib Module Categories

Waylib modules define their categories locally in each file. The category naming follows this pattern:
`waylib.<module>.<component>[.<subcomponent>]`

Example from wcursor.cpp:

```cpp
// Cursor management and movement
Q_LOGGING_CATEGORY(waylibCursor, "waylib.server.cursor", QtInfoMsg)
// Cursor input events
Q_LOGGING_CATEGORY(waylibCursorInput, "waylib.server.cursor.input", QtDebugMsg)
// Cursor gesture events
Q_LOGGING_CATEGORY(waylibCursorGesture, "waylib.server.cursor.gesture", QtDebugMsg)
```

## Log Levels

Use appropriate log levels based on the message importance:

- **Debug (qCDebug)**: Detailed information for debugging
- **Info (qCInfo)**: General operational information
- **Warning (qCWarning)**: Potentially harmful situations
- **Critical (qCCritical)**: Critical errors that need immediate attention

Example:
```cpp
qCDebug(waylibCursor) << "Processing cursor motion at" << position;
qCWarning(waylibCursorInput) << "Invalid button code:" << code;
qCCritical(treelandCore) << "Failed to initialize core component";
```

## Best Practices

1. **Clear and Contextual Messages**
   - Include relevant context in log messages
   ```cpp
   // Good
   qCDebug(waylibCursor) << "Cursor moved from" << oldPos << "to" << newPos;
   
   // Bad
   qCDebug(waylibCursor) << "Cursor moved";
   ```

2. **Appropriate Log Levels**
   - Debug: Development and troubleshooting
   - Info: Normal operations
   - Warning: Unexpected but recoverable
   - Critical: Serious errors

3. **Category Organization**
   - Use specific categories for better filtering
   ```cpp
   // Instead of
   qCDebug(waylibCursor) << "Touch event received";
   
   // Use
   qCDebug(waylibCursorTouch) << "Touch event received";
   ```

4. **State Changes**
   - Log important state changes with before/after values
   ```cpp
   qCDebug(waylibCursor) << "Visibility changed from" << oldVisible << "to" << newVisible;
   ```

5. **Error Handling**
   - Include error details and potential impact
   ```cpp
   qCWarning(waylibCursor) << "Failed to attach device" << device->name() 
                          << "- not a pointing device";
   ```

## Implementation Examples

### Treeland Module

```cpp
#include "treelandlogging.h"

void ExampleClass::processEvent()
{
    qCDebug(treelandCore) << "Processing event:" << eventType;
    if (error) {
        qCWarning(treelandCore) << "Event processing failed:" << errorDetails;
    }
}
```

### Waylib Module

```cpp
// Local category definition
Q_LOGGING_CATEGORY(waylibExample, "waylib.module.example", QtInfoMsg)

void ExampleClass::processEvent()
{
    qCDebug(waylibExample) << "Processing event:" << eventType;
    if (error) {
        qCWarning(waylibExample) << "Event processing failed:" << errorDetails;
    }
}
```

## Debugging Tips

1. **Enable/Disable Categories**
   ```bash
   export QT_LOGGING_RULES="waylib.server.cursor.debug=true"
   export QT_LOGGING_RULES="waylib.*.debug=false"
   ```

2. **Log to File**
   ```bash
   export QT_LOGGING_TO_FILE=1
   export QT_LOGGING_OUTPUT=/path/to/logfile.txt
   ```

## Common Mistakes to Avoid

1. **Don't expose sensitive information**
   ```cpp
   // Bad
   qCDebug(treelandCore) << "User password:" << password;
   ```

2. **Don't mix categories inappropriately**
   ```cpp
   // Bad
   qCDebug(treelandInput) << "Cursor position changed"; // Should use cursor category
   ```

3. **Don't overuse Critical level**
   ```cpp
   // Bad
   qCCritical(waylibCursor) << "Minor position adjustment failed";
   ```

## Contributing

When contributing to the project:

1. Follow the established category naming patterns
2. Use appropriate log levels
3. Provide clear and contextual messages
4. Add new categories through proper channels (centralized for treeland, local for waylib)

For questions about logging, please refer to this guide or contact the maintainers.
