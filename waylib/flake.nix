{
  description = "A basic flake to help develop waylib";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    nix-filter.url = "github:numtide/nix-filter";
  };

  outputs = { self, nixpkgs, flake-utils, nix-filter }@input:
    flake-utils.lib.eachSystem [ "x86_64-linux" "aarch64-linux" "riscv64-linux" ]
      (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};

          waylib = pkgs.qt6Packages.callPackage ./nix {
            # for test
            inherit pkgs waylib nix-filter;
            makeTest = import (pkgs.path + "/nixos/tests/make-test-python.nix");
          };
        in
        {
          packages.default = waylib;

          checks.default = self.packages.${system}.default.tests;

          devShells.default = pkgs.mkShell {
            packages = with pkgs; [
              wayland-utils
              qt6.qtwayland
            ];

            inputsFrom = [
              self.packages.${system}.default
            ];

            shellHook = let
              makeQtpluginPath = pkgs.lib.makeSearchPathOutput "out" pkgs.qt6.qtbase.qtPluginPrefix;
              makeQmlpluginPath = pkgs.lib.makeSearchPathOutput "out" pkgs.qt6.qtbase.qtQmlPrefix;
            in ''
              echo "welcome to waylib"
              echo "wayland-server: $(pkg-config --modversion wayland-server)"
              #export QT_LOGGING_RULES="*.debug=true;qt.*.debug=false"
              #export MESA_DEBUG=1
              #export EGL_LOG_LEVEL=debug
              #export LIBGL_DEBUG=verbose
              #export WAYLAND_DEBUG=1
              export QT_PLUGIN_PATH=${makeQtpluginPath (with pkgs.qt6; [ qtbase qtdeclarative qtwayland ])}
              export QML2_IMPORT_PATH=${makeQmlpluginPath (with pkgs.qt6; [ qtdeclarative ])}
              export QML_IMPORT_PATH=$QML2_IMPORT_PATH
            '';
          };
        }
      );
}
