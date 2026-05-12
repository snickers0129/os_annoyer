#!/usr/bin/env bash
# Copy the renamed sound files to the SD card in strict numerical order
# so the DFPlayer's directory index matches the 3-digit prefix.
#
# Usage:
#   ./copy_to_sd.sh            # uses /Volumes/DISK_IMG
#   ./copy_to_sd.sh "MY_CARD"  # uses /Volumes/MY_CARD

set -euo pipefail

CARD_NAME="${1:-DISK_IMG}"
SD="/Volumes/$CARD_NAME"
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/sounds"

if [ ! -d "$SD" ]; then
  echo "Error: $SD not mounted. Plug in the SD card and try again."
  echo "Mounted volumes:"
  ls /Volumes/
  exit 1
fi

if [ ! -d "$SRC" ]; then
  echo "Error: sounds folder not found at $SRC"
  exit 1
fi

echo "Source: $SRC"
echo "Target: $SD"
echo

# Disable Spotlight on the volume so it doesn't re-create indexes mid-copy
sudo mdutil -i off "$SD" 2>/dev/null || true
sudo touch "$SD/.metadata_never_index" 2>/dev/null || true

# Copy each folder one file at a time, in sorted numerical order.
# `sleep 0.05` gives the OS a moment to flush each file before the next.
for folder in 01 02 03 07; do
  echo "=== /$folder/ ==="

  # Wipe the target folder on the card so the directory is clean
  rm -rf "$SD/$folder"
  mkdir -p "$SD/$folder"

  # Copy files one at a time, sorted by their 3-digit prefix
  count=0
  for file in $(ls "$SRC/$folder"/*.mp3 2>/dev/null | sort); do
    name="$(basename "$file")"
    printf "  %s ... " "$name"
    cp "$file" "$SD/$folder/"
    sleep 0.05
    sync   # flush filesystem buffers after each file
    echo "ok"
    count=$((count + 1))
  done
  echo "  copied $count files"
  echo
done

# Strip the macOS metadata that just got created.
# dot_clean -m removes ._files (the -m flag is "remove" not "merge").
echo "Cleaning macOS metadata..."
dot_clean -m "$SD" || true
sudo find "$SD" -name "._*" -delete 2>/dev/null || true
sudo find "$SD" -name ".DS_Store" -delete 2>/dev/null || true
sudo rm -rf "$SD/.Spotlight-V100" 2>/dev/null || true
sudo rm -rf "$SD/.Trashes" 2>/dev/null || true
sudo rm -rf "$SD/.fseventsd" 2>/dev/null || true
sync

echo
echo "Verifying directory order on the card:"
for target in 01 02 03 07; do
  echo "  /$target/ (first 3):"
  ls "$SD/$target" 2>/dev/null | head -3 | sed 's/^/    /'
done

echo
echo "Done. Eject the card with: diskutil eject \"$SD\""
