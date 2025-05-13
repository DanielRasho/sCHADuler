{
  description = "A basic multiplatform flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = {
    self,
    nixpkgs,
  }: let
    # System types to support.
    supportedSystems = ["x86_64-linux" "x86_64-darwin" "aarch64-linux" "aarch64-darwin"];

    # Helper function to generate an attrset '{ x86_64-linux = f "x86_64-linux"; ... }'.
    forAllSystems = nixpkgs.lib.genAttrs supportedSystems;

    # Nixpkgs instantiated for supported system types.
    nixpkgsFor = forAllSystems (system: import nixpkgs {inherit system;});

    # Remember to update this command every time it changes on the nob file!
    schedulingBasicCompilation = ''clang $(pkg-config --cflags gtk4) $(pkg-config --libs gtk4) -g -O0 -Wall -o build/main src/main.c'';
  in {
    devShells = forAllSystems (system: let
      pkgs = nixpkgsFor.${system};
      schedulingPkgs = [pkgs.gtk4 pkgs.pkg-config pkgs.bear];
    in {
      default = pkgs.mkShell {
        packages = [pkgs.clang] ++ schedulingPkgs;
        shellHook = ''
          echo "THIS SHELL MUST BE STARTED FROM THE REPO ROOT!"
          echo "Generating Scheduling clangd tooling files..."
            cd ./scheduling/ || exit
            bear -- ${schedulingBasicCompilation}
            cd .. || exit
          echo "DONE!"
        '';
      };
    });
  };
}
