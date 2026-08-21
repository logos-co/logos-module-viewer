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
    
    # Copy liblogos_core AND the shared runtime it now imports.
    #
    # liblogos_core used to be self-contained: it absorbed liblogos_protocol.a
    # and liblogos_qt_host.a whole and re-exported them. Since logos-liblogos#182
    # it IMPORTS those types instead, recording
    #
    #     @rpath/liblogos_protocol.dylib
    #     @rpath/liblogos_qt_host.dylib
    #
    # with @loader_path as its only rpath -- so the loader looks for them BESIDE
    # itself, in this directory, and nowhere else. Copying liblogos_core alone
    # produces a bundle that builds and installs and cannot be loaded, failing at
    # dyld time before main() with no diagnostic from the build.
    #
    # Copy every shared library liblogos ships rather than naming the two, so
    # this stays correct if the runtime is split further.
    _copied=0
    for f in "${logosLiblogos}/lib/"*.dylib "${logosLiblogos}/lib/"*.so "${logosLiblogos}/lib/"*.dll; do
      # A non-matching glob stays literal, so test before copying.
      if [ -f "$f" ]; then
        cp -L "$f" "$out/lib/" || true
        _copied=$((_copied + 1))
      fi
    done
    echo "Copied $_copied shared librar(y|ies) from liblogos to $out/lib/"

    # Assert rather than trust the loop: `ls ... || true` copying nothing is
    # exactly the shape that produced the unloadable bundle above, and it exits
    # 0 either way.
    if [ "$_copied" -eq 0 ]; then
      echo "ERROR: copied no shared libraries from ${logosLiblogos}/lib" >&2
      ls -la "${logosLiblogos}/lib" >&2 || true
      exit 1
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
