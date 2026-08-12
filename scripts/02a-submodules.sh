#!/usr/bin/env bash
# 02a-submodules.sh - Initialize git submodules

set -euo pipefail

# Initialize submodules
if [[ -f "$BUNDLE_DIR/.gitmodules" ]]; then
    echo "[INFO]  Initializing submodules..."
    git submodule sync --recursive >/dev/null 2>&1 || true
    git submodule update --init --recursive --force >/dev/null 2>&1 || echo "[FAIL]  Failed to initialize all submodules." >&2

    # Pin plasma-wallpaper-application to the tagged release.
    WALLPAPER_DIR="$BUNDLE_DIR/src/plasma-wallpaper-application"
    WALLPAPER_TAG="v1.2"
    if [[ -e "$WALLPAPER_DIR/.git" ]]; then
        echo "[INFO]  Pinning plasma-wallpaper-application to tag $WALLPAPER_TAG..."
        git -C "$WALLPAPER_DIR" fetch --tags --quiet 2>/dev/null || true
        git -C "$WALLPAPER_DIR" checkout "tags/$WALLPAPER_TAG" --quiet 2>/dev/null || \
            echo "[WARN]  Could not checkout tag $WALLPAPER_TAG for plasma-wallpaper-application; using current HEAD."
    fi
fi