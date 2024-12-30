let
  sources = import ./npins;
  pkgs = import sources.nixpkgs { };
in

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    meson
    ninja
    pkg-config
    appstream
    desktop-file-utils
    wrapGAppsHook3

    npins
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
