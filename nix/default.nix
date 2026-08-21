{ pkgs, logosLiblogos, logosSdk }:

{
  pname = "logos-module-viewer";
  version = "1.0.0";
  
  nativeBuildInputs = [ 
    pkgs.cmake 
    pkgs.ninja 
    pkgs.pkg-config
    pkgs.qt6.wrapQtAppsHook
  ];
  
  buildInputs = [ 
    pkgs.qt6.qtbase
    pkgs.qt6.qtremoteobjects
    # liblogos re-exports the Qt host runtime headers, and two of them --
    # logos_provider_object.h and logos_qt_arg_decode.h -- include
    # <nlohmann/json.hpp>. Header-only, so this adds an include path and nothing
    # to the link line.
    #
    # It has to be listed HERE rather than inherited. Consumers that take
    # liblogos as a buildInput get nlohmann through its propagatedBuildInputs;
    # this repo does not -- logosLiblogos arrives as a bare attribute and is used
    # as ''${logosLiblogos}/include via string interpolation, which participates
    # in no dependency propagation at all. Measured: adding the propagation to
    # liblogos (both the headers output and the symlinkJoin) did not reach this
    # build, because there is no dependency edge for it to travel along.
    #
    # Without it the build stops at
    #     fatal error: nlohmann/json.hpp: No such file or directory
    # in a repo that never mentions nlohmann, and only once liblogos is bumped
    # past logos-liblogos#182 -- before that liblogos_core was self-contained
    # and its headers did not reach logos-protocol's.
    pkgs.nlohmann_json
  ];
  
  cmakeFlags = [ 
    "-GNinja"
  ];
  
  logosLiblogos = logosLiblogos;
  logosSdk = logosSdk;
  
  meta = with pkgs.lib; {
    description = "Logos Module Viewer - A Qt UI application for viewing Logos modules";
    platforms = platforms.unix;
  };
}
