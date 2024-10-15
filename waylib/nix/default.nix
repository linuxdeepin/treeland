{ stdenv
, lib
, nix-filter
, cmake
, pkg-config
, wayland-scanner
, wrapQtAppsHook
, qtbase
, qtquick3d
, qwlroots
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

  src = nix-filter.filter {
    root = ./..;

    exclude = [
      ".git"
      "debian"
      "LICENSES"
      "README.md"
      "README.zh_CN.md"
      (nix-filter.matchExt "nix")
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
    qwlroots
    wayland
    wayland-protocols
    wlr-protocols
    pixman
    libdrm
    libinput
  ];

  cmakeBuildType = if debug then "Debug" else "Release";

  cmakeFlags = [
    (lib.cmakeBool "INSTALL_TINYWL" true)
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

