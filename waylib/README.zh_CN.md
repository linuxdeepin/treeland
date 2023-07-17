# waylib

## 简介

waylib 是一个 Wayland 合成器开发库，基于 [qwlroots](https://github.com/vioken/qwlroots) 开发，提供 Qt 风格的开发接口。其设计目标是与 QtQuick 深度结合，利用 QtQuick 的 Scene Graphics 模型，以简化窗口管理的复杂度。在 waylib 中，一个 QQuickWindow 上可以被附加一个或多个 Wayland Output，一个 QQuickItem 上会附加一个对应的 Wanyland Surface，可将其与 QtQuick 的图形组件混合使用，并且支持 QRHI，只需要一份代码即可兼容 OpenGL 和 Vulkan。

使用 waylib 可以简单并高效的创建一个 Wayland 合成器，其在 wlroots 之上，提供了：

* 基于 QtQuick 的渲染模型
* 支持 Qt 事件模型
* 会话管理系统（开发中）
* 客户端/窗口组策略（开发中）
* 窗口管理抽象（开发中）

基于以上功能，合成器开发者只需聚焦于窗口管理的业务需求，可以像实现一个普通的应用程序那样开发窗口合成器。

## 特性

* 支持切换不同的显示后端：DRM、Wayland、X11
* 支持自定义输入后端，默认使用 libinput
* 支持软硬件两种方式渲染光标
* 支持 X Window 的光标文件格式和主题
* 支持在无 dri 设备的环境中运行
* 可与 Qt platform 插件系统共存
* 提供 QML 组件

## 构建

步骤一：编译安装 wlroots 和 qwlroots

waylib 需要安装开发版本（0.17）的 wlroots, 需要[自行编译安装](https://gitlab.freedesktop.org/wlroots/wlroots#building)， Archlinux 用户可以安装 aur 提供的 [wlroots-git](https://aur.archlinux.org/packages/wlroots-git)。

qwlroot 目前推荐使用 submodule 提供的版本，当然也可以[自行编译安装](https://github.com/vioken/qwlroots)。
如果使用 submodule 的版本，注意以下 2 点：

1. 下载源码时初始化子模块 `git clone git@github.com:vioken/waylib.git --recursive`
2. 编译时使用添加一个 cmake 参数 "-DWITH_SUBMODULE_QWLROOTS=ON" 启用 submodule 的构建。

步骤二：安装其他依赖

Debian

````
# apt install pkg-config qt6-base-private-dev qt6-base-dev-tools qt6-declarative-private-dev wayland-protocols libpixman-1-dev
````

Archlinux

````
# pacman -Syu --noconfirm qt6-base qt6-declarative cmake pkgconfig pixman wayland-protocols ninja
````

NixOS

推荐使用 [nix-direnv](https://github.com/nix-community/nix-direnv) 管理依赖，也可以使用 `nix develop` 命令进入构建环境。

使用 `nix build -v -L` 可以完成打包构建。

步骤三：运行以下命令

```bash
cmake -B build -DWITH_SUBMODULE_QWLROOTS=ON
cmake --build build
```

## 贡献指南

此项目默认您已经拥有 `Qt` 和 `wlroots` 库的使用经验，为了能更好的融合 Qt 和 wlroots，waylib 在接口风格上遵守Qt的相关规范，在底层的设计理念上遵守 wlroots 的模块化设计，上层与 wlroots 无直接关联的部分则遵守Qt的封装+分层的设计。

在遵守 waylib 的设计理念和以下几类要求的前提下，您可以自由地向此项目提交任意的代码贡献。

### 编码风格

* 在修改已有代码时，需遵守当前的代码风格
* 新增代码：与 wlroots 密切相关的部分遵守 wlroots 的代码风格；其它部分遵守Qt的代码风格（https://wiki.qt.io/Qt_Coding_Style 此链接仅为参考，实际请以Qt源码为准）
* 代码风格没有绝对的对与错，请顾全大局，而勿拘于小结

### 代码质量

* 代码以简单明了、容易理解为优先
* 无论是改动或新增代码，均需要在关键节点增加注释
* 安全性 > 兼容性 > 可扩展性 >= 性能

### 提交规范

* 提交步骤：
    1. 首先需要登录您的Github帐号，fork此项目
    2. 在本地使用`git clone`拉取fork之后的项目
    3. 将新的提交使用`git push`推送到您的项目中
    4. 在Github上使用Pull Requese功能提交您的代码到上游项目、
* commit信息规范：与Qt项目保持一致，统一使用英文。一定要准确描述出这个提交“做了什么”、“为什么要这样做”
* 一个commit只做一件事情，代码量改动越小的commit越容易被接受。对于较大的代码改动，尽量拆分为多个代码提交（满足git commit的原则为前提）
* 在提交代码之前请自行进行测试和代码Review，在确认代码无误后再提交PR

## 未来计划

* 支持 vulkan
* 支持硬件叠加层（Hardware Plane）
* 支持虚拟设备（远程屏幕投射，鼠标、键盘多设备共享）
* 支持多端融合
* 支持高刷新率（120HZ）
* 支持屏幕可变刷新率
