## 选择语言

[English](./README.md)

## 简介

qwlroots 是一个 [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots) 的绑定，提供 Qt 风格的开发接口。其设计目标是使用 Qt 简化调用 wlroots API 的方式，服务于希望在使用 Qt 编写的项目中调用 wlroots 的需求。在 qwlroots 中，每一个 wlroots struct 及其配套的函数被封装为一个 c++ class，同时，wayland 中的 wl_singal 被封装为 Qt 中的信号。

## 受支持的 wlroots 版本

由于 wlroots 还处于开发中，至今未发布 1.0 版本，可能会随时进行比较大的改动。因此，qwlroots 的最新代码仅保证兼容 wlroots 的最新版本和下一个即将发布的版本（即目前还处于开发中的版本），对于历史版本一概不保证兼容。同时，同 wlroots，qwlroots 目前也不提供长期的接口兼容性保障。

## 支持的环境

* Arch Linux
* Debian
* NixOS

> 其它环境未测试

## 构建

步骤一：编译安装 [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots#building)

> 亦可在系统仓库中安装 wlroots （需注意版本）


步骤二：安装依赖

Debian
````
# apt install pkg-config cmake qt6-base-private-dev qt6-base-dev-tools wayland-protocols libpixman-1-dev
````

Arch Linux

````
# pacman -Syu --noconfirm qt6-base cmake pkgconfig pixman wayland-protocols ninja
````

步骤三：运行以下命令

```bash
cmake -B build
cmake --build build
```

## 贡献指南

此项目默认您已经拥有`Qt`和`wlroots`库的使用经验，为了能更好的融合 Qt 和 wlroots，qwlroots 在接口风格上遵守Qt的相关规范，在底层的设计理念（如模块划分）上遵守 wlroots 的设计思想，上层与 wlroots 无直接关联的部分则遵守 Qt 的“封装+分层”的设计思想。

在遵守 qwlroots 的设计理念和以下几类要求的前提下，您可以自由地向此项目提交任意的代码贡献。

### 编码风格

* 在修改已有代码时，需遵守当前的代码风格
* 新增代码：与 wlroots 密切相关的部分遵守 wlroots 的代码风格，如使用 QWSignalConnector 链接 wl_signal 时，相应的槽函数使用“下划线命名法”；其它部分（特别是公共 API）遵守 Qt 的代码风格（https://wiki.qt.io/Qt_Coding_Style 此链接仅为参考，实际请以Qt源码为准）
* 代码风格没有绝对的对与错，请顾全大局，而勿拘于小结

### 代码质量

* 代码以简单明了、容易理解为优先
* 无论是改动或新增代码，均需要在关键节点增加注释
* 安全性 > 兼容性 > 可扩展性 >= 性能

### 提交规范

* 提交步骤：
    1. 首先需要登录您的 GitHub 帐号，fork 此项目
    2. 在本地使用`git clone`拉取fork之后的项目
    3. 将新的提交使用`git push`推送到您的项目中
    4. 在 GitHub 上使用 Pull Requese 功能提交您的代码到上游项目

* commit 信息规范：与 Qt 项目保持一致，统一使用英文。一定要准确描述出这个提交“做了什么”、“为什么要这样做”
* 一个 commit 只做一件事情，代码量改动越小的 commit 越容易被接受。对于较大的代码改动，尽量拆分为多个代码提交（满足 git commit 的原则为前提）
* 在提交代码之前请自行进行测试和代码 Review，在确认代码无误后再提交 PR

## 一些或许是通用的要求

### 封装新的 wlroots 类型时

* 先确定此对象的生命周期管理机制，是需要创建方手动销毁，还是会跟随其它对象销毁。wlroots 中有些类型会跟随 wl_display 销毁
* 对于会跟随其它对象自动化销毁的类型，在封装的 class 中需要将析构函数标记为 private
* 对于提供了 destroy 事件的 wlroots 类型，在封装的 class 中需要连接此信号，在收到信号后将自己销毁
* 对于无需手动销毁，或未提供对应的 wlr_xxx_create_xxx 函数的类型，封装的 class 中无需提供 public 构造函数，统一提供一个或多个 `from` 静态函数实现对象的创建
* 对于提供了信号的 wlroots 类型，封装的 class 需要继承于 QObject（且往往需要同时继承 QWObject）
* 封装的 class 为了方便维护二进制兼容性，一律使用 Qt 的 D-Pointer 模式，当需要成员变量时，需要继承 QWObject 实现 D-Pointer 模式
* 封装的 class 至少要提供 `handle`、`from`两个成员函数，一般还可能需要提供`get`函数。
  * handle：返回所对应的 wlroots 的结构体对象指针
  * from：一个静态函数，根据传入的 wlroots 的结构体对象指针，返回一个 class 对象，除系统错误外，返回值一定不为`nullptr`
  * get: 一个静态函数，根据传入的 wlroots 的结构体对象指针，返回一个 class 对象，如果找不到此结构体对应的 class 对象（一般在此之前明确创建过相关对象），则返回`nullptr`，使用此函数的返回值之前一定需要判断指针是否为空
* 对于没有提供信号，且在封装的 class 中无需任何成员变量时，可直接将 wlroots 的结构体指针强转为对应的 class 指针使用。可参考 [QWTexture 的实现](https://github.com/vioken/qwlroots/blob/master/src/render/qwtexture.cpp#L28)
* 对于 wlroots 中命名为`setxxxx`的信号，一般是用于通知`xxxx`被设置了，在 qwlroots 中封装时需将其改为`xxxxChanged`，以符合 Qt 信号的命名习惯
* 对于 wlr_xxxx struct 中的成员变量，当前阶段不进行封装，因为 wlroots 的成员变量在开发过程中可能会有比较大的变化，如果将其封装为类的成员函数，将导致相应的 API 也需要跟随变化，等以后 wlroots 相对稳定后再进行封装。相应的，如果一个 wlr_xxxx struct 没有对应的函数，仅被当作数据结构访问其成员变量，因此此类 struct 也暂不封装

> 以上部分内容可参考 [class 模板](https://github.com/vioken/qwlroots/blob/master/src/class_template.txt)。
