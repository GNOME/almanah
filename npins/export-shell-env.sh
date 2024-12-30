#!/bin/sh

set -o errexit

DIRENV=$(nix-build npins/nixpkgs.nix -A direnv --no-out-link)/bin/direnv

$DIRENV allow
$DIRENV export bash

# nix-shell sets this to `/tmp/nix-shell-xxx-x`
echo TMPDIR=/tmp
