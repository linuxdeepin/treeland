{ pkgs ? import <inxpkgs> { }, wlroots_master_src ? null , nix-filter }:
let
  mkDate = longDate: (pkgs.lib.concatStringsSep "-" [
    (builtins.substring 0 4 longDate)
    (builtins.substring 4 2 longDate)
    (builtins.substring 6 2 longDate)
  ]);
in
rec {
  qwlroots-qt6 = pkgs.qt6.callPackage ./nix {
    inherit nix-filter;
    wlroots = pkgs.wlroots_0_19;
  };

  qwlroots-qt5 = pkgs.libsForQt5.callPackage ./nix {
    inherit nix-filter;
    wlroots = pkgs.wlroots_0_19;
  };

  qwlroots-qt6-wlroots18 = qwlroots-qt6.override {
    wlroots = pkgs.wlroots_0_18;
  };

  qwlroots-qt6-clang = qwlroots-qt6.override {
    stdenv = pkgs.clangStdenv;
  };

  default = qwlroots-qt6;
}
