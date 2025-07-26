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
    wrapGAppsHook4

    npins
  ];

  buildInputs = with pkgs; [
    cairo
    evolution-data-server
    gcr_4
    glib
    gpgme
    gtk4
    gtksourceview5
    sqlite
  ];
}
