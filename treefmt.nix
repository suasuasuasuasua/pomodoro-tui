# treefmt.nix
{
  # Used to find the project root
  projectRootFile = "flake.nix";

  # Nix
  programs.nixfmt.enable = true;
  programs.statix.enable = true;
  # C++
  programs.clang-format.enable = true;
  # Cmake
  programs.cmake-format.enable = true;
  # Just
  programs.just.enable = true;
  # General
  programs.prettier.enable = true;

  settings.on-unmatched = "debug";
}
