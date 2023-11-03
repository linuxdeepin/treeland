{ stdenv
, lib
, nix-filter
, fetchFromGitHub
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
, nixos-artwork

# only for test 
, makeTest ? null
, pkgs ? null
, waylib ? null
}:

stdenv.mkDerivation rec {
  pname = "waylib";
  version = "0.0.1";

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

  postPatch = ''
    substituteInPlace examples/tinywl/OutputDelegate.qml \
      --replace "/usr/share/wallpapers/deepin/desktop.jpg" \
                "${nixos-artwork.wallpapers.simple-blue}/share/backgrounds/nixos/nix-wallpaper-simple-blue.png"
  '';

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
  ];

  propagatedBuildInputs = [
    qwlroots
  ];

  cmakeFlags = [
    "-DINSTALL_TINYWL=ON"
  ];

  strictDeps = true;

  outputs = [ "out" "dev" ];

  passthru.tests = import ./nixos-test.nix {
    inherit pkgs makeTest waylib;
  };

  meta = with lib; {
    description = "A wrapper for wlroots based on Qt";
    homepage = "https://github.com/vioken/waylib";
    license = licenses.gpl3Plus;
    platforms = platforms.linux;
  };
}

