{
  description = "A basic flake to help develop treeland";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    nix-filter.url = "github:numtide/nix-filter";
    dde-nixos = {
      url = "github:linuxdeepin/dde-nixos";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    waylib = {
      url = "github:vioken/waylib";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.flake-utils.follows = "flake-utils";
      inputs.nix-filter.follows = "nix-filter";
    };
  };

  outputs = { self, nixpkgs, flake-utils, nix-filter, waylib, dde-nixos }@input:
    flake-utils.lib.eachSystem [ "x86_64-linux" "aarch64-linux" "riscv64-linux" ]
      (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};

          treeland = pkgs.qt6Packages.callPackage ./nix {
            nix-filter = nix-filter.lib;
            waylib = waylib.packages.${system}.default;
            inherit (dde-nixos.packages.${system}.qt6) dtkdeclarative dtksystemsettings;
          };
        in
        {
          packages.default = treeland;

          devShells.default = pkgs.mkShell {
            packages = with pkgs; [
              # For submodule build
              libinput
              wayland
            ];

            inputsFrom = [
              self.packages.${system}.default
            ];

            shellHook = let
              makeQtpluginPath = pkgs.lib.makeSearchPathOutput "out" pkgs.qt6.qtbase.qtPluginPrefix;
              makeQmlpluginPath = pkgs.lib.makeSearchPathOutput "out" pkgs.qt6.qtbase.qtQmlPrefix;
            in ''
              # unexpected QT_NO_DEBUG form qt-base-hook
              # https://github.com/NixOS/nixpkgs/issues/251918
              export NIX_CFLAGS_COMPILE=$(echo $NIX_CFLAGS_COMPILE | sed 's/-DQT_NO_DEBUG//')
              #export QT_LOGGING_RULES="*.debug=true;qt.*.debug=false"
              #export WAYLAND_DEBUG=1
              export QT_PLUGIN_PATH=${makeQtpluginPath (with pkgs.qt6; [ qtbase qtdeclarative qtquick3d qtimageformats qtwayland qt5compat qtsvg ])}
              export QML2_IMPORT_PATH=${makeQmlpluginPath (with pkgs.qt6; [ qtdeclarative qtquick3d qt5compat ] 
                                                          ++ [ dde-nixos.packages.${system}.qt6.dtkdeclarative ] )}
              export QML_IMPORT_PATH=$QML2_IMPORT_PATH
            '';
          };
        }
      );
}
