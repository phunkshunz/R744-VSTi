#!/usr/bin/env bash
# Installerar pluginen i den här mappen för din användare:
#   VST3 -> ~/.vst3   LV2 -> ~/.lv2
set -e
cd "$(dirname "$0")"
mkdir -p "$HOME/.vst3" "$HOME/.lv2"
shopt -s nullglob
for p in *.vst3; do
  rm -rf "$HOME/.vst3/$p"
  cp -R "$p" "$HOME/.vst3/"
  echo "VST3: $p -> ~/.vst3"
done
for p in *.lv2; do
  rm -rf "$HOME/.lv2/$p"
  cp -R "$p" "$HOME/.lv2/"
  echo "LV2:  $p -> ~/.lv2"
done
echo "Klart. Starta om din DAW och låt den söka efter nya plugins."
