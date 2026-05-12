---
name: logging-guidelines
description: Use this skill whenever a task adds, changes, reviews, or removes logging in treeland, waylib, or qwlroots code. Trigger on logging categories, `Q_LOGGING_CATEGORY`, `Q_DECLARE_LOGGING_CATEGORY`, `qCDebug`, `qCInfo`, `qCWarning`, `qCCritical`, log level selection, category naming, or requests to clean up log messages. For treeland code, prefer centralized categories in `src/common/treelandlogging.h/.cpp`. For waylib code, prefer per-file local categories named `waylib.<module>.<component>[.<subcomponent>]`.
---

# Logging Guidelines

## Goal
Keep logging changes consistent with this repository's existing split:

- `treeland`: centralized categories in `src/common/treelandlogging.h/.cpp`
- `waylib`: local per-file categories

Use this skill for both implementation work and review work.

## Routing
Choose the logging path by module ownership, not by personal preference.

### Treeland code
Use centralized categories declared in:

- `src/common/treelandlogging.h`
- `src/common/treelandlogging.cpp`

Do not introduce ad hoc local `Q_LOGGING_CATEGORY` definitions in treeland code unless the repository already has an explicit exception in that area.

### Waylib code
Define categories locally in the file that uses them.

Use the naming pattern:

`waylib.<module>.<component>[.<subcomponent>]`

Example:

```cpp
Q_LOGGING_CATEGORY(waylibCursor, "waylib.server.cursor", QtInfoMsg)
Q_LOGGING_CATEGORY(waylibCursorInput, "waylib.server.cursor.input", QtDebugMsg)
```

## Log Levels
Choose the lowest level that still matches the operational importance.

- `qCDebug`: detailed debugging and troubleshooting
- `qCInfo`: normal operational information
- `qCWarning`: unexpected but recoverable situations
- `qCCritical`: serious failures requiring immediate attention

Do not upgrade routine noise to warning or critical just to make it visible.

## Message Quality
Write messages that are specific and filterable.

Prefer:

```cpp
qCDebug(waylibCursor) << "Cursor moved from" << oldPos << "to" << newPos;
```

Avoid:

```cpp
qCDebug(waylibCursor) << "Cursor moved";
```

When useful, include:

- the object or subsystem involved
- before/after state
- identifiers or names
- the impact of an error

## Category Organization
Use the most specific category that matches the event.

Prefer a narrower category for a sub-area when the file already separates them. Do not log unrelated events through a broad category if a better one exists.

## Error Logging
When logging failures, include enough context to debug the issue without reading surrounding code.

Prefer:

```cpp
qCWarning(waylibCursor) << "Failed to attach device" << device->name()
                        << "- not a pointing device";
```

Avoid vague failure messages with no object, cause, or impact.

## Sensitive Data
Do not log secrets, passwords, tokens, or other sensitive user data.

## Review Checklist
When reviewing a logging change, verify:

1. the category follows the correct treeland or waylib pattern
2. the chosen log level matches severity
3. the message contains enough context
4. the change does not leak sensitive information
5. the change does not introduce unrelated category churn

## Repository References
- `src/common/treelandlogging.h`
- `src/common/treelandlogging.cpp`
