---
name: treeland-dconfig-configuration
description: Use this skill whenever a task adds, changes, reviews, or debugs treeland DConfig configuration files, generated config wrappers, or runtime DConfig CLI checks. Trigger on `misc/dconfig/*.json`, `dtk_add_config_to_cpp`, generated headers like `treelandconfig.hpp`, `TreelandConfig`, `TreelandUserConfig`, `OutputConfig`, `SeatUserDConfig`, `AppConfig`, `createByName`, `configInitializeSucceed`, DConfig `magic/version/contents`, `serial`, `flags`, `permissions`, `visibility`, overrides, `dde-dconfig`, or moving settings into dconfig.
---

# Treeland DConfig Configuration

## Scope
Use this skill for treeland configuration described by Desktop Spec Group config metadata and consumed through DTK DConfig generated C++ wrappers.

Primary repo locations:

- `misc/dconfig/*.json`
- `misc/dconfig/CMakeLists.txt`
- `src/CMakeLists.txt` `dtk_add_config_to_cpp(...)` calls
- generated build headers under `build/src/`, such as `build/src/treelandconfig.hpp`, `build/src/treelanduserconfig.hpp`, `build/src/outputconfig.hpp`, `build/src/seatuserconfig.hpp`, and `build/src/appconfig.hpp`
- consumers under `src/`

Reference spec:

- Desktop Spec Group "配置文件规范": `https://desktopspec.org/unstable/%E9%85%8D%E7%BD%AE%E6%96%87%E4%BB%B6%E8%A7%84%E8%8C%83.html`.
- The spec is under `unstable`; verify the current online version when exact spec details matter. This skill was written against version 1.0, modified 2023.09.01.

## Goal
Make DConfig changes that satisfy both the Desktop Spec Group metadata format and treeland's generated-wrapper usage:

- the JSON descriptor is valid and compatible
- the right generated C++ class exposes typed getters, setters, and change signals
- asynchronous initialization is handled before reading or writing runtime values
- public settings remain compatible across non-major releases
- install and build integration stays in sync

## Treeland Runtime Modes
Treeland has two runtime modes, and DConfig ownership must be chosen with both in mind:

- User mode: treeland starts after a user logs in, like a normal window manager, and runs as that user.
- Global mode: preferred mode. One treeland process manages all users, user switching does not restart the compositor, and the process runs as the `dde` user.

Because global mode exists, never infer the target user's settings from the treeland process uid. Split settings by ownership:

- Global configuration: process/compositor policy that is the same regardless of the active login user. Access it through `Helper::globalConfig()` and `TreelandConfig`.
- User configuration: settings that follow the currently selected/login user. Access it through `Helper::config()` and `TreelandUserConfig`; `Helper` recreates this object when `UserModel::currentUserNameChanged` fires and emits `configChanged()`.

In `Helper::Helper`, treeland creates:

- `TreelandUserConfig::createByName("org.deepin.dde.treeland.user", "org.deepin.dde.treeland", "/dde")` as a temporary/default user config, then later replaces it with `"/" + currentUserName`.
- `TreelandConfig::create("org.deepin.dde.treeland", QString())` as the compositor-global config.

In `Helper::init`, `updateCurrentUser` rebuilds `m_config` with `"/" + m_userModel->currentUserName()`, emits `configChanged()`, updates personalization user id, and calls `InputManager::setupSeatUserConfig(currentUserName)` for non-`dde` users. Any user-owned setting that must survive user switching belongs on this path, not in `m_globalConfig`.

## Descriptor Format
DConfig descriptor files are JSON metadata files. In treeland they live under `misc/dconfig/` and follow this top-level shape:

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

Top-level rules:

- `magic` for descriptors is always `dsg.config.meta`.
- `version` uses the spec format `major.minor`; treeland currently uses `1.0`.
- `contents` is a flat object. Do not nest configuration items.
- Item order has no semantic meaning; keep nearby related settings together for reviewability.

## Choosing The File
First decide whether the setting is compositor-global or active-user-owned. In global mode this decision is about semantic ownership, not the uid of the treeland process.

Pick the descriptor by ownership and lookup pattern:

- `org.deepin.dde.treeland.json` -> compositor-global settings that should not change when the active user changes; generated as `TreelandConfig` / `treelandconfig.hpp`; usually reached through `Helper::globalConfig()`.
- `org.deepin.dde.treeland.user.json` -> active-user session, personalization, workspace, cursor, theme, and similar settings; generated as `TreelandUserConfig` / `treelanduserconfig.hpp`; reached through `Helper::config()` and recreated on user switch.
- `org.deepin.dde.treeland.output.json` -> per-output settings; generated as `OutputConfig` / `outputconfig.hpp`; current code uses `OutputConfig::createByName("org.deepin.dde.treeland.output", "org.deepin.dde.treeland", "/" + outputName, ...)`.
- `org.deepin.dde.treeland.user.seat.json` -> active-user input/seat settings; generated as `SeatUserDConfig` / `seatuserconfig.hpp`; `InputManager::setupSeatUserConfig(userName)` is called when the active user changes.
- `org.deepin.dde.treeland.app.json` -> per-application settings; generated as `AppConfig` / `appconfig.hpp`; `WindowConfigStore` uses subpath `"/" + appId`.

Before adding a new descriptor file, check whether one of these existing scopes already matches. If a new file is genuinely needed, follow the "Adding A New Descriptor" section below.

Default choice guidance:

- If the setting affects compositor infrastructure before any user is selected, use `TreelandConfig`.
- If the user would expect the setting to follow their account across login/switching, use `TreelandUserConfig` or `SeatUserDConfig`.
- If the setting is tied to a physical/logical output name, use `OutputConfig`.
- If the setting is tied to an application id, use `AppConfig`.
- Do not put user preferences in `TreelandConfig` just because global mode runs as `dde`.

## Item Fields
Every treeland item should include:

- `value`: the default value. Use normal JSON types. This type drives the generated C++ accessor type.
- `serial`: a monotonically increasing integer, normally `0` for new keys.
- `flags`: an array. Use `["global"]` when storage should ignore the process user identity. In treeland global mode this is often still needed for user-owned descriptors, because the running uid can be `dde`; user separation then comes from descriptor name/subpath such as `"/" + currentUserName`, not from uid. Use `["nooverride"]` when OEM/domain override must not be allowed. Use `[]` only when per-process-user storage is genuinely intended.
- `name` and `name[zh_CN]`: short user-visible names.
- `description` and `description[zh_CN]`: operational meaning, valid ranges, enum values, units, and side effects.
- `permissions`: `readonly` or `readwrite`.
- `visibility`: `private` or `public`.

Compatibility rules from the spec:

- `public` settings are external API. Do not delete them or change their meaning, `permissions`, `visibility`, or `flags` in a compatible release.
- Changing `value`, `name`, or `description` for a `public` key does not by itself break compatibility.
- `private` settings are internal to treeland and can evolve more freely.
- Increment `serial` when an old stored value must be invalidated because the meaning, validation, or initialization semantics changed.
- If `serial` is omitted, stored-value serial checks are ignored. In treeland, keep `serial` explicit.

## Naming
Use lower camel case for keys because `dtk_add_config_to_cpp` generates lower camel case getters and Pascal-style change signals from the key:

- key `quickSwitchTimeout` -> getter `quickSwitchTimeout()`, setter `setQuickSwitchTimeout(...)`, signal `quickSwitchTimeoutChanged`
- key `keyboardNumLock` -> getter `keyboardNumLock()`, setter `setKeyboardNumLock(...)`

Avoid abbreviations unless the surrounding protocol or library name uses them. Include units in the key when the unit is not obvious, for example `prelaunchSplashTimeoutMs`.

## Values And Descriptions
Choose defaults that are valid before any runtime service is ready.

For constrained values, put the constraint in both descriptions:

- numeric range and unit, such as milliseconds, pixels, Kelvin, or `-1.0` to `1.0`
- enum values and their meanings
- whether `0`, empty string, or `false` has special behavior
- whether the setting is read at startup only or applied live

Do not encode validation only in prose if the consuming C++ path also needs to guard against invalid runtime values. Add or reuse validation at the consumer boundary when invalid DConfig values can affect compositor behavior.

## Generated C++ Integration
Treeland generates typed wrappers in `src/CMakeLists.txt` using `dtk_add_config_to_cpp(...)`.

When using an existing descriptor:

1. Include the generated header in the `.cpp` consumer, not public headers when avoidable. Use `Q_MOC_INCLUDE` in headers that need the type for Qt properties.
2. Use the generated getter, setter, and change signal instead of raw key strings.
3. Connect change signals when the setting must apply live.
4. If the setting is read during startup, handle DConfig asynchronous initialization.
5. When listening to active-user settings through `Helper::config()`, remember that the object can be replaced on user switch. Connect to `Helper::configChanged()` and reconnect per-key signals on the new `TreelandUserConfig` object. `Helper::globalConfig()` is `CONSTANT`; `Helper::config()` is not.

For new code, use the current DTK API directly:

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

Existing code may still contain `#if *_DCONFIG_FILE_VERSION_MINOR > 0` branches that call deprecated `isInitializeSucceed()` for old DTK compatibility. Do not copy that pattern into new code. Also do not churn existing call sites just to remove it unless the task is already touching that code for a related reason.

Do not read, write, or emit protocol state from an uninitialized config object unless the surrounding code has an established fallback and the behavior is intentional.

## Generated Header Facts
Do not search the source tree for generated config headers. They are produced by CMake/dconfig2cpp into `build/src/`.

After running `cmake --preset default`, inspect the generated header directly, for example:

```bash
sed -n '1,260p' build/src/treelanduserconfig.hpp
```

Current generated headers show these APIs and conventions:

- Header comment records the `dconfig2cpp` command line, output path, class name, source JSON file, JSON file version, and tool version.
- Include guard/class macro prefix is derived from `CLASS_NAME`, for example `TREELANDUSERCONFIG_DCONFIG_FILE_VERSION_MAJOR` and `TREELANDUSERCONFIG_DCONFIG_FILE_VERSION_MINOR`.
- The generated `*_DCONFIG_FILE_VERSION_MINOR` macro reflects generator API compatibility; current generated headers define it as `1` even when `Q_CLASSINFO("DConfigFileVersion", "1.0")` comes from the JSON descriptor. New code does not need old-DTK compatibility branches.
- Each key gets `#define <CLASS>_DCONFIG_FILE_<key>`.
- Each key gets a `Q_PROPERTY` with getter, setter, `*Changed` signal, and `reset*` method.
- Numeric JSON integers are generated as `qlonglong`; JSON floating point numbers are generated as `double`; strings as `QString`; booleans as `bool`.
- The class provides `keyList()`, `isDefaultValue(key)`, per-key `*IsDefaultValue()`, `config()`, `isInitializeSucceeded()`, deprecated `isInitializeSucceed()`, `isInitializeFailed()`, and `isInitializing()`.
- The class emits both the specific `keyChanged()` signal and generic `valueChanged(const QString &key, const QVariant &value)`.
- Setters update cached property storage immediately, then invoke `DConfig::setValue(...)` asynchronously if the underlying `DConfig` object exists.
- Initialization is asynchronous on `DTK_CORE_NAMESPACE::DConfig::globalThread()`. The constructor emits `configInitializeSucceed(DConfig *)` or `configInitializeFailed()`.

Creation helpers generated for each class include:

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

Use `create(...)` when the generated descriptor name is enough. Use `createByName(...)` when the existing treeland code passes an explicit descriptor name and app id, as in current user, output, and seat config paths.

## Adding A New Descriptor
Prefer extending an existing descriptor. If a new DConfig descriptor is genuinely required:

1. Add `misc/dconfig/org.deepin.dde.treeland.<scope>.json` with the standard `dsg.config.meta` shape.
2. Add the file to `misc/dconfig/CMakeLists.txt` so install output includes it under `${CMAKE_INSTALL_DATADIR}/dsg/configs/org.deepin.dde.treeland`. Confirm in `build/misc/dconfig/cmake_install.cmake` after configuring.
3. Add a `dtk_add_config_to_cpp(...)` block in `src/CMakeLists.txt`:

   ```cmake
   dtk_add_config_to_cpp(
       NEW_SCOPE_CONFIG
       "${PROJECT_SOURCE_DIR}/misc/dconfig/org.deepin.dde.treeland.<scope>.json"
       OUTPUT_FILE_NAME "<scope>config.hpp"
       CLASS_NAME "<Scope>Config"
   )
   ```

4. Add the generated variable, for example `${NEW_SCOPE_CONFIG}`, to the `qt_add_qml_module(libtreeland SOURCES ...)` list. This is required so the generated header participates in the target sources and moc/autogen sees the `QObject` class.
5. Run `cmake --preset default` to regenerate the header.
6. Inspect `build/src/<scope>config.hpp`, not the source tree, to confirm:
   - `Q_CLASSINFO("DConfigFileName", "...")`
   - `Q_CLASSINFO("DConfigFileVersion", "1.0")`
   - version/key macros
   - `Q_PROPERTY` types
   - `create(...)` / `createByName(...)` signatures
   - `configInitializeSucceed` / `configInitializeFailed` signals
7. Add C++ consumers using the generated header and the async initialization pattern.

## Scope And Subpath Usage
The Desktop Spec supports `subpath` lookup. In treeland, preserve existing construction patterns:

- use `create(...)` / `createByName(...)` the same way nearby code does
- keep app-specific and output-specific lookup names/subpaths aligned with existing consumers
- do not invent a new subpath scheme without checking descriptor installation and fallback behavior

The spec says descriptor lookup can fall back through parent subpaths, but stored runtime cache with a subpath does not fall back. Keep this difference in mind for per-output and per-app settings.

Current treeland subpath patterns:

- active user config: `"/" + currentUserName`
- initial/default user config before user selection: `"/dde"`
- seat user config: current user name passed to `InputManager::setupSeatUserConfig(...)`
- output config: `"/" + outputName`
- app config: `"/" + appId`

For QML and C++ live listeners, remember the actual properties from `Helper`:

```cpp
Q_PROPERTY(TreelandUserConfig* config READ config NOTIFY configChanged FINAL)
Q_PROPERTY(TreelandConfig* globalConfig READ globalConfig CONSTANT FINAL)
```

This means `Helper.config` can change when the active user changes, while `Helper.globalConfig` is constant. User-owned QML bindings should bind through `Helper.config` so QML reevaluates after `configChanged()`. C++ code that stores a `TreelandUserConfig *` or connects to a user config signal must reconnect after `Helper::configChanged()`, otherwise it may keep listening to the old user's config object and miss changes for the new user.

## Overrides And Mutability
The spec allows override files to change `value` and `permissions` unless the item has `nooverride`.

Implications for treeland:

- Code must tolerate `permissions: readonly`; setters may fail or be ignored depending on the DConfig implementation.
- UI or protocol write paths should avoid presenting a writable control when the key is effectively readonly if that state is available.
- Use `nooverride` for invariants that must not be overridden by OEM/domain policy.
- Do not use the `global` flag as the only user/global ownership signal. In global mode, a user-owned descriptor may still need `global` plus a user-specific subpath so the `dde` process does not collapse all users into one cache.

## Review Checklist
When reviewing a DConfig change, verify:

1. The descriptor has `magic: "dsg.config.meta"`, `version: "1.0"`, and flat `contents`.
2. New keys are in the right descriptor file for their lifetime and scope.
3. Every item has `value`, explicit `serial`, `flags`, `name`, `name[zh_CN]`, `description`, `description[zh_CN]`, `permissions`, and `visibility`.
4. Public keys do not break compatibility through changed meaning, `permissions`, `visibility`, or `flags`.
5. `serial` was incremented if old stored values must be invalidated.
6. The default value's JSON type matches the expected generated C++ type and consumer assumptions.
7. Descriptions document ranges, units, enum values, and special values.
8. The setting behaves correctly in both user mode and global mode.
9. User-owned settings use the active-user config/subpath path and are not accidentally stored only for the `dde` process user.
10. Consumers include the right generated header and use generated typed accessors.
11. Startup reads wait for DConfig initialization or follow an existing intentional fallback pattern.
12. Live settings connect to generated `*Changed` signals when needed.
13. New descriptor files are installed from `misc/dconfig/CMakeLists.txt` and wired through `dtk_add_config_to_cpp(...)`.
14. The change is built or at least configured so generated headers reflect the new keys.

## Verification
Prefer the standard repo commands when the change touches generated wrappers or consumers:

```bash
cmake --preset default
cmake --build --preset default
ctest --test-dir build --output-on-failure
```

For descriptor-only edits, also run:

```bash
python3 -m json.tool misc/dconfig/<file>.json >/dev/null
```

If build time is too high, at minimum run `cmake --preset default` and inspect the generated header in `build/src/` to confirm the accessor, setter, signal, creation helper, class info, and version macro names.

## Debugging With dde-dconfig
Use `dde-dconfig` for manual runtime inspection after the DConfig descriptor files have been installed. The CLI reads installed configuration metadata; editing `misc/dconfig/*.json` in the source tree is not enough. Install the project or otherwise install the descriptors under the DSG config location first, typically from `misc/dconfig/CMakeLists.txt` to:

```text
${CMAKE_INSTALL_DATADIR}/dsg/configs/org.deepin.dde.treeland
```

Treeland option mapping:

- `-a org.deepin.dde.treeland`: app id used by treeland DConfig wrappers.
- `-r <resource>`: descriptor/config id, usually the descriptor filename without `.json`.
- `-s <subpath>`: subpath for active-user, output, or app-scoped configs.
- `-k <key>`: config key.
- `-v <value>`: value for `set`.
- `-u <uid>`: operate on a specific user's config when needed.

Common commands:

```bash
# List installed configurable app/resource/subpath entries.
dde-dconfig list

# List keys for compositor-global TreelandConfig.
dde-dconfig get -a org.deepin.dde.treeland -r org.deepin.dde.treeland

# Read and write a compositor-global key.
dde-dconfig get -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k showOtherUserOption
dde-dconfig set -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k showOtherUserOption -v true

# Read an active-user TreelandUserConfig key.
dde-dconfig get -a org.deepin.dde.treeland -r org.deepin.dde.treeland.user -s /rewine -k windowThemeType

# Read a per-app AppConfig key.
dde-dconfig get -a org.deepin.dde.treeland -r org.deepin.dde.treeland.app -s /deepin-screen-recorder -k enablePrelaunchSplash

# Reset one key, or omit -k to reset all cached values for the config file.
dde-dconfig reset -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k showOtherUserOption

# Watch a key while testing live signal handling.
dde-dconfig watch -a org.deepin.dde.treeland -r org.deepin.dde.treeland.user -s /rewine -k windowThemeType
```

For metadata inspection, use `get` with `-m`:

```bash
dde-dconfig get -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k showOtherUserOption -m permissions
dde-dconfig get -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k showOtherUserOption -m visibility
dde-dconfig get -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k showOtherUserOption -m isDefaultValue
dde-dconfig get -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k showOtherUserOption -m name -l zh_CN
```

Notes:

- Values are parsed by `dde-dconfig`; use JSON-like values (`true`, `false`, numbers, or quoted strings when needed). Avoid typos such as `ture` even if a local tool version appears to coerce or accept it.
- `-r org.deepin.dde.treeland` targets `TreelandConfig`.
- `-r org.deepin.dde.treeland.user -s /<username>` targets the same active-user subpath pattern used by `Helper::config()`.
- `-r org.deepin.dde.treeland.app -s /<appId>` targets `WindowConfigStore` / `AppConfig`.
- `watch` is useful for checking that generated `*Changed` signals and `Helper.config` reconnection behavior are wired correctly.
