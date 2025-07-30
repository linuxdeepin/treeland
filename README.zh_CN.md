# Treeland

treeland 是一个基于 wlroots 和 QtQuick 开发的 Wayland 合成器，旨在提供高效且灵活的图形界面支持。

## 依赖

查看 `debian/control` 文件来了解具体的构建与运行时依赖，或者使用 `cmake` 检查缺失的必要组件。

核心构建依赖：

- [waylib](https://github.com/vioken/waylib) 整合 wlroots 和 QtQuick 的 Wayland 合成器开发库
  - Qt >= 6.8.0
  - wlroots = 0.19
- [treeland-protocols](https://github.com/linuxdeepin/treeland-protocols) treeland 使用的私有 wayland 协议

推荐的运行时依赖：

- [ddm](https://github.com/linuxdeepin/ddm) 为多用户优化的登录管理器

## 构建

treeland 使用 cmake 进行构建，`WITH_SUBMODULE_WAYLIB` 选项可以强制使用子模块中的 `waylib` 代码，如果希望使用系统提供的 `waylib` 应该设置为 `OFF`。

使用系统库提供的 `waylib`：

```shell
$ git clone git@github.com:linuxdeepin/treeland.git
$ cd treeland
$ cmake -Bbuild -DWITH_SUBMODULE_WAYLIB=OFF
$ cmake --build build
```
使用子模块中的 `waylib`：

```shell
$ git clone git@github.com:linuxdeepin/treeland.git --recursive
$ cd treeland
$ cmake -Bbuild -DWITH_SUBMODULE_WAYLIB=ON
$ cmake --build build
```

## 打包

在 *deepin* 桌面发行版进行此软件包的构建，我们还提供了一个 `debian` 目录。若要构建软件包，可参照下面的命令进行构建：

```shell
$ sudo apt build-dep . # 安装构建依赖
$ dpkg-buildpackage -uc -us -nc -b # 构建二进制软件包
```

## GitHub Actions / 持续集成

本项目使用 GitHub Actions 进行持续集成。配置了以下工作流：

- **qwlroots 构建**：当 `qwlroots/**` 文件被修改时触发
- **waylib 构建**：当 `waylib/**` 或 `qwlroots/**` 文件被修改时触发（因为 waylib 依赖于 qwlroots）
- **treeland 构建**：主项目构建

waylib 工作流配置为在 qwlroots 代码变化时也会触发，确保 waylib 构建与 qwlroots 修改保持兼容。

## 参与贡献

- [通过 GitHub 发起代码贡献](https://github.com/linuxdeepin/treeland/)
- [通过 GitHub Issues 与 GitHub Discussions 汇报缺陷与反馈建议](https://github.com/linuxdeepin/developer-center/issues/new/choose)

## 许可协议

**Treeland** 使用 Apache-2.0, LGPL-3.0-only, GPL-2.0-only 或 GPL-3.0-only 许可协议进行发布。
