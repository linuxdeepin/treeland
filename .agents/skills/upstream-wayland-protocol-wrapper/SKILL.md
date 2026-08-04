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
3. the matching wlroots public header and implementation under `3rdparty/wlroots`
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
    m_handle = wlr_foo_manager_v1_create(server->handle(), FOO_MANAGER_V1_VERSION);
    if (!m_handle)
        return;

    m_destroyListener.notify = handle_destroy;
    wl_signal_add(&m_handle->events.destroy, &m_destroyListener);
}
```

Typical `global()`:

```cpp
wl_global *WFoo::global() const
{
    return m_handle ? m_handle->global : nullptr;
}
```

Do not introduce a generic QObject wrapper for `wlr_*` structs. Store the exact
native pointer on the owning or observing Waylib object and embed one named
`wl_listener` member for each native event the object consumes. Listener
callbacks should recover their owner with `wl_container_of` and immediately
cross into the relevant Qt/domain-object behavior.

## How To Judge Destruction
Do not mechanically copy `globalRemove()` from treeland private protocols.

Decide destruction based on native ownership:

- if `wlr_*_create(...)` creates a display-owned global with no public destroy
  function, `destroy()` may only unlink listeners and clear the pointer
- if wlroots exposes an explicit `wlr_*_destroy()`/finish API, call it exactly
  once from the native owner's teardown path
- for non-owning resources, observe their destroy signal, unlink all remaining
  listeners, clear the pointer, and invalidate/delete the Waylib domain object

How to determine that:

1. read nearby `W...` wrappers in the same directory
2. read the matching wlroots public header for ownership and destroy/finish APIs
3. inspect the wlroots implementation when ownership or display teardown order
   is not explicit in the header

Core rule: destruction must match native handle ownership, and both owner-first
and resource-first teardown must unlink every live `wl_listener` exactly once.

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
