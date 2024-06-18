{ pkgs, makeTest, waylib }:

makeTest
{
  name = "waylib-tinywl-test";
  meta.maintainers = with pkgs.lib.maintainers; [ rewine ];

  nodes.machine = { ... }:

    {

      fonts.packages = with pkgs; [ dejavu_fonts ];

      imports = [  (pkgs.path + "/nixos/tests/common/user-account.nix") ];
      
      virtualisation.memorySize = 2048;
      
      services.getty.autologinUser = "alice";
      security.polkit.enable = true;
      hardware.opengl.enable = true;
      programs.xwayland.enable = true;
      environment = {
        variables = {
          WLR_RENDERER = "pixman";
          XDG_RUNTIME_DIR = "/run/user/1000";
          WAYLIB_DISABLE_GESTURE = "on";
        };
        systemPackages = with pkgs; [
          waylib
          wayland-utils
          foot
          xterm
          wlogout
        ];
      };

      # Automatically start TinyWL when logging in on tty1:
      programs.bash.loginShellInit = ''
        if [ "$(tty)" = "/dev/tty1" ]; then
          set -e
          tinywl-qtquick |& tee /tmp/tinywl.log
          touch /tmp/tinywl-exit-ok
        fi
      '';

      # Need to switch to a different GPU driver than the default one (-vga std) so that Tinywl can launch:
      virtualisation.qemu.options = [ "-vga virtio -device virtio-gpu-pci" ];
    };

  enableOCR = true;

  testScript = { nodes, ... }:
    let
      user = nodes.machine.users.users.alice;
    in
    ''
      start_all()
      machine.wait_for_unit("multi-user.target")

      # Wait for complete startup:
      machine.wait_until_succeeds("pgrep tinywl-qtquick")
      machine.wait_for_file("/run/user/1000/wayland-0")

      print(machine.succeed("wayland-info"))

      # Test run client in wayland
      machine.succeed("su - ${user.name} -c 'foot >&2 &'")
      machine.wait_until_succeeds("pgrep foot")
      machine.screenshot("tinywl_foot")

      # Test run client in xwayland
      machine.succeed("su - ${user.name} -c 'xterm >&2 &'")
      machine.wait_until_succeeds("pgrep xterm")
      machine.screenshot("tinywl_xterm")

      # Test layer shell
      machine.succeed("su - ${user.name} -c 'wlogout -p layer-shell >&2 &'")
      machine.wait_until_succeeds("pgrep wlogout")
      machine.screenshot("tinywl_wlogout")

      # Terminate cleanly:
      machine.send_key("ctrl-q")
      # machine.wait_until_fails("pgrep tinywl-qtquick")
      # machine.wait_for_file("/tmp/tinywl-exit-ok")
      machine.copy_from_vm("/tmp/tinywl.log")
    '';
} {
  inherit pkgs;
  inherit (pkgs) system;
}

