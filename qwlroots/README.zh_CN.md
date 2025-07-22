# qwlroots

## 选择语言

[English](./README.md)

## 简介

qwlroots 是一个 [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots) 的绑定，提供 Qt 风格的开发接口。其设计目标是简化在Qt项目中调用 wlroots API 的方式。在 qwlroots 中，每一个 wlroots struct 及其配套的函数被封装为一个 c++ class，同时，wayland 中的 wl_singal 被封装为 Qt 中的信号。

## 受支持的 wlroots 版本

由于 wlroots 还处于开发中，至今未发布 1.0 版本，可能会随时进行比较大的改动。因此，原则上 qwlroots 的最新代码仅保证兼容 wlroots 的最新版本和下一个即将发布的版本（即目前还处于开发中的版本），对于历史版本一概不保证兼容。同 wlroots 一样，qwlroots 目前也不提供长期的接口兼容性保障。

当前 qwlroots 兼容的wlroots版本是： 0.18.x, 0.19.x。

## 支持的环境

* Arch Linux
* Debian
* deepin
* openSUSE
* NixOS

> 其它环境未测试

## 构建

步骤一：编译安装 [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots#building)

> 亦可使用系统包管理器安装 wlroots （需注意版本）

步骤二：安装依赖

Debian

````shell
apt install pkg-config cmake qt6-base-private-dev qt6-base-dev-tools wayland-protocols libpixman-1-dev
````

Arch Linux

````shell
pacman -Syu --noconfirm qt6-base cmake pkgconfig pixman wayland-protocols ninja
````

步骤三：运行以下命令

```shell
cmake -B build
cmake --build build
```

此外我们使用 open build service 和 garnix 提供的自动化构建测试, 同时提供了 [openSUSE](https://build.opensuse.org/package/show/home:rewine:vioken/qwlroots), [deepin](https://build.deepin.com/package/show/vioken/qwlroots), [NixOS](https://garnix.io/docs/caching) 的软件包下载。

## 贡献指南

此项目默认您已经拥有 `Qt`和 `wlroots`库的使用经验，为了能更好的融合 Qt 和 wlroots，qwlroots 在接口风格上遵守Qt的相关规范，在底层的设计理念（如模块划分）上遵守 wlroots 的设计思想，上层与 wlroots 无直接关联的部分则遵守 Qt 的“封装+分层”的设计思想。

在遵守 qwlroots 的设计理念和以下几类要求的前提下，您可以自由地向此项目提交任意的代码贡献。

### 编码风格

* qwlroots 代码高度模板化，建议参考当前的代码风格
* 代码风格没有绝对的对与错，请顾全大局，而勿拘于小结

### 代码质量

* 代码以简单明了、容易理解为优先
* 无论是改动或新增代码，均需要在关键节点增加注释
* 安全性 > 兼容性 > 可扩展性 >= 性能

### 提交规范

* 提交步骤：

  1. 首先需要登录您的 GitHub 帐号，fork 此项目
  2. 在本地使用 `git clone`拉取fork之后的项目
  3. 将新的提交使用 `git push`推送到您的项目中
  4. 在 GitHub 上使用 Pull Requese 功能提交您的代码到上游项目
* commit 信息规范：与 Qt 项目保持一致，统一使用英文。一定要准确描述出这个提交“做了什么”、“为什么要这样做”
* 一个 commit 只做一件事情，代码量改动越小的 commit 越容易被接受。对于较大的代码改动，尽量拆分为多个代码提交（满足 git commit 的原则为前提）
* 在提交代码之前请自行进行测试和代码 Review，在确认代码无误后再提交 PR

## 封装覆盖率状态

qwlroots 为 wlroots 类型提供 Qt 风格的绑定。下表显示了基于 wlroots 0.19.0 的当前封装覆盖率：

| 分类           | 已封装       | 总可用数      | 覆盖率          |
| -------------- | ------------ | ------------- | --------------- |
| Types          | 66           | 74            | 89.2%           |
| Util           | 3            | 6             | 50.0%           |
| Render         | 5            | 14            | 35.7%           |
| Interfaces     | 9            | 9             | 100.0%          |
| **总计** | **83** | **103** | **80.6%** |

### 未封装的类型（截至 wlroots 0.19.0）

以下 wlroots 类型尚未在 qwlroots 中封装：

**Types（剩余 20 个）：**

- alpha_modifier_v1, color_management_v1, ext_data_control_v1, ext_foreign_toplevel_list_v1
- ext_image_capture_source_v1, ext_image_copy_capture_v1, foreign_toplevel_management_v1
- fractional_scale_v1, gamma_control_v1, linux_drm_syncobj_v1, output_swapchain_manager
- presentation_time, security_context_v1, server_decoration, tablet_tool, transient_seat_v1
- xdg_decoration_v1, xdg_dialog_v1, xdg_system_bell_v1, xdg_toplevel_icon_v1

**Util（剩余 3 个）：**

- addon, edges, log, region, transform

**Render（剩余 9 个）：**

- color, dmabuf, drm_format_set, drm_syncobj, gles2, interface, pass, pixman, vulkan

**Interfaces（剩余 9 个）：**

- buffer, ext_image_capture_source_v1, keyboard, output, pointer, switch, tablet_pad, tablet_tool, touch

> 注意：此分析基于版本 0.19.0 的 wlroots 子模块。要更新此信息，请在项目根目录运行 `python3 analyze_coverage.py`。

### 如何更新覆盖率信息

使用提供的分析工具来计算和更新封装覆盖率：

```bash
# 运行覆盖率分析脚本
python3 analyze_coverage.py
```

脚本工作原理：

- 扫描 `src/` 目录中现有的 qwlroots 封装
- 扫描 `wlroots/include/wlr/` 目录中可用的 wlroots 类型
- 比较两个集合来计算覆盖率百分比
- 生成详细的未封装类型列表

工具自动处理命名约定（如 `wlr_compositor.h` ↔ `qwcompositor.h`）并输出各个分类（types、util、render、interfaces）的覆盖率统计。

## AI 助手文档

`doc/ai/` 目录包含专为 AI 助手设计的文档，用于理解和使用 qwlroots 代码库：

- **[qwlroots-wrapping-patterns-best-practices.md](./doc/ai/qwlroots-wrapping-patterns-best-practices.md)**: 中文版综合指南，涵盖所有四种封装模式（QW_CLASS_OBJECT、QW_CLASS_REINTERPRET_CAST、QW_CLASS_BOX、QW_CLASS_INTERFACE），包含详细示例和添加新封装的分步说明。
- **[qwlroots-wrapping-patterns-best-practices-en.md](./doc/ai/qwlroots-wrapping-patterns-best-practices-en.md)**: 英文版封装模式指南，内容与中文版相同。

这些文档作为 AI 助手的参考资料，用于：

- 添加新的 wlroots 类型封装
- 理解项目的架构和设计模式
- 遵循一致的编码实践和命名约定
- 实现适当的版本兼容性处理

AI 文档包含完整的工作流程，从更新 wlroots 子模块到实现和测试新封装，确保代码库的一致性和可维护性。

## 一些或许是通用的要求

### 封装新的 wlroots 类型时

关于封装新的 wlroots 类型的详细指导，请参考我们的综合性 [封装模式最佳实践指南](./doc/ai/qwlroots-wrapping-patterns-best-practices.md)，该指南涵盖了所有四种封装模式并提供了分步说明。

在单元测试中提供了一个 [qw_abc 的示例](https://github.com/vioken/qwlroots/tree/master/tests/qwobject_test), 可以作为使用 qwlroots 的参考。

* 首先检查这种类型是否提供信号，有信号的类型使用 QW_CLASS_OBJECT 封装，无信号类型使用 QW_CLASS_REINTERPRET_CAST 封装
  * wlr_abc, 使用 QW_CLASS_OBJECT(abc) 会声明基于 qw_object（QObject）的 qw_abc 类
  * qw_abc 提供 handle() 函数访问 wlr_abc 类型，同时支持到 wlr_abc* 的隐式转换
  * 提供 from 函数从 wlr_abc* 获取 qw_abc，如果没有对应 qw_abc 则会创建一个
  * 提供 get 函数从 wlr_abc* 获取 qw_abc，如果没有对应 qw_abc 则返回 nullptr
  * qw_abc 会自动连接 wlr_abc 的 destroy 信号（如果有），自动析构，并在销毁前发出 qt 信号 before_destroy
  * 如果主动析构 qw_abc，只有是 handle 的 owner 才会销毁 wlr_abc 类型
  * from 函数创建的 qw_abc 都不是 handle 的 owner，只有下文提到的 create 创建出的是 owner
  * QW_CLASS_REINTERPRET_CAST 本质是对 handle 的强转，不分配额外内存
* 对于有信号类型，需要添加 QW_OBJECT 和 Q_OBJECT 宏，使用 QW_SIGNAL 进行信号绑定
  * QW_SIGNAL 第一个参数是名字，之后是信号参数
  * 在 qw_abc 例子中 QW_SIGNAL(set_name, char*) 将 wlr_abc 的 events.set_name 信号转为  qw::notify_set_name
  * destroy 是比较特殊的信号，在 qw_object 基类中已经处理，无需手动绑定
* 对成员函数，使用 QW_FUNC_STATIC 和 QW_FUNC_MEMBER 进行绑定，它们通常应该是 public 的
  * QW_FUNC_MEMBER 参数为函数名，返回值类型，和函数参数。其中函数参数第一个固定传入 handle, 不用填。
  * 以 `QW_FUNC_MEMBER(abc, sum, int)` 为例，将 `int wlr_abc_sum(wlr_abc *)` 绑定到成员函数 `int qw_abc::sum()`
  * QW_FUNC_STATIC 不同的地方是函数参数第一个传入的不是 handle，会为 qw_abc 添加静态成员函数。
  * 需要使用 QW_FUNC_STATIC 的函数如果名字有 create/get ，返回值是 handle 类型，应该直接填 qw 类型，比如 `QW_FUNC_STATIC(abc, create, qw_abc*, wl_display*)`, 将 `wlr_abc *wlr_abc_create(wl_display *)` 封装为 `static qw_abc *qw_abc::create(wl_display *)` 返回值自动转换
  * 对于提供 destroy 方法的类，一般情况标记为 protected，防止非 handle 的 owner 调用此函数销毁 wlr_abc 类型。该方法会在调用 delete（强转类型）或者析构函数（qw_object类型）时被自动调用。
