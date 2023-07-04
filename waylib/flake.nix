{
  description = "A basic flake to help develop waylib";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    nix-filter.url = "github:numtide/nix-filter";
    qwlroots = {
      url = "github:vioken/qwlroots";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.flake-utils.follows = "flake-utils";
      inputs.nix-filter.follows = "nix-filter";
    };
  };

  outputs = { self, nixpkgs, flake-utils, nix-filter, qwlroots }@input:
    flake-utils.lib.eachSystem [ "x86_64-linux" "aarch64-linux" "riscv64-linux" ]
      (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};

          waylib = pkgs.qt6.callPackage ./nix {
            nix-filter = nix-filter.lib;
            qwlroots = qwlroots.packages.${system}.default;
          };
        in
        rec {
          packages.default = waylib;

          devShells.default = pkgs.mkShell { 
            packages = with pkgs; [
              wayland-utils
            ];

            inputsFrom = [
              packages.default
            ];

            shellHook = ''
              echo "welcome to waylib"
              echo "wlroots: $(pkg-config --modversion wlroots)"
              echo "wayland-server: $(pkg-config --modversion wayland-server)"
              export QT_LOGGING_RULES="*.debug=true;qt.*.debug=false"
              export MESA_DEBUG=1
              export EGL_LOG_LEVEL=debug
              export LIBGL_DEBUG=verbose
              export WAYLAND_DEBUG=1
            '';
          };
        }
      );
}
