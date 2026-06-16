{
  description = "Logos Module Viewer - a Qt UI application for inspecting Logos modules";

  inputs = {
    nixpkgs.follows = "logos-liblogos/nixpkgs";
    logos-liblogos.url = "github:logos-co/logos-liblogos";
    # Introspection library (ModuleLib::LogosModule) — reads a plugin's methods
    # and metadata for both legacy and universal/cdylib provider plugins.
    logos-module.url = "github:logos-co/logos-module";
    # Sample module used by the doctest check (a real load + IPC round-trip).
    accounts-module.url = "github:logos-co/logos-accounts-module";
    # QObject-tree inspector — embedded in the GUI so headless ui_test doc-tests
    # can drive the app and capture screenshots.
    logos-qt-mcp.url = "github:logos-co/logos-qt-mcp";
  };

  outputs = { self, nixpkgs, logos-liblogos, logos-module, accounts-module, logos-qt-mcp }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        inherit system;
        pkgs = import nixpkgs { inherit system; };
        liblogos = logos-liblogos.packages.${system}.logos-liblogos;
        logosModule = logos-module.packages.${system}.lib;
        accountsInstall = accounts-module.packages.${system}.install;
        qtMcp = logos-qt-mcp.packages.${system}.default;
      });

      version = "1.0.0";
      pname = "logos-module-viewer";
    in
    {
      packages = forAllSystems ({ pkgs, system, liblogos, logosModule, accountsInstall, qtMcp }:
        let
          meta = with pkgs.lib; {
            description = "Logos Module Viewer - inspect and exercise Logos module plugins";
            platforms = platforms.unix;
          };

          # Compile the Qt Widgets binary against liblogos + logos-module, with the
          # logos-qt-mcp QObject inspector embedded (for headless ui_test).
          build = pkgs.stdenv.mkDerivation {
            pname = "${pname}-build";
            inherit version meta;
            src = ./.;

            dontWrapQtApps = true;

            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
              pkgs.pkg-config
              pkgs.qt6.wrapQtAppsHook
            ];

            buildInputs = [
              pkgs.qt6.qtbase
              pkgs.qt6.qtremoteobjects
              pkgs.nlohmann_json
              # The inspector lib (qml-inspector) links these Qt components.
              pkgs.qt6.qtdeclarative
            ];

            configurePhase = ''
              runHook preConfigure

              # The inspector is built via add_subdirectory(${"\${LOGOS_QT_MCP_ROOT}"}/qt-plugin),
              # which writes a CMakeCache into the source tree — so copy qt-mcp out of
              # the read-only nix store into a writable dir first.
              cp -r ${qtMcp} ./qt-mcp
              chmod -R +w ./qt-mcp

              cmake -S app -B build \
                -GNinja \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
                -DLOGOS_LIBLOGOS_ROOT=${liblogos} \
                -DLOGOS_MODULE_ROOT=${logosModule} \
                -DENABLE_QML_INSPECTOR=ON \
                -DLOGOS_QT_MCP_ROOT=$(pwd)/qt-mcp
              runHook postConfigure
            '';

            buildPhase = ''
              runHook preBuild
              cmake --build build
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall
              mkdir -p $out/bin
              cp build/bin/logos-module-viewer $out/bin/
              runHook postInstall
            '';
          };

          # Wrap the binary: bundle liblogos_core, logos_host, and a modules dir
          # (capability_module from liblogos). Set LOGOS_HOST_PATH so the runtime
          # can spawn the per-module host process.
          app = pkgs.stdenvNoCC.mkDerivation {
            inherit pname version meta;
            dontUnpack = true;

            nativeBuildInputs =
              [ pkgs.qt6.wrapQtAppsHook ]
              ++ pkgs.lib.optionals pkgs.stdenv.isDarwin [ pkgs.darwin.cctools ]
              ++ pkgs.lib.optionals pkgs.stdenv.isLinux [ pkgs.autoPatchelfHook ];

            buildInputs = [
              pkgs.qt6.qtbase
              pkgs.qt6.qtremoteobjects
              liblogos
            ];

            qtWrapperArgs = [
              "--set" "LOGOS_HOST_PATH" "${liblogos}/bin/logos_host"
            ];

            installPhase = ''
              runHook preInstall
              mkdir -p $out/bin $out/lib $out/modules

              cp ${build}/bin/logos-module-viewer $out/bin/
              chmod -R +w $out/bin

              # All of liblogos's libs — liblogos_core plus its siblings
              # (e.g. libpackage_manager_lib), which liblogos_core depends on via
              # @rpath. Copy them all so those references resolve next to
              # liblogos_core in the bundle; copying only liblogos_core breaks the
              # @rpath lookup at load time (fatal on macOS). Glob each extension
              # independently so a non-matching pattern (e.g. *.dylib on Linux)
              # doesn't abort the copy.
              for ext in so dylib; do
                cp -L ${liblogos}/lib/*.$ext $out/lib/ 2>/dev/null || true
              done
              chmod -R +w $out/lib

              # logos_host (+ qt variant) for per-module process isolation
              cp -L ${liblogos}/bin/logos_host* $out/bin/ 2>/dev/null || true

              # Built-in modules shipped with liblogos (capability_module, ...)
              if [ -d ${liblogos}/modules ]; then
                cp -r ${liblogos}/modules/. $out/modules/ 2>/dev/null || true
                chmod -R +w $out/modules
              fi

              ${pkgs.lib.optionalString pkgs.stdenv.isDarwin ''
                # On macOS, liblogos_core.dylib loads sibling libs (e.g.
                # libpackage_manager_lib) via @rpath. dyld resolves @rpath using the
                # LC_RPATH entries of the loading binary chain, so add the bundle's
                # lib dir as an rpath on the executable — the canonical fix.
                install_name_tool -add_rpath "$out/lib" \
                  "$out/bin/logos-module-viewer" 2>/dev/null || true

                # Belt-and-suspenders: also pin the install id of each bundled dylib
                # and rewrite cross-references to absolute bundle paths, so the libs
                # resolve each other regardless of which binary loaded them.
                for dylib in $out/lib/*.dylib; do
                  [ -f "$dylib" ] || continue
                  libname=$(basename "$dylib")
                  install_name_tool -id "$out/lib/$libname" "$dylib" 2>/dev/null || true
                  for other in $out/lib/*.dylib; do
                    othername=$(basename "$other")
                    install_name_tool -change "@rpath/$othername" "$out/lib/$othername" \
                      "$dylib" 2>/dev/null || true
                  done
                  install_name_tool -change "@rpath/$libname" "$out/lib/$libname" \
                    "$out/bin/logos-module-viewer" 2>/dev/null || true
                done
              ''}

              runHook postInstall
            '';
          };
        in
        {
          inherit app;
          default = app;
        }
      );

      checks = forAllSystems ({ pkgs, system, liblogos, accountsInstall, ... }:
        let
          appPkg = self.packages.${system}.app;

          # The accounts_module install tree is the doctest's sample module: a
          # `modules/accounts_module/{accounts_module_plugin.so,manifest.json,variant}`
          # layout that liblogos discovery can scan directly.
          testModules = "${accountsInstall}/modules";
          accountsPlugin =
            "${testModules}/accounts_module/accounts_module_plugin.so";
        in
        {
          # Headless self-test: prove the modernized viewer introspects a real
          # universal module and completes a live IPC round-trip against it.
          doctest = pkgs.runCommand "${pname}-doctest"
            {
              nativeBuildInputs = [ appPkg ]
                ++ pkgs.lib.optionals pkgs.stdenv.isLinux [ pkgs.qt6.qtbase ];
            }
            ''
              export QT_QPA_PLATFORM=offscreen
              export QT_FORCE_STDERR_LOGGING=1
              ${pkgs.lib.optionalString pkgs.stdenv.isLinux ''
                export QT_PLUGIN_PATH="${pkgs.qt6.qtbase}/${pkgs.qt6.qtbase.qtPluginPrefix}"
              ''}
              export LOGOS_HOST_PATH=${liblogos}/bin/logos_host

              VIEWER=${appPkg}/bin/logos-module-viewer
              PLUGIN=${accountsPlugin}
              MODULES=${testModules}

              echo "== 1. introspect accounts_module (--list-methods) =="
              "$VIEWER" --module "$PLUGIN" --list-methods | tee methods.txt
              grep -q "createRandomMnemonic" methods.txt
              grep -q "lengthToEntropyStrength" methods.txt

              echo "== 2. live IPC round-trip: lengthToEntropyStrength(12) == 128 =="
              "$VIEWER" --module "$PLUGIN" --modules-dir "$MODULES" \
                --call "lengthToEntropyStrength(12)" | tee call.txt
              grep -q "128" call.txt

              echo "== 3. round-trip a string method: createRandomMnemonic(12) =="
              "$VIEWER" --module "$PLUGIN" --modules-dir "$MODULES" \
                --call "createRandomMnemonic(12)" | tee mnemonic.txt
              grep -q "result" mnemonic.txt

              mkdir -p $out
              cp methods.txt call.txt mnemonic.txt $out/
              echo "doctest passed"
            '';
        }
      );

      devShells = forAllSystems ({ pkgs, liblogos, logosModule, ... }: {
        default = pkgs.mkShell {
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
          ];
          buildInputs = [
            pkgs.qt6.qtbase
            pkgs.qt6.qtremoteobjects
            pkgs.nlohmann_json
          ];
          shellHook = ''
            export LOGOS_LIBLOGOS_ROOT="${liblogos}"
            export LOGOS_MODULE_ROOT="${logosModule}"
            export LOGOS_HOST_PATH="${liblogos}/bin/logos_host"
            echo "Logos Module Viewer dev shell"
            echo "  configure: cmake -S app -B build -GNinja -DLOGOS_LIBLOGOS_ROOT=\$LOGOS_LIBLOGOS_ROOT -DLOGOS_MODULE_ROOT=\$LOGOS_MODULE_ROOT"
            echo "  build:     cmake --build build"
            echo "  run:       ./build/bin/logos-module-viewer -m <plugin.so>"
          '';
        };
      });
    };
}
