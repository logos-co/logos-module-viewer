{
  description = "Logos Module Viewer - A Qt UI application for viewing Logos modules";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        pkgs = import nixpkgs { inherit system; };
      });
    in
    {
      packages = forAllSystems ({ pkgs }: 
        let
          common = import ./nix/default.nix { inherit pkgs; };
          src = ./.;
          app = import ./nix/app.nix { inherit pkgs common src; };
        in
        {
          app = app;
          default = app;
        }
      );

      devShells = forAllSystems ({ pkgs }: {
        default = pkgs.mkShell {
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
          ];
          buildInputs = [
            pkgs.qt6.qtbase
          ];
          
          shellHook = ''
            echo "Logos Module Viewer development environment"
            echo "Build with: nix build"
            echo "Run with: ./result/bin/logos-module-viewer"
          '';
        };
      });
    };
}

