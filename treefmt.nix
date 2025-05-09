# treefmt.nix
{
  # Used to find the project root
  projectRootFile = "flake.nix";

  # Nix
  programs = {
    nixfmt.enable = true;
    statix.enable = true;
    # C++
    clang-format.enable = true;
    # Cmake
    cmake-format.enable = true;
    # Just
    just.enable = true;
    # General
    prettier.enable = true;
  };

  settings.on-unmatched = "debug";
}
