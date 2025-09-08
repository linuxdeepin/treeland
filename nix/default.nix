{
  stdenv,
  lib,
  nix-filter,
  cmake,
  ninja,
  extra-cmake-modules,
  pkg-config,
  wayland-scanner,
  qttools,
  wrapQtAppsHook,
  qtbase,
  qtdeclarative,
  qtimageformats,
  qtwayland,
  qtsvg,
  ddm,
  deepin,
  wayland,
  wayland-protocols,
  wlr-protocols,
  wlroots_0_19,
  treeland-protocols,
  pixman,
  pam,
  libxcrypt,
  libinput,
  nixos-artwork,
  qwlroots,
  waylib,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "treeland";
  version = "0.5-unstable";

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

  postPatch = ''
    for file in $(grep -rl "/usr/share/wallpapers/deepin/desktop.jpg")
    do
      substituteInPlace $file \
        --replace-fail "/usr/share/wallpapers/deepin/desktop.jpg" \
                "${nixos-artwork.wallpapers.simple-blue}/share/backgrounds/nixos/nix-wallpaper-simple-blue.png"
    done
  '';

  depsBuildBuild = [ pkg-config ];

  nativeBuildInputs = [
    cmake
    ninja
    extra-cmake-modules
    pkg-config
    wayland-scanner
    qttools
    wrapQtAppsHook
  ];

  buildInputs = [
    qtbase
    qtdeclarative
    qtimageformats
    qtwayland
    qtsvg
    ddm
    deepin.dtk6core
    deepin.dtk6declarative
    deepin.dtk6systemsettings
    wayland
    wayland-protocols
    wlr-protocols
    treeland-protocols
    wlroots_0_19
    pixman
    pam
    libxcrypt
    libinput
    qwlroots
    waylib
  ];

  cmakeFlags = [
    (lib.cmakeFeature "QT_IMPORTS_DIR" "${placeholder "out"}/${qtbase.qtQmlPrefix}")
    (lib.cmakeFeature "CMAKE_INSTALL_SYSCONFDIR" "${placeholder "out"}/etc")
    (lib.cmakeFeature "SYSTEMD_SYSTEM_UNIT_DIR" "${placeholder "out"}/lib/systemd/system")
    (lib.cmakeFeature "SYSTEMD_SYSUSERS_DIR" "${placeholder "out"}/lib/sysusers.d")
    (lib.cmakeFeature "SYSTEMD_TMPFILES_DIR" "${placeholder "out"}/lib/tmpfiles.d")
    (lib.cmakeFeature "DBUS_CONFIG_DIR" "${placeholder "out"}/share/dbus-1/system.d")
    (lib.cmakeBool "WITH_SUBMODULE_WAYLIB" false)
  ];

  env.PKG_CONFIG_SYSTEMD_SYSTEMDUSERUNITDIR = "${placeholder "out"}/lib/systemd/user";

  # RPATH of binary /nix/store/.../bin/... contains a forbidden reference to /build/
  noAuditTmpdir = true;

  meta = {
    description = "Wayland compositor based on wlroots and QtQuick";
    homepage = "https://github.com/linuxdeepin/treeland";
    license = with lib.licenses; [
      gpl3Only
      lgpl3Only
      asl20
    ];
    platforms = lib.platforms.linux;
    maintainers = with lib.maintainers; [ rewine ];
  };
})
