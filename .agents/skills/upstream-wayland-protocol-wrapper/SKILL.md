---
name: upstream-wayland-protocol-wrapper
description: Use this skill for integrating or wrapping upstream Wayland protocols in wlroots, waylib, and treeland. Trigger whenever the task involves standard/ext/unstable/wlr protocols, wlroots-implemented protocols, waylib `WServerInterface` wrappers, `m_server->attach<W...>()`, native handle lifetime, or wiring an upstream protocol into the startup path from `Helper::init`. Do not use it for treeland private xml or `QtWaylandServer::*` private protocol modules.
---

# Upstream Protocol Wrapping And Integration

## Scope
Use this skill only for upstream protocols, not for treeland private protocols.

Use this skill when any of these are true:

- the protocol is a standard/ext/unstable/wlr protocol
- wlroots already implements the native protocol
- the target is a waylib `WServerInterface` wrapper
- treeland consumes it through `m_server->attach<W...>()`

If the task is about treeland-owned xml, `QtWaylandServer::*`, or a new private module under `src/modules/*`, stop and use the treeland private protocol skill instead.

## Goal
- confirm whether waylib already has a wrapper
- add a `WServerInterface` wrapper when needed
- make native handle ownership and destruction explicit
- wire the result into the startup path from `Helper::init`

## Read First
1. `waylib/src/server/protocols`
2. `waylib/src/server/kernel`
3. `qwlroots/src`
4. `src/seat/helper.cpp`

Search for an existing wrapper first. Do not assume treeland should reimplement a manager locally.

## First Principle
For upstream protocols, prefer reusing wlroots native implementations and let waylib provide the Qt/C++ wrapper. Treeland should usually only attach the wrapper and connect business behavior.

The preferred treeland-side shape is usually:

```cpp
auto *iface = m_server->attach<W...>();
connect(iface, ...);
```

## Minimum Wrapper Requirements
Provide a `WServerInterface` subclass that implements at least:

- `interfaceName() const`
- `create(WServer *server)`
- `destroy(WServer *server)`
- `global() const`

Typical creation:

```cpp
void WFoo::create(WServer *server)
{
    m_handle = qw_foo_manager_v1::create(*server->handle(), FOO_MANAGER_V1_VERSION);
}
```

Typical `global()`:

```cpp
wl_global *WFoo::global() const
{
    return nativeInterface<qw_foo_manager_v1>()->handle()->global;
}
```

## How To Judge Destruction
Do not mechanically copy `globalRemove()` from treeland private protocols.

Decide destruction based on native ownership:

- if `qw_*::create(...)` returns an object owned by the wrapper or `WObject`, `destroy()` may stay empty
- if wlroots or the native wrapper exposes an explicit destroy/reset/listener cleanup API, call it from `destroy()`
- if destruction happens naturally through QObject/wrapper destruction, do not free it twice

How to determine that:

1. read nearby `W...` wrappers in the same directory
2. read whether the corresponding `qw_*` type exposes explicit `destroy()` or similar APIs
3. read whether wlroots native objects expose a destroy API

Core rule: destruction must match native handle ownership.

## Wiring Through `Helper::init`
This still needs to connect back into `src/seat/helper.cpp`:

1. `m_server->attach<W...>()`
2. store the object as a member or local when appropriate
3. immediately add business `connect(...)` calls
4. wire it into `ShellHandler`, surface/output management, or the QML layer

Do not interpret "must be attached directly inside `Helper::init`" as the only valid form.

Some protocols may be initialized through a higher-level object, for example:

```cpp
m_shellHandler->init(m_server, m_seat);
```

That still counts as valid integration. The real checks are:

- whether startup still flows in from `Helper::init`
- whether a higher-level object cleanly encapsulates initialization for a shell or protocol family
- whether the registration entry point and lifetime ownership remain clear

If a wrapper is neither attached directly from `Helper::init` nor initialized through a higher-level object, the protocol is still not effectively available to treeland.

## Reference Starting Points
- `waylib/src/server/protocols/wforeigntoplevelv1.cpp`
- `waylib/src/server/protocols/wextforeigntoplevellistv1.cpp`
- `waylib/src/server/kernel/wserver.h`
- `src/seat/helper.cpp`

## Output Requirements
When using this skill for a real task, make these explicit first:

1. whether wlroots already implements the protocol
2. whether waylib already has a `W...` wrapper
3. if not, who owns the native handle and who is responsible for destruction
4. where treeland enters the startup path from `Helper::init` and where business `connect(...)` calls should live
