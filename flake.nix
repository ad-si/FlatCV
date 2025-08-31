{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      utils,
    }:
    utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            bash
            clang-tools  # For clang-format
            coreutils
            doxygen
            emscripten
            gcc
            gnumake
            graphicsmagick
            hyperfine
            imagemagick
            mdbook
            rsync
            uv
            vips
          ];
          shellHook = ''
            cargo install --locked scrut@0.4.1
          '';
        };
        formatter = pkgs.nixfmt-tree; # Format this file with `nix fmt`
      }
    );
}
