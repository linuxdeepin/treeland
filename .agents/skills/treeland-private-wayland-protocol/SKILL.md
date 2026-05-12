---
name: treeland-private-wayland-protocol
description: Use this skill for treeland private Wayland server-side protocol integration. Trigger whenever the task involves treeland-owned xml, `treeland_*` interfaces, `src/modules/*`, QtWayland scanner output, `QtWaylandServer::*`, `local_qtwayland_server_protocol_treeland(...)`, `globalRemove()`, `destroy_resource(...)`, or wiring a private compositor protocol into `Helper::init`. Do not use it for upstream wlroots/waylib protocol wrappers.
---

# Treeland Private Protocol Integration

## Scope
Use this skill only for treeland private protocols, not for upstream wlroots/waylib protocols.

Use this skill when any of these are true:

- the protocol xml is maintained by treeland
- the interface name is typically `treeland_*`
- the code lives under `src/modules/*`
- CMake uses `local_qtwayland_server_protocol_treeland(...)`
- the generated type is `QtWaylandServer::*`

If the protocol is a standard/ext/unstable/wlr upstream protocol, stop and use the upstream protocol skill instead.

## Goal
Complete these four things correctly:

- generate the server protocol code
- implement the manager and resource classes
- integrate it through `WServerInterface`
- wire it into the startup path from `Helper::init`

## Read First
1. The target module `CMakeLists.txt`
2. `src/modules/tools/CMakeLists.txt`
3. Generated files in the build directory
4. `src/seat/helper.cpp`

Look at generated files first:

- `qwayland-server-<basename>.h/.cpp`
- `wayland-<basename>-server-protocol.h/.c`

Do not guess generated class names, `Resource` callback signatures, `init(...)` overloads, or `interfaceName()`.

From generated `qwayland-server-*.h/.cpp`, confirm at least these facts directly:

- which helper APIs are generated, such as `resourceMap()`, `resource()`, `isGlobalRemoved()`, and `interfaceVersion()`
- what the default behavior of `destroy(...)`, `destroy_resource(...)`, and `bind_resource(...)` is
- that `handle_destroy(...)` only forwards the destroy request to your override of `destroy(Resource *resource)`; actual destruction still usually requires `wl_resource_destroy(resource->handle)`
- that `destroy_func` removes the resource from `m_resource_map` before calling your `destroy_resource(...)`
- that after `destroy_resource(...)` returns, generated code deletes the generated `Resource` wrapper object; this does not mean your own QObject/private object is deleted automatically
- that `globalRemove()` does not call `wl_global_destroy()` immediately; it first calls `wl_global_remove(...)` and then performs deferred destruction

These details affect implementation boundaries:

- `destroy_resource(...)` is responsible for cleaning up business objects and business state, not for deleting the generated `Resource` wrapper
- after `globalRemove()`, the more accurate meaning is "global removal scheduled", not "destroyed synchronously"
- if the manager interface in xml has no destructor request, generated code will not provide `handle_destroy(...)` or `virtual void destroy(Resource *resource)` for that manager; do not assume you can override manager destroy in that case

## Build Path
Prefer the existing helper:

`local_qtwayland_server_protocol_treeland(...)`

Typical outputs:

- `wayland-<basename>-server-protocol.c`
- `qwayland-server-<basename>.h/.cpp`

At minimum, verify:

1. the target source list includes `wayland-<basename>-server-protocol.c`
2. include paths contain `${CMAKE_CURRENT_BINARY_DIR}`
3. `BASENAME`, generated filenames, and C++ type names are consistent

## Recommended Code Structure
- public class: `QObject + WServerInterface`
- private manager: inherits `QtWaylandServer::treeland_xxx_manager_v1`
- private resource: inherits `QtWaylandServer::treeland_xxx_v1`

Keep `QtWaylandServer::*` inheritance in `.cpp` private implementation classes so those types do not leak into public headers.

### Layering Model
The dominant design in current treeland private protocols separates business API from protocol implementation details.

#### Public class
Public headers should expose business-facing APIs, not `QtWaylandServer::*` types.

The public class usually:

- inherits `QObject` and `WServerInterface`
- exposes methods, signals, and small query APIs to business code
- implements `create/destroy/global/interfaceName`
- owns the private manager via `std::unique_ptr`

This public class is what `Helper::init` and other treeland business code attach, connect, and call.

#### Private manager class
Defined in `.cpp`, directly inherits `QtWaylandServer::treeland_xxx_manager_v1`.

It usually owns:

- protocol global lifetime
- manager request dispatch
- initial state sync in `bind_resource(...)`
- creation of child resources or context objects

#### Private resource / context class
Defined in `.cpp`, inherits `QtWaylandServer::treeland_xxx_v1`.

It usually exists for one concrete protocol object and is responsible for:

- handling requests for that resource
- cleanup in `destroy_resource(...)`
- translating protocol events into public class signals, callbacks, or business actions

#### When a public child wrapper is needed
Not every child resource needs its own public header.

If the child object is only an internal detail of the manager, keep it as a private `.cpp` resource class like `appidresolver` or `prelaunch-splash`.

If the child object must be referenced by treeland business code for a longer lifetime, needs a parent, emits signals, provides extra methods, or is owned by other classes, use a public QObject wrapper like `output-manager`, with a private protocol implementation class under it.

### Design Preference
Use this rule:

- create a public class only when business code actually needs a business-facing API
- keep pure protocol details inside `.cpp` private classes

Do not create a public header for every protocol child object just because the protocol contains one. Promote it to a public wrapper only when business code actually needs to know about it.

## Manager Interface Wiring
The public class usually implements:

- `create(WServer *server)`
- `destroy(WServer *server)`
- `global() const`
- `interfaceName() const`

Recommended pattern:

```cpp
void Manager::create(WServer *server)
{
    d->init(*server->handle(), TREELAND_FOO_MANAGER_V1_VERSION);
}

void Manager::destroy(WServer *server)
{
    Q_UNUSED(server);
    d->globalRemove();
}

wl_global *Manager::global() const
{
    return d->globalHandle();
}

QByteArrayView Manager::interfaceName() const
{
    return QtWaylandServer::treeland_foo_manager_v1::interfaceName();
}
```

Rules:

- use a named version constant, not a bare literal
- call `globalRemove()` directly in `destroy()`
- do not wrap it with redundant `if (m_global)` checks
- `create()` often uses `d->init(server->handle()->handle(), InterfaceVersion)`; if the module already uses a named version macro, that is also fine

### Common Manager Variants
Do not think of the manager as only "the thing that creates the global".

In current code, the manager also often:

- sends initial state in `bind_resource(Resource *resource)`
- manually creates child resources with `wl_resource_create(...)` inside request handlers
- uses `resource->version()` for child resource version instead of inventing a new hard-coded version

New private protocols should default to having a manager `destroy` request and implement:

```cpp
void destroy(Resource *resource) override
{
    wl_resource_destroy(resource->handle);
}
```

A few existing managers missing a `destroy` request are historical design issues. Treat them as compatibility exceptions, not as templates for new protocols.

## Resource Destruction
This is the most error-prone part.

### Explicit client destroy
In `destroy(Resource *resource)`, do only:

```cpp
wl_resource_destroy(resource->handle);
```

Do not put real business cleanup here.

### Final cleanup
Prefer `destroy_resource(Resource *resource)` for the actual cleanup path. This is where you:

- update liveness state
- emit disconnect/close-related signals
- clean up callbacks and connections
- `delete this` or `deleteLater()`

Typical pattern:

```cpp
void destroy_resource(Resource *) override
{
    m_alive = false;
    if (onDisconnected)
        onDisconnected();
    delete this;
}
```

If the callback can run inside Qt signal dispatch or another re-entrant path, prefer `deleteLater()`.

Do not assume `destroy_resource(...)` always deletes the object itself. In current code, these patterns also exist:

- a manager resource only clears maps, watch lists, or owner maps associated with `resource->handle`
- QObject lifetime is owned by a parent, an outer wrapper object, or an explicit container

In other words, the core responsibility of `destroy_resource(...)` is "finish this resource lifetime cleanly", not necessarily "delete every related object immediately".

### When to delete the object
If the QObject/private object was created exclusively for this single Wayland resource and has no other owner, `destroy_resource(...)` should end that object lifetime.

Typical signs:

- the constructor directly takes `wl_resource *` or `Resource *`
- one object corresponds to exactly one client resource
- the object's purpose is only to wrap this protocol object
- no parent, container, or manager continues to own it

Typical implementation:

```cpp
void destroy_resource(Resource *) override
{
    delete this;
}
```

If there is re-entrancy risk, use:

```cpp
void destroy_resource(Resource *) override
{
    deleteLater();
}
```

### When not to delete the object
If the QObject is not exclusively owned by one resource and only uses the resource to track client-specific state, `destroy_resource(...)` should only clean up that resource-specific state.

Typical signs:

- the object itself is the manager and follows global or outer interface lifetime
- the object has a clear QObject parent and that parent owns destruction
- the object is still held by static lists, management containers, or business code
- one object manages multiple resources rather than a strict one-to-one mapping

In that case, typically do only:

- remove `resource->handle` from maps/lists/sets
- clear owner, watch, or callback bookkeeping
- update connection state or availability flags

Do not delete a manager or shared object just because one client resource is destroyed if that object still serves other state.

### Decision rule
Ask one question first:

"Was this C++ object created only for this resource, and should it die with this resource?"

- if yes, `destroy_resource(...)` should delete the object
- if no, `destroy_resource(...)` should only clean up the state tied to that resource

### Manager per-client resources
If the manager only has a lightweight per-client proxy resource, `destroy(Resource*)` calling `wl_resource_destroy(...)` is usually enough. Do not pile business state cleanup into it.

### Child resource creation
Many current private protocols use the pattern "manager manually creates a child resource, then hands it to a private wrapper object", for example:

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

In this pattern, verify:

- child resource version follows `resource->version()`
- failures use `wl_client_post_no_memory(...)`
- child destruction removes the object from any static list, map, or owner structure when needed

## Wiring Through `Helper::init`
Eventually this must connect back into `src/seat/helper.cpp`.

Check:

1. whether it is initialized through `m_server->attach<...>()`
2. whether it is stored as a `Helper` member when needed
3. whether business `connect(...)` calls are made immediately after attach
4. whether it is wired into `ShellHandler`, `RootSurfaceContainer`, `QmlEngine`, or other existing objects

Do not interpret "the registration point is in `Helper::init`" too rigidly.

The repository also has cases where a higher-level object encapsulates protocol initialization, for example:

```cpp
m_shellHandler->init(m_server, m_seat);
```

That still counts as a protocol registration entry point. The real criteria are:

- whether startup still flows in from `Helper::init`
- whether a higher-level object cleanly encapsulates initialization for a protocol family
- whether the registration entry point and lifetime ownership are still easy to locate

If a protocol is neither registered directly from `Helper::init` nor initialized through a higher-level object such as `ShellHandler`, it is usually still not fully integrated.

## Existing Samples You Should Not Copy Blindly
- `foreign-toplevel`
- `capture`

This refers to treeland private protocol implementations, not upstream wlroots/waylib protocol wrappers.

These are not wrong implementations, but they do not follow the currently preferred Qt scanner path and instead use a more direct `wayland-scanner` style.

For new private protocols, prefer the `QtWaylandServer::*` plus `local_qtwayland_server_protocol_treeland(...)` path. Treat those modules as historical compatibility samples, not as the default template.

## Reference Starting Points
- `src/modules/app-id-resolver/appidresolver.cpp`
- `src/modules/prelaunch-splash/prelaunchsplash.cpp`
- `src/modules/screensaver/screensaverinterfacev1.cpp`
- `src/modules/tools/CMakeLists.txt`
- `src/seat/helper.cpp`

## Output Requirements
When using this skill for a real task, make these explicit first:

1. where the xml and generated files are
2. how manager `create/destroy/global/interfaceName` should be implemented
3. how `destroy(...)` and `destroy_resource(...)` split responsibilities
4. where the protocol enters the startup path from `Helper::init`
