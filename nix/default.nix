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
