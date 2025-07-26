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
    cppcheck
    desktop-file-utils
    wrapGAppsHook4
    clang-tools

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
