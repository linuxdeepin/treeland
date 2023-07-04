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
, pixman
, libdrm
}:

stdenv.mkDerivation rec {
  pname = "waylib";
  version = "0.0.1";

  src = nix-filter.filter {
    root = ./..;

    exclude = [
      ".git"
      "debian"
      "LICENSE"
      "README.md"
      "README.zh_CN.md"
      (nix-filter.matchExt "nix")
    ];
  };

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
    pixman
    libdrm
  ];

  strictDeps = true;

  meta = with lib; {
    description = "A wrapper for wlroots based on Qt";
    homepage = "https://github.com/vioken/waylib";
    license = licenses.gpl3Plus;
    platforms = platforms.linux;
  };
}

