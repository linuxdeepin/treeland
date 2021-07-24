# waylib

## 简介

waylib 是一个 Wayland 合成器开发库，基于 [wlroots](https://github.com/swaywm/wlroots) 开发，提供 Qt 风格的开发接口。其设计目标是与 QtQuick 深度结合，利用 QtQuick 的 Scene Graphics 模型，以简化窗口管理的复杂度。在 waylib 中，每个 Output 即是一个 QQuickWindow，每个 Surface 是一个 QQuickItem，可将其与 QtQuick 的图形组件混合使用，并且支持 QRHI，只需要一份代码即可兼容 OpenGL 和 Vulkan。

使用 waylib 可以简单并高效的创建一个 Wayland 合成器，其在 wlroots 之上，提供了：

* 基于 QtQuick 的渲染模型
* 支持 Qt 事件模型
* 会话管理系统（开发中）
* 客户端/窗口组策略（开发中）
* 窗口动效系统（开发中）
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

步骤一：编译安装 [wlroots](https://github.com/swaywm/wlroots#building)


步骤二：安装依赖

* pkg-config
* qtbase5
* qtbase5-private
* qtbase5-dev-tools
* qtdeclarative5
* qtdeclarative5-private
* libpixman

步骤三：运行以下命令

```bash
mkdir build && cd build
qmake ..
make
```

## 贡献指南

待补充

## 未来计划

* 支持 vulkan
* 支持硬件叠加层（Hardware Plane）
* 支持虚拟设备（远程屏幕投射，鼠标、键盘多设备共享）
* 支持多端融合
* 支持高刷新率（120HZ）
* 支持屏幕可变刷新率
