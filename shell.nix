let
  sources = import ./npins;
  pkgs = import sources.nixpkgs { };
in

pkgs.mkShell.override {stdenv = pkgs.clangStdenv;} {
  nativeBuildInputs = with pkgs; [
    meson
    ninja
    pkg-config
    appstream
    desktop-file-utils
    wrapGAppsHook3
    (tartan.overrideAttrs { src = ~/Projects/tartan; })
    npins
    clang-analyzer
  ];

  buildInputs = with pkgs; [
    atk
    cairo
    evolution-data-server
    gcr_4
    glib
    gpgme
    gtk3
    gtksourceview4
    gtkspell3
    libcryptui
    sqlite
  ];
}
