{
  description = "Logos Module Viewer - A Qt UI application for viewing Logos modules";

  inputs = {
    nixpkgs.follows = "logos-liblogos/nixpkgs";
    logos-liblogos.url = "github:logos-co/logos-liblogos";
    logos-cpp-sdk.url = "github:logos-co/logos-cpp-sdk";
    logos-capability-module.url = "github:logos-co/logos-capability-module";
    logos-package-manager.url = "github:logos-co/logos-package-manager";
  };

  outputs = { self, nixpkgs, logos-liblogos, logos-cpp-sdk, logos-capability-module, logos-package-manager }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        pkgs = import nixpkgs { inherit system; };
        logosLiblogos = logos-liblogos.packages.${system}.default;
        logosSdk = logos-cpp-sdk.packages.${system}.default;
        logosCapabilityModule = logos-capability-module.packages.${system}.default;
        logosPackageManager = logos-package-manager.packages.${system}.default;
      });
    in
    {
      packages = forAllSystems ({ pkgs, logosLiblogos, logosSdk, logosCapabilityModule, logosPackageManager }: 
        let
          common = import ./nix/default.nix { inherit pkgs logosLiblogos logosSdk; };
          src = ./.;
          app = import ./nix/app.nix { inherit pkgs common src logosLiblogos logosSdk logosCapabilityModule logosPackageManager; };
        in
        {
          app = app;
          default = app;
        }
      );

      devShells = forAllSystems ({ pkgs, logosLiblogos, logosSdk, logosCapabilityModule, logosPackageManager }: {
        default = pkgs.mkShell {
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
          ];
          buildInputs = [
            pkgs.qt6.qtbase
            pkgs.qt6.qtremoteobjects
          ];
          
          shellHook = ''
            export LOGOS_LIBLOGOS_ROOT="${logosLiblogos}"
            export LOGOS_CPP_SDK_ROOT="${logosSdk}"
            export LOGOS_CAPABILITY_MODULE_ROOT="${logosCapabilityModule}"
            export LOGOS_PACKAGE_MANAGER_ROOT="${logosPackageManager}"
            echo "Logos Module Viewer development environment"
            echo "LOGOS_LIBLOGOS_ROOT: $LOGOS_LIBLOGOS_ROOT"
            echo "LOGOS_CPP_SDK_ROOT: $LOGOS_CPP_SDK_ROOT"
            echo "Build with: nix build"
            echo "Run with: ./result/bin/logos-module-viewer"
          '';
        };
      });
    };
}
