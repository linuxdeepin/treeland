{ stdenv
, lib
, nix-filter
, cmake
, pkg-config
, wayland-scanner
, wrapQtAppsHook
, qtbase
, qtquick3d
, wlroots_0_19
, wayland
, wayland-protocols
, wlr-protocols
, pixman
, libdrm
, libinput
, nixos-artwork

# only for test
, makeTest ? null
, pkgs ? null
, waylib ? null
, debug ? true
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "waylib";
  version = "0.1.1";

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
    wlroots_0_19
    wayland
    wayland-protocols
    wlr-protocols
    pixman
    libdrm
    libinput
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
    description = "A QtQuick-friendly compositor framework based on wlroots";
    homepage = "https://github.com/vioken/waylib";
    license = with lib.licenses; [ gpl3Only lgpl3Only asl20 ];
    platforms = lib.platforms.linux;
    maintainers = with lib.maintainers; [ rewine ];
  };
})
