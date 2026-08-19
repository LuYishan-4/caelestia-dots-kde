#!/usr/bin/env bash
# installDP.sh - Arch package installation for Caelestia KDE Port

set -uo pipefail

log()  { echo -e "\033[0;36m[INFO]\033[0m $*"; }
err()  { echo -e "\033[0;31m[ERR]\033[0m  $*"; }

log "Installing Arch packages..."

INSTALL_FISH="${INSTALL_FISH:-true}"
INSTALL_PAPIRUS="${INSTALL_PAPIRUS:-true}"
INSTALL_DARKLY="${INSTALL_DARKLY:-true}"

# Ensure yay
if ! command -v yay >/dev/null 2>&1; then
    log "yay not found - installing..."
    sudo pacman -S --needed --noconfirm base-devel git || true
    tmpdir="$(mktemp -d)"
    git clone --depth 1 https://aur.archlinux.org/yay-bin.git "$tmpdir"
    (
        cd "$tmpdir" || exit 1
        makepkg -si --noconfirm
    )
    rm -rf "$tmpdir"
fi

# Core dependencies split by group — controlled via PACKAGE_GROUP env var
PACKAGE_GROUP="${PACKAGE_GROUP:-all}"

CORE_PACKAGES=(
    cmake ninja ccache
    wl-clipboard cliphist wl-clip-persist inotify-tools app2unit wireplumber trash-cli jq aubio lm_sensors
    libpipewire glibc qt6-declarative gcc-libs qt6-base qt6-declarative qt6-wayland libqalculate kpipewire kglobalaccel kglobalacceld libsecret
    networkmanager-qt vulkan-headers
    ffmpeg
)

SHELL_PACKAGES=(
    caelestia-cli quickshell
    foot eza fastfetch starship btop bash
)

THEME_PACKAGES=(
    adw-gtk-theme ttf-jetbrains-mono-nerd ttf-material-symbols-variable ttf-rubik-vf ttf-cascadia-code-nerd
    noto-fonts noto-fonts-cjk noto-fonts-emoji
)

UTILITY_PACKAGES=(
    swappy ddcutil networkmanager imagemagick tesseract tesseract-data-eng satty spectacle xdg-utils sassc bat ripgrep lazygit xdg-user-dirs
)

# Build final package list based on selected group
PACKAGES=()
case "$PACKAGE_GROUP" in
    core)   PACKAGES=("${CORE_PACKAGES[@]}") ;;
    shell)  PACKAGES=("${SHELL_PACKAGES[@]}") ;;
    themes) PACKAGES=("${THEME_PACKAGES[@]}") ;;
    utils)  PACKAGES=("${UTILITY_PACKAGES[@]}") ;;
    all|*)  PACKAGES=("${CORE_PACKAGES[@]}" "${SHELL_PACKAGES[@]}" "${THEME_PACKAGES[@]}" "${UTILITY_PACKAGES[@]}") ;;
esac

if [[ "$PACKAGE_GROUP" == "all" || "$PACKAGE_GROUP" == "shell" ]]; then
    if [[ "$INSTALL_FISH" == "true" ]]; then
        PACKAGES+=(fish)
    else
        log "Skipping Fish installation by user choice."
    fi
fi

if [[ "$PACKAGE_GROUP" == "all" || "$PACKAGE_GROUP" == "themes" ]]; then
    if [[ "$INSTALL_PAPIRUS" == "true" ]]; then
        PACKAGES+=(papirus-icon-theme)
    else
        log "Skipping Papirus icon theme installation by user choice."
    fi
fi

# libcava and darkly are the last packages that would compile from source.
# They are prebuilt by CI into a binary repo hosted on GitHub Releases (see
# .github/workflows/prebuilt-artifacts.yml); fall back to AUR source builds
# when the repo is unreachable or a package is missing from it.
PREBUILT_PKGS=()
if [[ "$PACKAGE_GROUP" == "all" || "$PACKAGE_GROUP" == "core" ]]; then
    PREBUILT_PKGS+=(libcava)
fi
if [[ "$PACKAGE_GROUP" == "all" || "$PACKAGE_GROUP" == "themes" ]]; then
    if [[ "$INSTALL_DARKLY" == "true" ]]; then
        PREBUILT_PKGS+=(darkly)
    else
        log "Skipping Darkly package installation by user choice."
    fi
fi

BIN_REPO_NAME="caelestia-bin"
BIN_REPO_URL="https://github.com/ladybug-me/caelestia-dots-kde/releases/download/caelestia-bin-repo"

install_from_binary_repo() {
    if ! grep -q "^\[$BIN_REPO_NAME\]" /etc/pacman.conf 2>/dev/null; then
        {
            echo ""
            echo "[$BIN_REPO_NAME]"
            echo "SigLevel = Optional"
            echo "Server = $BIN_REPO_URL"
            echo ""
        } | sudo tee -a /etc/pacman.conf >/dev/null
    fi
    sudo pacman -Sy --noconfirm >/dev/null 2>&1
}

if [[ ${#PREBUILT_PKGS[@]} -gt 0 ]] && [[ -z "${CAELESTIA_SKIP_BINARY_REPO:-}" ]]; then
    if install_from_binary_repo; then
        for pkg in "${PREBUILT_PKGS[@]}"; do
            if sudo pacman -S --needed --noconfirm "$pkg" >/dev/null 2>&1; then
                log "Installed $pkg from the prebuilt repo."
            else
                log "Prebuilt $pkg unavailable; will build from the AUR."
                PACKAGES+=("$pkg")
            fi
        done
    else
        log "Prebuilt repo unreachable; building from the AUR instead."
        PACKAGES+=("${PREBUILT_PKGS[@]}")
        sudo sed -i "/^\[$BIN_REPO_NAME\]/,/^$/d" /etc/pacman.conf
    fi
elif [[ ${#PREBUILT_PKGS[@]} -gt 0 ]]; then
    PACKAGES+=("${PREBUILT_PKGS[@]}")
fi

log "Installing packages (group: $PACKAGE_GROUP)..."
FAILED_PKGS=()

# Batch install all packages at once — much faster than individual yay calls
if ! yay -S --needed --noconfirm "${PACKAGES[@]}"; then
    log "Batch install had failures. Retrying individually..."
    for pkg in "${PACKAGES[@]}"; do
        # Skip packages already installed by the batch attempt
        if pacman -Q "$pkg" >/dev/null 2>&1; then
            continue
        fi
        if ! yay -S --needed --noconfirm "$pkg"; then
            log "yay failed to install $pkg. Attempting manual build from AUR..."
            tmpdir="$(mktemp -d)"
            if git clone --depth 1 "https://aur.archlinux.org/${pkg}.git" "$tmpdir"; then
                (
                    cd "$tmpdir" || exit 1
                    makepkg -si --noconfirm
                ) || {
                    err "Manual build for $pkg failed."
                    FAILED_PKGS+=("$pkg")
                }
            else
                err "Could not find AUR repository for $pkg."
                FAILED_PKGS+=("$pkg")
            fi
            rm -rf "$tmpdir"
        fi
    done
fi

if [ ${#FAILED_PKGS[@]} -ne 0 ]; then
    mkdir -p "${XDG_CACHE_HOME:-$HOME/.cache}/caelestia-kde"
    err "The following packages could not be installed:"
    for pkg in "${FAILED_PKGS[@]}"; do
        err "  - $pkg"
        echo "$pkg" >> "${XDG_CACHE_HOME:-$HOME/.cache}/caelestia-kde/failed_packages.txt"
    done
fi

if command -v xdg-user-dirs-update >/dev/null 2>&1; then
    xdg-user-dirs-update || true
fi

if command -v sassc >/dev/null 2>&1 && ! command -v sass >/dev/null 2>&1; then
    sudo ln -sf /usr/bin/sassc /usr/local/bin/sass || true
fi

log "Arch package installation complete."
