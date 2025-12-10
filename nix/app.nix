{ pkgs, common, src, logosLiblogos, logosSdk, logosCapabilityModule, logosPackageManager }:

pkgs.stdenv.mkDerivation rec {
  pname = "logos-module-viewer";
  version = common.version;
  
  inherit src;
  inherit (common) buildInputs cmakeFlags meta;
  
  nativeBuildInputs = common.nativeBuildInputs;
  
  qtLibPath = pkgs.lib.makeLibraryPath (
    [
      pkgs.qt6.qtbase
      pkgs.qt6.qtremoteobjects
      pkgs.zstd
      pkgs.zlib
      pkgs.glib
      pkgs.stdenv.cc.cc
      pkgs.freetype
      pkgs.fontconfig
    ]
    ++ pkgs.lib.optionals pkgs.stdenv.isLinux [
      pkgs.libglvnd
      pkgs.mesa.drivers
      pkgs.xorg.libX11
      pkgs.xorg.libXext
      pkgs.xorg.libXrender
      pkgs.xorg.libXrandr
      pkgs.xorg.libXcursor
      pkgs.xorg.libXi
      pkgs.xorg.libXfixes
      pkgs.xorg.libxcb
    ]
  );
  qtPluginPath = "${pkgs.qt6.qtbase}/lib/qt-6/plugins";
  
  dontWrapQtApps = false;
  
  qtWrapperArgs = [
    "--prefix" "LD_LIBRARY_PATH" ":" qtLibPath
    "--prefix" "QT_PLUGIN_PATH" ":" qtPluginPath
  ];
  
  configurePhase = ''
    runHook preConfigure
    
    echo "logosLiblogos: ${logosLiblogos}"
    echo "logosSdk: ${logosSdk}"
    echo "logosCapabilityModule: ${logosCapabilityModule}"
    echo "logosPackageManager: ${logosPackageManager}"
    
    cmake -S app -B build \
      -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
      -DLOGOS_LIBLOGOS_ROOT=${logosLiblogos} \
      -DLOGOS_CPP_SDK_ROOT=${logosSdk}
    
    runHook postConfigure
  '';
  
  buildPhase = ''
    runHook preBuild
    
    cmake --build build
    
    runHook postBuild
  '';
  
  installPhase = ''
    runHook preInstall
    
    mkdir -p $out/bin $out/lib $out/modules
    cp build/bin/logos-module-viewer "$out/bin/"
    
    # Copy liblogos_core library
    if ls "${logosLiblogos}/lib/"liblogos_core.* >/dev/null 2>&1; then
      cp -L "${logosLiblogos}/lib/"liblogos_core.* "$out/lib/" || true
      echo "Copied liblogos_core to $out/lib/"
    fi
    
    # Copy logos_sdk library
    if ls "${logosSdk}/lib/"liblogos_sdk.* >/dev/null 2>&1; then
      cp -L "${logosSdk}/lib/"liblogos_sdk.* "$out/lib/" || true
      echo "Copied liblogos_sdk to $out/lib/"
    fi
    
    # Copy logos_host binary (needed for remote mode)
    if [ -f "${logosLiblogos}/bin/logos_host" ]; then
      cp -L "${logosLiblogos}/bin/logos_host" "$out/bin/"
      echo "Copied logos_host to $out/bin/"
    fi
    
    # Determine platform-specific plugin extension
    OS_EXT="so"
    case "$(uname -s)" in
      Darwin) OS_EXT="dylib";;
      Linux) OS_EXT="so";;
      MINGW*|MSYS*|CYGWIN*) OS_EXT="dll";;
    esac
    
    # Copy module plugins into the modules directory
    if [ -f "${logosCapabilityModule}/lib/capability_module_plugin.$OS_EXT" ]; then
      cp -L "${logosCapabilityModule}/lib/capability_module_plugin.$OS_EXT" "$out/modules/"
      echo "Copied capability_module_plugin.$OS_EXT to $out/modules/"
    fi
    if [ -f "${logosPackageManager}/lib/package_manager_plugin.$OS_EXT" ]; then
      cp -L "${logosPackageManager}/lib/package_manager_plugin.$OS_EXT" "$out/modules/"
      echo "Copied package_manager_plugin.$OS_EXT to $out/modules/"
    fi
    
    runHook postInstall
  '';
}
