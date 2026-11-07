#!/bin/sh
set -eu
[ "$#" = 1 ] || { echo 'Usage: configure-library.sh /media/your-drive/SGK_Hyperion_Library' >&2; exit 2; }
case "$1" in /*) ;; *) echo 'Use an absolute library folder path' >&2; exit 2;; esac
[ -f "$1/catalog.tsv" ] && [ -d "$1/Patches" ] && [ -d "$1/Samples" ] || { echo 'Expected catalog.tsv, Patches and Samples' >&2; exit 1; }
root=$(CDPATH= cd -- "$1" && pwd -P)
plugin=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
mkdir -p "$plugin/User"
printf '%s\n' "$root" > "$plugin/User/library-path.txt.new"
mv "$plugin/User/library-path.txt.new" "$plugin/User/library-path.txt"
echo "Library configured: $root"
echo 'Reload the plugin instance to use this folder. No MPC restart is performed.'
