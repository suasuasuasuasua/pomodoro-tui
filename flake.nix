# This is a Nix flake for building and running a C++ Pomodoro TUI app
{
  description = "C++ Pomodoro TUI Application";

  # Flake inputs: Nixpkgs for packages, flake-utils for multi-system support
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11"; # Pin all package versions here
    flake-utils.url = "github:numtide/flake-utils";
    pre-commit-hooks.url = "github:cachix/git-hooks.nix";
    treefmt-nix.url = "github:numtide/treefmt-nix";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      treefmt-nix,
      pre-commit-hooks,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        # Overlay to pin gcc, cmake, and nix to specific versions
        overlay = final: prev: {
          gcc = prev.gcc13;
          nix = prev.nixVersions.nix_2_25;
        };
        # Import the Nixpkgs package set for the current system, with overlay
        pkgs = import nixpkgs {
          inherit system;
          overlays = [ overlay ];
        };
        treefmtEval = treefmt-nix.lib.evalModule pkgs ./treefmt.nix;
      in
      {
        # Development shell for interactive work (nix develop)
        devShells.default = pkgs.mkShell {
          inherit (self.checks.${system}.pre-commit-check) shellHook;

          buildInputs =
            with pkgs;
            [
              clang-tools_18 # Provides clang-format and clang-tidy (latest stable)
              cmake
              doxygen
              gcc
              just
              ncurses
              nix-output-monitor
              nodePackages.prettier
              treefmt
            ]
            ++ self.checks.${system}.pre-commit-check.enabledPackages;
        };

        # The actual package derivation (for nix build)
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "pomodoro-tui"; # Package name
          version = "1.0.0";

          src = ./.; # Use the current directory as the source

          buildInputs = with pkgs; [
            gcc
            ncurses
            cmake
          ]; # Dependencies

          # Build phase: configure and build with CMake, using all CPU cores
          buildPhase = ''
            cmake -S $src -B build
            cmake --build build -- -j$NIX_BUILD_CORES
          '';

          # Install phase: copy the built binary to $out/bin
          installPhase = ''
            mkdir -p $out/bin
            cp build/pomodoro $out/bin/
          '';
        };

        # App definition for nix run
        apps.default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/pomodoro";
          meta = {
            description = "Modern C++ ncurses Pomodoro TUI";
            mainProgram = "pomodoro";
            platforms = [
              "x86_64-linux"
              "aarch64-linux"
              "x86_64-darwin"
              "aarch64-darwin"
            ];
            maintainers = [ "justinhoang" ];
            license = "MIT";
          };
        };

        # for `nix fmt`
        formatter = treefmtEval.config.build.wrapper;
        # for `nix flake check`
        checks = {
          formatting = treefmtEval.config.build.check self;
          pre-commit-check = pre-commit-hooks.lib.${system}.run {
            src = ./.;
            hooks = {
              nixfmt-rfc-style.enable = true;
              statix.enable = true;
              clang-format.enable = true;
              cmake-format.enable = true;
              prettier.enable = true;
            };
          };
        };
      }
    );
}
