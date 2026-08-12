{
  pkgs ? import <nixpkgs> { },
  nix-filter,
  ddm,
  treeland-protocols,
}:
rec {
  waylib = pkgs.qt6Packages.callPackage ./waylib/nix {
    inherit nix-filter;
    makeTest = import (pkgs.path + "/nixos/tests/make-test-python.nix");
  };

  treeland = pkgs.qt6Packages.callPackage ./nix {
    inherit
      nix-filter
      ddm
      treeland-protocols
      waylib
      ;
  };
}
