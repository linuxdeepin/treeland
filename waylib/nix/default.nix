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
, deepin
, buildTinywl ? true
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

  postPatch = lib.optionalString buildTinywl ''
    substituteInPlace examples/tinywl/Main.qml \
      --replace "/usr/share/backgrounds/deepin/desktop.jpg" "${deepin.deepin-wallpapers}/share/backgrounds/default_background.jpg"
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
    qwlroots
    wayland
    wayland-protocols
    pixman
    libdrm
  ];

  cmakeFlags = [
    "-DBUILD_TINYWL=${if buildTinywl then "ON" else "OFF"}"
  ];

  strictDeps = true;

  meta = with lib; {
    description = "A wrapper for wlroots based on Qt";
    homepage = "https://github.com/vioken/waylib";
    license = licenses.gpl3Plus;
    platforms = platforms.linux;
  };
}

