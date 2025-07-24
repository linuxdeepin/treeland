{
  pkgs ? import <nixpkgs> { },
  nix-filter,
  ddm,
  treeland-protocols,
}:
rec {
  qwlroots = pkgs.qt6Packages.callPackage ./qwlroots/nix {
    inherit nix-filter;
    wlroots = pkgs.wlroots_0_19;
  };

  waylib = pkgs.qt6Packages.callPackage ./waylib/nix {
    inherit nix-filter qwlroots;
    makeTest = import (pkgs.path + "/nixos/tests/make-test-python.nix");
  };

  treeland = pkgs.qt6Packages.callPackage ./nix {
    inherit
      nix-filter
      ddm
      treeland-protocols
      qwlroots
      waylib
      ;
  };
}
