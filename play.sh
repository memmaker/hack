#!/bin/sh
# Hack (restoHack, X11 tiles). ./play.sh [nethack] picks the NetHack tile set
# (default DawnLike). Saves, record: save/ (seeded from build/hackdir).
cd "$(dirname "$0")"
export XAUTHORITY="${XAUTHORITY:-$HOME/.Xauthority}" TERM=vt100
export HACK_TILESET="${1:-${HACK_TILESET:-dawn}}"
[ -d save ] || cp -R build/hackdir save
exec build/hack
