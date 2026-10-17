#!/bin/bash
# Installerar R-744. Kör från Terminal:  bash installera.command
set -e
cd "$(dirname "$0")"

VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"

rm -rf "$VST3/R-744.vst3" "$AU/R-744.component"
cp -R R-744.vst3 "$VST3/"
cp -R R-744.component "$AU/"

# Pluginet är osignerat, så macOS spärrar det. Detta tar bort spärren.
xattr -dr com.apple.quarantine "$VST3/R-744.vst3" "$AU/R-744.component" R-744.app 2>/dev/null || true

echo ""
echo "Klart! R-744 ligger nu i:"
echo "  $VST3"
echo "Starta om Bitwig, eller tryck Rescan under Settings > Locations > Plug-ins."
