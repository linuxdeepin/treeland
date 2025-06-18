# Treeland

treeland is a wayland compositor based on wlroots and QtQuick, designed to provide efficient and flexible graphical interface support.

## Dependencies

Check the `debian/control` file to understand specific build and runtime dependencies, or use `cmake` to check for missing necessary components.

Core build dependencies:

- [waylib](https://github.com/vioken/waylib): A Wayland compositor development library based on wlroots and QtQuick
  - Qt >= 6.8.0
  - wlroots = 0.19
- [treeland-protocols](https://github.com/linuxdeepin/treeland-protocols): Private Wayland protocols used by treeland

Recommended runtime dependencies:

- [ddm](https://github.com/linuxdeepin/ddm): A display manager optimized for multiple users

## Building

Treeland uses cmake for building. The WITH_SUBMODULE_WAYLIB option can force the use of the waylib code from the submodule. If you want to use the system-provided waylib, set this option to OFF.

Using the system-provided waylib:

```shell
$ git clone git@github.com:linuxdeepin/treeland.git
$ cd treeland
$ cmake -Bbuild -DWITH_SUBMODULE_WAYLIB=OFF
$ cmake --build build
```

Using the waylib from the submodule:

```shell
$ git clone git@github.com:linuxdeepin/treeland.git --recursive
$ cd treeland
$ cmake -Bbuild -DWITH_SUBMODULE_WAYLIB=ON
$ cmake --build build
```

## Packaging

A `debian` folder is provided to build the package under the *deepin* linux desktop distribution. To build the package, use the following command:

```shell
$ sudo apt build-dep . # install build dependencies
$ dpkg-buildpackage -uc -us -nc -b # build binary package(s)
```

## Getting Involved

- [Code contribution via GitHub](https://github.com/linuxdeepin/treeland/)
- [Submit bug or suggestions to GitHub Issues or GitHub Discussions](https://github.com/linuxdeepin/developer-center/issues/new/choose)

## License

treeland is licensed under Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only.
