## Language

[简体中文](./README.zh_CN.md)

## Introduction

qwlroots is a binding of [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots), which provides a Qt style development interface. It aims to simplify wlroots API calling methods with Qt, and serve the needs of calling wlroots within Qt projects. In qwlroots, each wlroots struct is wrapped as a C++ class with its corresponding functions. At the same time, wl_singals in Wayland are wrapped as Qt signals.

## Supported wlroots versions

As wlroots is still in development, and has not released its 1.0 version yet, there might be large changes at any time. Therefore, the latest code of qwlroots is only guaranteed to be compatible with the latest version of wlroots and the next release version (i.e., the version currently in development), and no history versions are guaranteed to be compatible. Meanwhile, just like wlroots, qwlroots currently does not provide long-term support for interface compatibility.

## Supported Environments

* Arch Linux
* Debian
* NixOS

> Other environments have not been tested yet.

## Build

Step 1: Compile and install [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots#building)

> It's also possible to install wlroots with the system repository. (Pay attention to its version.)


Step 2: Install dependencies

Debian
````shell
# apt install pkg-config cmake qt6-base-private-dev qt6-base-dev-tools wayland-protocols libpixman-1-dev
````

Arch Linux

````shell
# pacman -Syu --noconfirm qt6-base cmake pkgconfig pixman wayland-protocols ninja
````

Step 3: Run the following commands:

```shell
cmake -B build
cmake --build build
```

## How to contribute

Assuming you have already had experience with `Qt` and `wlroots` libraries, in order to better integrate Qt with wlroots, qwlroots follows Qt specifications in terms of interface style, wlroots' design philosophy in terms of low-level design concepts (e.g., module separation), and Qt's "encapsulation + hierarchy" design philosophy in terms of high-level parts that are not directly related to wlroots.

You are free to submit as many code contributions to this project as you like, as long as you follow the design philosophy of qwlroots and the following requirements.

### Code Style

* When modifying existing code, the current code style must be observed.
* Added code: Parts closely related to wlroots should follow wlroots code style, e.g., when using QWSignalConnector to link wl_signal, the corresponding slot function should use the "underscore naming convention"; other parts (especially the public APIs) should follow Qt code style (https://wiki.qt.io/Qt_Coding_Style This link is for reference only, please refer to the actual link of Qt source code)
* There is no absolute correct or wrong code style, please take the whole project rather than small parts in to consideration

### Code Quality

* Simplicity and understandability come first.
* Add comments at parts that are important, no matter modifying or adding code.
* Security > Compatibility > Scalability >= Performance

### Commit Specifications

* Commit Steps:
    1. First, you need to log in to your GitHub account and fork this project.
    2. Use `git clone` to get the forked project locally
    3. Push new commits to your project with `git push`
    4. Submit your code to the upstream project with the "Pull Request" function on Github.

* Commit Message Specification: consistent with the Qt project, written in English. Be sure to describe exactly "what the commit does" and "why do it this way"
* Each commit should only do one task, and the smaller the changes to the code, the more likely it will be accepted. For large code changes, try to split the commit into multiple commits (satisfying the git commit principle as a prerequisite).
* Please do the testing and code review by yourself before submitting the code, and only submit the PR after the code is confirmed to be error-free.

## Some probably generic requirements

### When wrapping a new wlroots type

* First, determine the lifecycle mechanism of this object, whether it needs to be destroyed manually by the allocator or will be destroyed with other objects. wlroots has some types that will be destroyed by wl_display.
* For types that will be destroyed automatically with other objects, the destructor needs to be marked private in the wrapped class
* For wlroots types that provide a destroy event, this signal needs to be connected in the wrapped class and will destroy itself when it receives the signal.
* For types that do not require manual destruction or do not provide a corresponding wlr_xxx_create_xxx function, there is no need to provide a public constructor in the wrapper class, and one or multiple `from` static functions are provided uniformly to create the object.
* For wlroots types that provide signals, the wrapper class needs to be inherited from QObject (and often also from QWObject)
* The wrapper classes always use Qt's D-Pointer mode for binary compatibility, and when member variables are needed, they need to be inherited from QWObject to implement the D-Pointer mode.
* The wrapped class should provide at least two member functions: `handle`and `from`. Generally, it may also need to provide the `get` function.
  * handle: Returns a pointer to the corresponding wlroots struct object 
  * from: A static function that returns a class object based on the passed in pointer to the wlroots struct object , the return value must not be `nullptr` except for system errors
  * get: A static function that returns a class object based on the pointer to the passed in wlroots structure object, if the class object corresponding to this struct is not found (generally, the relevant object has been created explicitly before), then `nullptr` is returned. Before using the return value of this function, the pointer must be determined whether it is null.
* If no signals are provided and no member variables are needed in the wrapper class, the wlroots struct pointer can be explicitly converted to the corresponding class pointer directly. See [QWTexture implementation](https://github.com/vioken/qwlroots/blob/master/src/render/qwtexture.cpp#L28)
* For signals named `setxxxx` in wlroots, they are generally used to indicate that `xxxx` has been set, and need to be changed to `xxxxChanged` when wrapped in qwlroots to comply with the Qt signals naming convention.
* For the member variables in wlr_xxxx struct, there is no wrapping at this stage, because the member variables of wlroots may change a lot during development, and if they are wrapped as class member functions, the corresponding APIs will need to adapt to the changes, and they will be wrapped later when wlroots is relatively stable. Therefore, if a wlr_xxxx struct does not have a corresponding function and is only used as a data structure to access its member variables, then this struct will not be wrapped at present.

> Some of the above content can be found in [class template](https://github.com/vioken/qwlroots/blob/master/src/class_template.txt)
