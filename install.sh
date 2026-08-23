#!/bin/sh
# ==============================================================
#   Caelestia KDE Port - Bootstrap installer
#
#   Clone (or update) the repo and hand off to setup.sh.
#   Install with a single command:
#
#     curl -fsSL https://raw.githubusercontent.com/ladybug-me/caelestia-dots-kde/main/install.sh | sh
#
#   Overridable via environment:
#     CAELESTIA_REPO    repository URL (default: ladybug-me/caelestia-dots-kde)
#     CAELESTIA_BRANCH  branch to install (default: main)
#     CAELESTIA_DIR     target directory (default: ~/caelestia-dots-kde)
# ==============================================================

set -eu

REPO="${CAELESTIA_REPO:-https://github.com/ladybug-me/caelestia-dots-kde.git}"
BRANCH="${CAELESTIA_BRANCH:-main}"
DEST="${CAELESTIA_DIR:-$HOME/caelestia-dots-kde}"

if ! command -v git >/dev/null 2>&1; then
    echo "[Caelestia] git is required but not installed." >&2
    exit 1
fi

# If run from an existing checkout (e.g. `sh install.sh` inside the repo),
# reuse it instead of cloning a fresh copy.
if [ -f "./scripts/setup.sh" ]; then
    if [ ! -t 0 ] && [ -c /dev/tty ]; then
        exec bash "$(pwd)/scripts/setup.sh" </dev/tty
    fi
    exec bash "$(pwd)/scripts/setup.sh"
fi

if [ -d "$DEST/.git" ]; then
    echo "[Caelestia] Updating existing checkout at $DEST"
    git -C "$DEST" pull --ff-only
elif [ -e "$DEST" ]; then
    echo "[Caelestia] $DEST already exists and is not a git checkout; aborting." >&2
    exit 1
else
    echo "[Caelestia] Cloning $REPO ($BRANCH) into $DEST"
    git clone -b "$BRANCH" --single-branch --depth 1 "$REPO" "$DEST"
fi

if [ ! -t 0 ] && [ -c /dev/tty ]; then
    exec bash "$DEST/scripts/setup.sh" </dev/tty
fi
exec bash "$DEST/scripts/setup.sh"
