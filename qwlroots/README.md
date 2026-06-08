# qwlroots

## Language

[简体中文](./README.zh_CN.md)

## Introduction

qwlroots is a binding of [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots) that provides a Qt style development interface. It aims to simplify the process of calling wlroots API within Qt projects. In qwlroots, each wlroots struct is wrapped as a C++ class with corresponding member functions. Additionally, wl_signals in Wayland are wrapped as Qt signals.

## Supported wlroots versions

As wlroots is still under development and has not yet released its 1.0 version, significant changes may occur at any time. Therefore, the latest code of qwlroots is only guaranteed to be compatible with the latest version of wlroots and the next release version (i.e., the version currently in development); compatibility with historical versions is not guaranteed. Similarly, like wlroots, qwlroots currently does not provide long-term support for interface compatibility.

Currently, qwlroots is compatible with the following versions of wlroots: 0.18.x, 0.19.x

## Supported Environments

* Arch Linux
* Debian
* deepin
* openSUSE
* NixOS

> Other environments have not been tested yet.

## Build

Step 1: Compile and Install wlroots

Follow the instructions in [building wlroots](https://gitlab.freedesktop.org/wlroots/wlroots#building) to compile and install wlroots.

> You can also install wlroots using a system package manager, but pay attention to the version.

Step 2: Install Dependencies

For Debian

````shell
apt install pkg-config cmake qt6-base-private-dev qt6-base-dev-tools wayland-protocols libpixman-1-dev
````

For Arch Linux

````shell
pacman -Syu --noconfirm qt6-base cmake pkgconfig pixman wayland-protocols ninja
````

Step 3: Run the Following Commands

```shell
cmake -B build
cmake --build build
```

We also use the Open Build Service and Garnix to provide automated build and testing services for openSUSE, deepin, and NixOS. See the [openSUSE](https://build.opensuse.org/package/show/home:rewine:vioken/qwlroots), [deepin](https://build.deepin.com/package/show/vioken/qwlroots) and [NixOS](https://garnix.io/docs/caching) for package downloads.

## How to contribute

We assume the you have already had experience with `Qt` and `wlroots` libraries. To better integrate Qt with wlroots, qwlroots follows the Qt specifications in terms of interface style, adheres to wlroots' design philosophy regarding low-level design concepts (e.g., module separation), and adopts Qt's "encapsulation + hierarchy" design philosophy for high-level components that are not directly related to wlroots.

You are free to submit as many code contributions to this project as you like, as long as you follow the design philosophy of qwlroots and the guidelines below.

### Coding Style

* When modifying existing code, adhere to the current coding style.
* There is no absolute right or wrong code style; consider the project as a whole rather than focusing on small parts.

### Code Quality

* Simplicity and understandability come first.
* Add comments in important areas, whether you are modifying or adding code.
* Security > Compatibility > Scalability >= Performance

### Commit Guidelines

* Commit Steps:

  1. First, you need to log in to your GitHub account and fork this project.
  2. Use `git clone` to get the forked project locally
  3. Push new commits to your project with `git push`
  4. Submit your code to the upstream project with the "Pull Request" function on Github.
* Commit Message Guidelines: Follow the conventions used in Qt projects and write in English. Be sure to clearly describe "what the commit does" and "why this approach was taken."
* Each commit should perform a single task; the smaller the changes to the code, the more likely the commit will be accepted. For large code changes, try to split the commit into multiple smaller commits (while satisfying the principles of git commits).
* Please conduct testing and code reviews on your own before submitting your code, and only submit the pull request after confirming that the code is error-free.

## Wrapping Coverage Status

qwlroots provides Qt-style bindings for wlroots types. The following table shows the current wrapping coverage based on wlroots 0.19.0:

| Category        | Wrapped      | Total Available | Coverage        |
| --------------- | ------------ | --------------- | --------------- |
| Types           | 67           | 74              | 90.5%           |
| Util            | 3            | 6               | 50.0%           |
| Render          | 5            | 14              | 35.7%           |
| Interfaces      | 9            | 9               | 100.0%          |
| **Total** | **83** | **103**   | **80.6%** |

### Unwrapped Types (as of wlroots 0.19.0)

The following wlroots types are not yet wrapped in qwlroots:

**Types (20 remaining):**

- alpha_modifier_v1, color_management_v1, ext_data_control_v1, ext_image_capture_source_v1, ext_image_copy_capture_v1, foreign_toplevel_management_v1
- fractional_scale_v1, gamma_control_v1, linux_drm_syncobj_v1, output_swapchain_manager
- presentation_time, security_context_v1, server_decoration, tablet_tool, transient_seat_v1
- xdg_decoration_v1, xdg_dialog_v1, xdg_system_bell_v1, xdg_toplevel_icon_v1

**Util (3 remaining):**

- addon, edges, log, region, transform

**Render (9 remaining):**

- color, dmabuf, drm_format_set, drm_syncobj, gles2, interface, pass, pixman, vulkan

**Interfaces (9 remaining):**

- buffer, ext_image_capture_source_v1, keyboard, output, pointer, switch, tablet_pad, tablet_tool, touch

> Note: This analysis is based on the wlroots submodule at version 0.19.0. To update this information, run `python3 analyze_coverage.py` in the project root.

### How to Update Coverage Information

Use the provided analysis tool to calculate and update wrapping coverage:

```bash
# Run the coverage analysis script
python3 analyze_coverage.py
```

The script works by:

- Scanning `src/` directories for existing qwlroots wrappers
- Scanning `wlroots/include/wlr/` directories for available wlroots types
- Comparing the two sets to calculate coverage percentages
- Generating detailed lists of unwrapped types

The tool automatically handles naming conventions (e.g., `wlr_compositor.h` ↔ `qwcompositor.h`) and outputs coverage statistics for each category (types, util, render, interfaces).

## Documentation for AI Assistants

The `doc/ai/` directory contains specialized documentation designed for AI assistants to understand and work with the qwlroots codebase:

- **[qwlroots-wrapping-patterns-best-practices-en.md](./doc/ai/qwlroots-wrapping-patterns-best-practices-en.md)**: Comprehensive English guide covering all four wrapping patterns (QW_CLASS_OBJECT, QW_CLASS_REINTERPRET_CAST, QW_CLASS_BOX, QW_CLASS_INTERFACE) with detailed examples and step-by-step instructions for adding new wrappers.
- **[qwlroots-wrapping-patterns-best-practices.md](./doc/ai/qwlroots-wrapping-patterns-best-practices.md)**: Chinese version of the wrapping patterns guide with the same comprehensive coverage.

These documents serve as reference materials for AI assistants when:

- Adding new wlroots type wrappers
- Understanding the project's architecture and design patterns
- Following consistent coding practices and naming conventions
- Implementing proper version compatibility handling

The AI documentation includes complete workflows, from updating the wlroots submodule to implementing and testing new wrappers, ensuring consistency and maintainability across the codebase.

## General Requirements

### When Wrapping a New wlroots Type

For detailed guidance on wrapping new wlroots types, please refer to our comprehensive [Wrapping Patterns Best Practices Guide](./doc/ai/qwlroots-wrapping-patterns-best-practices-en.md), which covers all four wrapping patterns and provides step-by-step instructions.

We also provide a demo [example of qw_abc](https://github.com/vioken/qwlroots/tree/master/tests/qwobject_test) in unit test for reference when wrapping a new wlroots type.

* First, check if this type provides signals. If so, wrap it with QW_CLASS_OBJECT; otherwise, wrap it with QW_CLASS_REINTERPRET_CAST.

  * For wlr_abc, using QW_CLASS_OBJECT(abc) will declare a class named qw_abc based on qw_object(QObject).
  * qw_abc provides a handle() function to access the `wlr_abc*` type and supports implicit conversion to the `wlr_abc*` type.
  * qw_abc provides a from() function to acquire `qw_abc*` from `wlr_abc*`. If there is no corresponding qw_abc, a new instance will be created.
  * qw_abc provides a get() function to acquire `qw_abc*` from `wlr_abc*`. If there is no corresponding qw_abc, it returns nullptr.
  * qw_abc will automatically connect to wlr_abc's destroy signal (if it exists) and will be automatically destroyed while emitting the qt signal before_destroy when wlr_abc is destroyed.
  * If qw_abc is explicitly destroyed, the wlr_abc handle will only be destroyed if the owner of the handle is qw_abc itself.
  * qw_abc created by the from() function is not the owner of its handle; only qw_abc created by the create() function will be its handle's owner.
  * QW_CLASS_REINTERPRET_CAST is essentially a cast from the handle. It does not allocate extra memory.
* For types with signals, you should add QW_OBJECT and Q_OBJECT macros to the class, and use QW_SIGNAL to bind signals.

  * The first argument of the QW_SIGNAL it the name of signal, followed by the argument list of the signal itself.
  * In the example of qw_abc, QW_SIGNAL(set_name, char *) forwards wlr_abc's events.set_name signal to qw::notify_set_name.
  * The destroy signal is a special signal that is already handled in the base class qw_object; there is no need for manual binding.
* Use QW_FUNC_STATIC and QW_FUNC_MEMBER to bind functions. Functions should generally be public.

  * The arguments of QW_FUNC_MEMBER are the function name, return type and argument list. The first argument is always the handle; there is no need to declare it.
  * For example, `QW_FUNC_MEMBER(abc, sum, int)` will bind `int wlr_abc_sum(wlr_abc *)` to `int qw_abc::sum()`.
  * QW_FUNC_STATIC differs from QW_FUNC_MEMBER in that the first argument is not the handle; it will declare a static function of class qw_abc.
  * If the name of the function forwarded by QW_FUNC_STATIC contains create or get and the return type is the handle type, you should use the qw type as the return value type. For example, `QW_FUNC_STATIC(abc, create, qw_abc*, wl_display*)` will expand to `static qw_abc *qw_abc::create(wl_display *)`, which wraps the function `wlr_abc *wlr_abc_create(wl_display *)`. The return value will be automatically wrapped as a qw type.
  * For classes that provide a destroy method, the destructor should generally be protected to prevent non-handle owners from calling it to destroy the handle. The destructor will be called automatically when the qw type is deleted (for cast types) or when the destructor of the qw type is invoked (for qw_object).
