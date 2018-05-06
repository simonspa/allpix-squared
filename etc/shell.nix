# A user with Nix [0] installed can with this file enter a build environment with
# all dependencies compiled and installed locally with a call to nix-shell(1):
#
#     nix-shell [--pure] etc/shell.nix
#
# Dependencies to build (both code and documentation) and check the syntax with
# clang-tidy are all installed.
#
# Datasets for geant4 (g4data) are downloaded in the buildPhase instead of directly
# via some fetchTarball so this environment will not build in a Nix sandbox.
#
# [0] https://nixos.org/nix/

with import <nixpkgs> {};

let
  coin = stdenv.mkDerivation rec {
    name = "coin-${version}";
    version = "3.1.3";

    src = fetchurl {
      url = "https://bitbucket.org/Coin3D/coin/downloads/Coin-${version}.tar.gz";
      sha256 = "583478c581317862aa03a19f14c527c3888478a06284b9a46a0155fa5886d417";
    };

    patches = writeTextFile {
     name = "fixed-wrong-assignment.patch";
     text = ''
       # HG changeset patch
       # User Giampiero Gabbiani <giampiero@gabbiani.org>
       # Date 1504091210 -7200
       # Branch fedora26-adaptations
       # Node ID 6008c4bd6ba67387bffde6b2b23e7aa01f137e9c
       # Parent  fbd6ca1c14368c388e177f5a7103378dd22b4296
       fixed wrong assignment to buffer pointer

       diff --git a/src/3ds/SoStream.cpp b/src/3ds/SoStream.cpp
       --- a/src/3ds/SoStream.cpp
       +++ b/src/3ds/SoStream.cpp
       @@ -185,7 +185,7 @@
          if (!gotNum) { setBadBit(); return FALSE; } \
         \
          char *ce; \
       -  s = '\0'; \
       +  *s = '\0'; \
          _convertType_ tempVal = _convertFunc_(buf, &ce, 0); \
         \
          if (ce != s) \
       @@ -282,7 +282,7 @@
        gotAll: \
          \
          char *ce; \
       -  s = '\0'; \
       +  *s = '\0'; \
          double tempVal = _convertFunc_(buf, &ce); \
         \
          if (ce != s) \
     '';
    };

    postUnpack = ''
      # fix compilation
      sed -i '/^#include "fonts\/freetype.h"$/i #include <cstdlib>\n#include <cmath>' $sourceRoot/src/fonts/freetype.cpp

      # fix http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=667139
      sed -i '/^#include <Inventor\/C\/basic.h>$/i #include <Inventor/C/errors/debugerror.h>' $sourceRoot/include/Inventor/SbBasic.h
    '';

    buildInputs = [ expat ];

    propagatedBuildInputs = [ libGLU ];
  };

  soxt = stdenv.mkDerivation rec {
    name = "soxt-${version}";
    version = "1.3.0";

    src = fetchurl {
      url = "https://bitbucket.org/Coin3D/coin/downloads/SoXt-${version}.tar.gz";
      sha256= "f5443aadafe8e2222b9b5a23d1f228bb0f3e7d98949b8ea8676171b7ea5bf013";
    };

    buildInputs = [ coin motif ];

    propagatedBuildInputs = [ xlibsWrapper coin ];
  };

  geant4 = stdenv.mkDerivation rec {
    name = "geant4-${version}";
    version = "10.4.1";

    src = fetchurl{
      url = "http://cern.ch/geant4-data/releases/geant4.10.04.p01.tar.gz";
      sha256 = "a3eb13e4f1217737b842d3869dc5b1fb978f761113e74bd4eaf6017307d234dd";
    };

    g4data = installData {
      inherit version src;
    };

    enableParallelBuilding = true;
    propagatedBuildInputs =
      [ g4data cmake expat zlib xercesc qt5.qtbase motif libGLU_combined xlibsWrapper xorg.libXmu soxt xorg.libXpm.dev ];

    # The data directory holds not just interaction cross section data, but other
    # files which the installer needs to write, so we link to the previously installed
    # data instead. This assumes the default data installation location of $out/share.
    preConfigure = ''
      mkdir -p $out/share/Geant4-${version}
      ln -s ${g4data}/Geant4-${version}/data $out/share/Geant4-${version}/data
    '';

    shellHook = ''
      source $out/nix-support/setup-hook
    '';

    setupHook = writeTextFile {
      name = "setup-hook.sh";
      text = "source @out@/bin/geant4.sh";
    };


    cmakeFlags = ''
      -DGEANT4_INSTALL_DATA=OFF
      -DGEANT4_BUILD_MULTITHREADED=ON
      -DGEANT4_BUILD_CXXSTD=14
      -DGEANT4_INSTALL_DATA=OFF
      -DGEANT4_USE_GDML=ON
      -DGEANT4_USE_G3TOG4=ON
      -DGEANT4_USE_QT=ON
      -DGEANT4_USE_XM=ON
      -DGEANT4_USE_OPENGL_X11=ON
      -DGEANT4_USE_INVENTOR=ON
      -DGEANT4_USE_RAYTRACER_X11=ON
      -DGEANT4_USE_SYSTEM_CLHEP=OFF
      -DGEANT4_USE_SYSTEM_EXPAT=ON
      -DGEANT4_USE_SYSTEM_ZLIB=ON
      -DINVENTOR_INCLUDE_DIR=${coin}/include
      -DINVENTOR_LIBRARY_RELEASE=${coin}/lib/libCoin.so
    '';
  };

  installData = { version, src }:
    stdenv.mkDerivation rec {
      inherit version src;
      name = "g4data-${version}";

      buildInputs = [ cmake expat ];

      cmakeFlags = ''
        -DGEANT4_INSTALL_DATA="ON"
      '';

      enableParallelBuilding = true;
      buildPhase = ''
        make G4EMLOW G4NDL G4NEUTRONXS G4PII G4SAIDDATA G4ABLA G4ENSDFSTATE PhotonEvaporation RadioactiveDecay RealSurface
      '';

      installPhase = ''
        mkdir -p $out/Geant4-${version}
        cp -R data/ $out/Geant4-${version}
      '';
    };
in
  stdenv.mkDerivation {
    name = "allpix2-buildenv";
    buildInputs = [
      geant4
      root
      eigen3_3
      cmake

      clang-tools

      # Documentation dependencies
      doxygen
      texlive.combined.scheme-basic
      pandoc
      biber
      imagemagick
      ghostscript
      poppler_utils
    ];
  }
