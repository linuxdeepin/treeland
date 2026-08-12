{ stdenv
, lib
, nix-filter
, cmake
, pkg-config
, wayland-scanner
, wrapQtAppsHook
, qtbase
, qtquick3d
, wayland
, wayland-protocols
, wlr-protocols
, pixman
, libdrm
, libinput
, xkbcommon
, libseat
, libdisplay-info
, hwdata
, systemdLibs
, libxcb
, xcbutilwm
, xcbutil-errors
, libglvnd
, mesa
, vulkan-headers
, vulkan-loader
, glslang
, lcms2
, libliftoff
, xwayland
, linuxHeaders
, nixos-artwork

# only for test
, makeTest ? null
, pkgs ? null
, waylib ? null
, debug ? true
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "waylib";
  version = "0.6.13";

  src = nix-filter.lib.filter {
    root = ./..;

    exclude = [
      ".git"
      "debian"
      "LICENSES"
      "README.md"
      "README.zh_CN.md"
      (nix-filter.lib.matchExt "nix")
    ];
  };

  depsBuildBuild = [ pkg-config ];

  nativeBuildInputs = [
    cmake
    pkg-config
    wayland-scanner
    wrapQtAppsHook
  ];

  buildInputs = [
    qtbase
    qtquick3d
    wayland
    wayland-protocols
    wlr-protocols
    pixman
    libdrm
    libinput
    xkbcommon
    libseat
    libdisplay-info
    hwdata
    systemdLibs
    libxcb
    xcbutilwm
    xcbutil-errors
    libglvnd
    mesa.dev
    vulkan-headers
    vulkan-loader
    glslang
    lcms2
    libliftoff
    xwayland
    linuxHeaders
  ];

  cmakeBuildType = if debug then "Debug" else "Release";

  cmakeFlags = [
    (lib.cmakeBool "BUILD_EXAMPLES" false)
    (lib.cmakeBool "ADDRESS_SANITIZER" debug)
  ];

  strictDeps = true;

  outputs = [ "out" "dev" ];

  passthru.tests = import ./nixos-test.nix {
    inherit pkgs makeTest waylib;
  };

  meta = {
    description = "A wrapper for wlroots based on Qt";
    homepage = "https://github.com/vioken/waylib";
    license = with lib.licenses; [ gpl3Only lgpl3Only asl20 ];
    platforms = lib.platforms.linux;
    maintainers = with lib.maintainers; [ rewine ];
  };
})

