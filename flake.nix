{
  description = "Logos Module Viewer - A Qt UI application for viewing Logos modules";

  inputs = {
    nixpkgs.follows = "logos-liblogos/nixpkgs";
    # The runtime-control wave: the app is the "module_viewer" shell of a runtime
    # whose capability_module is the token authority. Back to master as
    # liblogos#227, protocol#97 and plugin-qt#48 merge.
    logos-liblogos.url = "github:logos-co/logos-liblogos/feat/embedded-core-service";
    # The Qt host runtime this app links (TokenManager, LogosAPI), which
    # liblogos' Qt-free core does not ship. One protocol and one qt-host in the
    # app: qt-host bakes sizeof(LogosAPIClient) into code the protocol defines.
    logos-protocol.url = "github:logos-co/logos-protocol/feat/plain-local-inproc";
    logos-plugin-qt.url = "github:logos-co/logos-plugin-qt/feat/consumer-adoption-only";
    logos-plugin-qt.inputs.logos-protocol.follows = "logos-protocol";
    logos-liblogos.inputs.logos-protocol.follows = "logos-protocol";
    logos-liblogos.inputs.logos-plugin-qt.follows = "logos-plugin-qt";
    logos-capability-module.url = "github:logos-co/logos-capability-module";
    logos-package-manager.url = "github:logos-co/logos-package-manager-module";
  };

  outputs = { self, nixpkgs, logos-liblogos, logos-protocol, logos-plugin-qt, logos-capability-module, logos-package-manager }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        pkgs = import nixpkgs { inherit system; };
        logosLiblogos = logos-liblogos.packages.${system}.default;
        logosProtocolPkg = logos-protocol.packages.${system}.default;
        logosQtHost = logos-plugin-qt.packages.${system}.logos-qt-host;
        logosCapabilityModule = logos-capability-module.packages.${system}.default;
        logosPackageManager = logos-package-manager.packages.${system}.default;
      });
    in
    {
      packages = forAllSystems ({ pkgs, logosLiblogos, logosProtocolPkg, logosQtHost, logosCapabilityModule, logosPackageManager }: 
        let
          common = import ./nix/default.nix { inherit pkgs logosLiblogos logosProtocolPkg logosQtHost; };
          src = ./.;
          app = import ./nix/app.nix { inherit pkgs common src logosLiblogos logosProtocolPkg logosQtHost; };
        in
        {
          app = app;
          default = app;
        }
      );

      devShells = forAllSystems ({ pkgs, logosLiblogos, logosProtocolPkg, logosQtHost, logosCapabilityModule, logosPackageManager }: {
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
            export LOGOS_PROTOCOL_ROOT="${logosProtocolPkg}"
            export LOGOS_QT_HOST_ROOT="${logosQtHost}"
            export LOGOS_CAPABILITY_MODULE_ROOT="${logosCapabilityModule}"
            export LOGOS_PACKAGE_MANAGER_ROOT="${logosPackageManager}"
            echo "Logos Module Viewer development environment"
            echo "LOGOS_LIBLOGOS_ROOT: $LOGOS_LIBLOGOS_ROOT"
            echo "Build with: nix build"
            echo "Run with: ./result/bin/logos-module-viewer"
          '';
        };
      });
    };
}
