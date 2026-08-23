#!/usr/bin/env bash
# ==============================================================
#   Caelestia KDE Port - Unified Installer
#
#   Original Hyprland dots: Caelestia
#   KDE port and modifications: ladybug-me
#   Installer behavior: idempotent and safe for reruns
# ==============================================================

set -euo pipefail
export CAELESTIA_SETUP_RUNNING=1

# Hide cursor immediately for cleaner output
tput civis 2>/dev/null || true

# -- Paths ---------------------------------------------------------------------
BUNDLE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCRIPTS_DIR="$BUNDLE_DIR/scripts"
export BUNDLE_DIR
export INSTALL_START_EPOCH="$(date +%s)"

# Prevent concurrent runs.
if [[ "${CAELESTIA_TMUX_MASTER:-0}" == "0" ]]; then
    exec 9>"${XDG_RUNTIME_DIR:-/tmp}/caelestia-setup.lock"
    flock -n 9 || { echo "Another Caelestia setup is already running."; exit 1; }
fi

detect_base_distro() {
    local detected="unknown"

    if [[ -f /etc/os-release ]]; then
       # shellcheck disable=SC1091
        . /etc/os-release
        case "$ID" in
            arch|cachyos|endeavouros|manjaro|artix)
                detected="arch"
                ;;
            fedora|nobara|bazzite|rhel|centos|almalinux|rocky)
                detected="fedora"
                ;;
            debian|ubuntu|pop|mint|kali|raspbian|elementary|zorin|deepin|devuan)
                detected="debian"
                ;;
            *)
                if echo "${ID_LIKE:-}" | grep -iq "arch"; then
                    detected="arch"
                elif echo "${ID_LIKE:-}" | grep -iq "fedora"; then
                    detected="fedora"
                elif echo "${ID_LIKE:-}" | grep -iq -E "debian|ubuntu"; then
                    detected="debian"
                fi
                ;;
        esac
    fi

    if [[ "$detected" == "unknown" ]]; then
        if command -v pacman >/dev/null 2>&1; then
            detected="arch"
        elif command -v dnf >/dev/null 2>&1; then
            detected="fedora"
        elif command -v apt-get >/dev/null 2>&1; then
            detected="debian"
        fi
    fi

    echo "$detected"
}

is_cachyos() {
    local os_id=""
    local os_like=""

    if [[ -r /etc/os-release ]]; then
        # shellcheck disable=SC1091
        . /etc/os-release
        os_id="${ID:-}"
        os_like="${ID_LIKE:-}"
    fi

    [[ "$os_id" == "cachyos" || " $os_like " == *" cachyos "* ]]
}

silent_refresh_pacman_sources() {
    if [[ "$BASE_DISTRO" != "arch" ]]; then
        return 0
    fi

    local have_root=0
    if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
        have_root=1
    else
        # Ask for sudo once upfront; sudo -v caches the ticket.
        if sudo -v; then
            have_root=1
        else
            echo "[WARN]  Skipping pacman mirror refresh/ranking (sudo access not available)."
        fi
    fi

    if (( have_root )); then
        # Cached sudo — non-interactive from here on.
        as_root() {
            if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
                "$@"
            else
                sudo -n "$@"
            fi
        }

        # Enable parallel package downloads before anything syncs, so the
        # large package steps don't download hundreds of packages serially.
        if [[ -f /etc/pacman.conf ]]; then
            if grep -q '^#\?ParallelDownloads' /etc/pacman.conf; then
                as_root sed -i 's/^#\?ParallelDownloads.*/ParallelDownloads = 5/' /etc/pacman.conf
            else
                echo "ParallelDownloads = 5" | as_root tee -a /etc/pacman.conf >/dev/null
            fi
        fi

        if is_cachyos; then
            if command -v cachyos-rate-mirrors >/dev/null 2>&1; then
                echo "[INFO]  Ranking CachyOS mirrors..."
                as_root cachyos-rate-mirrors >/dev/null 2>&1 || echo "[WARN]  cachyos-rate-mirrors failed, continuing with current mirrors."
            else
                echo "[WARN]  cachyos-rate-mirrors is not installed; continuing with current mirrors."
            fi
        else
            # Reflector is the fallback for Arch-based systems without CachyOS tooling.
            if ! command -v reflector >/dev/null 2>&1; then
                as_root pacman -Sy --noconfirm reflector >/dev/null 2>&1 || true
            fi

            if command -v reflector >/dev/null 2>&1; then
                echo "[INFO]  Ranking Arch mirrors by download speed..."
                as_root reflector --latest 20 --protocol https --sort rate --save /etc/pacman.d/mirrorlist >/dev/null 2>&1 || echo "[WARN]  reflector failed, continuing with current mirrors."
            fi
        fi

        # Pre-install dos2unix for CRLF normalization later.
        if ! command -v dos2unix >/dev/null 2>&1; then
            as_root pacman -Sy --noconfirm dos2unix >/dev/null 2>&1 || true
        fi

        as_root pacman -Sy --noconfirm >/dev/null 2>&1 || echo "[WARN]  Failed to refresh pacman sources early. Continuing..."
        unset -f as_root
    fi
}

silent_refresh_native_sources() {
    local have_root=0

    case "$BASE_DISTRO" in
        fedora|debian) ;;
        *) return 0 ;;
    esac

    if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
        have_root=1
    elif sudo -v; then
        have_root=1
    else
        echo "[WARN]  Skipping package source refresh (sudo access not available)."
        return 0
    fi

    case "$BASE_DISTRO" in
        fedora)
            echo "[INFO]  Refreshing Fedora repository metadata using DNF..."
            if (( have_root )) && [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
                dnf makecache --refresh >/dev/null 2>&1 || echo "[WARN]  Failed to refresh DNF metadata. Continuing..."
            else
                sudo -n dnf makecache --refresh >/dev/null 2>&1 || echo "[WARN]  Failed to refresh DNF metadata. Continuing..."
            fi
            ;;
        debian)
            echo "[INFO]  Refreshing Debian repository metadata using APT..."
            if (( have_root )) && [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
                apt-get update >/dev/null 2>&1 || echo "[WARN]  Failed to refresh APT metadata. Continuing..."
            else
                sudo -n apt-get update >/dev/null 2>&1 || echo "[WARN]  Failed to refresh APT metadata. Continuing..."
            fi
            ;;
    esac
}

run_arch_pacman_install() {
    local -a pkgs=("$@")
    local -a pacman_args=(-S --needed --noconfirm)

    if (( ${#pkgs[@]} == 0 )); then
        return 0
    fi

    if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
        pacman -Sy --noconfirm >/dev/null 2>&1 || echo "[WARN]  Failed to refresh pacman sources before install. Continuing..."
        pacman "${pacman_args[@]}" "${pkgs[@]}" && return 0

        echo "[WARN]  pacman install failed. Refreshing sources and retrying once..."
        pacman -Sy --noconfirm >/dev/null 2>&1 || true
        pacman "${pacman_args[@]}" "${pkgs[@]}"
        return $?
    fi

    sudo pacman -Sy --noconfirm >/dev/null 2>&1 || echo "[WARN]  Failed to refresh pacman sources before install. Continuing..."
    sudo pacman "${pacman_args[@]}" "${pkgs[@]}" && return 0

    echo "[WARN]  pacman install failed. Refreshing sources and retrying once..."
    sudo pacman -Sy --noconfirm >/dev/null 2>&1 || true
    sudo pacman "${pacman_args[@]}" "${pkgs[@]}"
}

export BASE_DISTRO="$(detect_base_distro)"

# Only run in the outer (pre-tmux) invocation.
if [[ "${CAELESTIA_TMUX_MASTER:-0}" == "0" ]]; then
    if [[ "$BASE_DISTRO" == "arch" ]]; then
        silent_refresh_pacman_sources
    elif [[ "$BASE_DISTRO" == "fedora" || "$BASE_DISTRO" == "debian" ]]; then
        silent_refresh_native_sources
    fi
fi

normalize_line_endings_first() {
    export BASE_DISTRO="$(detect_base_distro)"
    local -a crlf_files=()
    local convert_choice=""

    mapfile -t crlf_files < <(
        find "$BUNDLE_DIR" -path "$BUNDLE_DIR/.git" -prune -o -type f -name '*.sh' -print0 | \
            xargs -0 grep -Il $'\r' 2>/dev/null || true
    )

    if (( ${#crlf_files[@]} == 0 )); then
        return 0
    fi

    echo "[WARN]  Detected ${#crlf_files[@]} file(s) with CRLF line endings."
    while true; do
        read -r -p "Convert all files under this repo to LF with dos2unix? [Y/n]: " convert_choice
        convert_choice="${convert_choice:-y}"

        case "${convert_choice,,}" in
            y|yes)
                if ! command -v dos2unix >/dev/null 2>&1; then
                    echo "[WARN]  dos2unix is not installed. Attempting to install it now..."
                    case "$BASE_DISTRO" in
                        arch)
                            run_arch_pacman_install dos2unix || return 1
                            ;;
                        fedora)
                            sudo dnf install -y dos2unix || return 1
                            ;;
                        debian)
                            sudo apt-get update && sudo apt-get install -y dos2unix || return 1
                            ;;
                        *)
                            echo "[WARN]  Could not detect distro for automatic dos2unix installation."
                            return 1
                            ;;
                    esac
                    echo "[OK]    dos2unix installed."
                fi

                (
                    cd "$BUNDLE_DIR" || exit 1
                    printf '%s\0' "${crlf_files[@]}" | xargs -0 -r dos2unix --
                ) || return 1

                echo "[OK]    Line endings normalized to LF."
                return 0
                ;;
            n|no)
                echo "[WARN]  Skipping line ending normalization by user choice."
                return 0
                ;;
            *)
                echo "Please answer with y or n."
                ;;
        esac
    done
}

if [[ "${CAELESTIA_TMUX_MASTER:-0}" == "0" ]]; then
    if ! normalize_line_endings_first; then
        echo "[FATAL] Line ending normalization step failed. Aborting installer." >&2
        exit 1
    fi
fi

BIN="$BUNDLE_DIR/caelestia-install"

# Try to fetch a prebuilt installer binary from GitHub Releases so we don't
# have to compile the TUI on the user's machine. The binary is built by
# .github/workflows/prebuilt-artifacts.yml and uploaded to the fixed
# `caelestia-bin-repo` release tag. Falls back to compiling when unavailable
# (no curl, offline, unsupported arch) or when forced via env var.
try_download_prebuilt_installer() {
    local arch
    arch="$(uname -m)"
    case "$arch" in
        x86_64|aarch64) ;;
        *) return 1 ;;
    esac

    local tmp_bin
    tmp_bin="$(mktemp)"
    local url
    url="https://github.com/ladybug-me/caelestia-dots-kde/releases/download/caelestia-bin-repo/caelestia-install-${arch}"
    if curl -fsSL --connect-timeout 10 --max-time 120 "$url" -o "$tmp_bin" 2>/dev/null; then
        chmod +x "$tmp_bin"
        printf '%s\n' "$tmp_bin"
        return 0
    fi
    rm -f "$tmp_bin"
    return 1
}

if [[ "${CAELESTIA_TMUX_MASTER:-0}" == "0" ]]; then
    echo -n "Preparing Caelestia installer"
    {
        while true; do
            printf "."
            sleep 0.5
            printf "."
            sleep 0.5
            printf "."
            sleep 0.5
            printf "\b\b\b   \b\b\b"
        done
    } &
    SPINNER_PID=$!

    PREBUILT_BIN=""
    if [[ -z "${CAELESTIA_FORCE_BUILD_INSTALLER:-}" ]] && command -v curl >/dev/null 2>&1; then
        PREBUILT_BIN="$(try_download_prebuilt_installer || true)"
    fi

    # Check and install requirements
    MISSING_PKGS=()
    if ! command -v g++ >/dev/null 2>&1; then
        MISSING_PKGS+=("g++")
    fi
    if ! command -v cmake >/dev/null 2>&1; then
        MISSING_PKGS+=("cmake")
    fi
    if ! command -v make >/dev/null 2>&1; then
        MISSING_PKGS+=("make")
    fi
    # tmux is used for the split-pane installer view unless explicitly disabled
    if [[ "${CAELESTIA_USE_TMUX:-1}" == "1" ]] && ! command -v tmux >/dev/null 2>&1; then
        MISSING_PKGS+=("tmux")
    fi

    if [ ${#MISSING_PKGS[@]} -ne 0 ]; then
        kill $SPINNER_PID 2>/dev/null || true
        echo ""
        echo "Missing build tools: ${MISSING_PKGS[*]}. Installing..."
        if [[ "$BASE_DISTRO" == "arch" ]]; then
            if [[ "${CAELESTIA_USE_TMUX:-1}" == "1" ]]; then
                run_arch_pacman_install base-devel cmake tmux
            else
                run_arch_pacman_install base-devel cmake
            fi
        elif [[ "$BASE_DISTRO" == "fedora" ]]; then
            if [[ "${CAELESTIA_USE_TMUX:-1}" == "1" ]]; then
                sudo dnf install -y gcc-c++ cmake make tmux
            else
                sudo dnf install -y gcc-c++ cmake make
            fi
        elif [[ "$BASE_DISTRO" == "debian" ]]; then
            if [[ "${CAELESTIA_USE_TMUX:-1}" == "1" ]]; then
                sudo apt-get update && sudo apt-get install -y build-essential g++ cmake make tmux
            else
                sudo apt-get update && sudo apt-get install -y build-essential g++ cmake make
            fi
        else
            echo "Could not auto-install build tools. Please install manually: ${MISSING_PKGS[*]}"
            exit 1
        fi
        echo -n "Preparing Caelestia installer"
        {
            while true; do
                printf "."
                sleep 0.5
                printf "."
                sleep 0.5
                printf "."
                sleep 0.5
                printf "\b\b\b   \b\b\b"
            done
        } &
        SPINNER_PID=$!
    fi

    if [[ -n "$PREBUILT_BIN" ]]; then
        kill $SPINNER_PID 2>/dev/null || true
        wait $SPINNER_PID 2>/dev/null || true
        echo ""
        rm -f "$BIN"
        mv "$PREBUILT_BIN" "$BIN"
        echo "[OK]    Using prebuilt installer binary (skipped compilation)."
    else
        BUILD_DIR="$BUNDLE_DIR/installer/build"
        BUILD_LOG="/tmp/caelestia_build.log"
        rm -rf "$BUILD_DIR"
        mkdir -p "$BUILD_DIR"
        (
            cd "$BUILD_DIR" || exit 1
            cmake -DCMAKE_BUILD_TYPE=Release .. >"$BUILD_LOG" 2>&1 || exit 1
            make -j"$(nproc 2>/dev/null || echo 1)" >>"$BUILD_LOG" 2>&1 || exit 1
        ) || {
            kill $SPINNER_PID 2>/dev/null || true
            echo ""
            echo "[FATAL] Failed to build the Caelestia installer." >&2
            echo "--- build log (last 60 lines) ---"
            tail -n 60 "$BUILD_LOG" 2>/dev/null || cat "$BUILD_LOG" 2>/dev/null
            echo "--- end build log ---"
            echo "Full log saved to: $BUILD_LOG"
            exit 1
        }


        kill $SPINNER_PID 2>/dev/null || true
        wait $SPINNER_PID 2>/dev/null || true
        echo ""

        rm -f "$BIN"
        cp "$BUILD_DIR/caelestia-install" "$BIN" || {
            echo "[FATAL] Failed to copy the compiled Caelestia installer to $BIN." >&2
            exit 1
        }
    fi
fi

cleanup_install_state() {
    # Always reset the terminal state on exit, regardless of how we exited (Ctrl+C, crash, etc.)
    stty sane 2>/dev/null || true
    tput cnorm 2>/dev/null || true
    printf '\033[0m\033[?1049l\033[?25h' 2>/dev/null || true

    if [[ -f /tmp/caelestia_inhibit.pid ]]; then
        kill -9 "$(cat /tmp/caelestia_inhibit.pid)" 2>/dev/null || true
    fi
    if [[ -f /tmp/caelestia_kde_inhibit.cookie ]]; then
        qdbus6 org.freedesktop.ScreenSaver /ScreenSaver org.freedesktop.ScreenSaver.UnInhibit "$(cat /tmp/caelestia_kde_inhibit.cookie)" 2>/dev/null || true
    fi
    rm -f /tmp/caelestia_inhibit.pid /tmp/caelestia_kde_inhibit.cookie

    if [[ -n "${TMUX:-}" && "${CAELESTIA_TMUX_MASTER:-0}" == "1" ]]; then
        tmux kill-session -t caelestia_install 2>/dev/null || true
        rm -f /tmp/caelestia_cmd /tmp/caelestia_status
    fi
    rm -f /tmp/caelestia_tmux_wrapper.sh
}
trap cleanup_install_state EXIT

if [[ -z "${TMUX:-}" && "${CAELESTIA_NO_TMUX:-0}" == "0" && "${CAELESTIA_USE_TMUX:-1}" == "1" ]]; then
    # Kill any stale session first
    tmux kill-session -t caelestia_install 2>/dev/null || true

    export CAELESTIA_TMUX_MASTER=1
    rm -f /tmp/caelestia_cmd /tmp/caelestia_status
    rm -f /tmp/caelestia_installer_err.log
    mkfifo /tmp/caelestia_cmd
    mkfifo /tmp/caelestia_status

    # Wrapper keeps the tmux pane alive after exit/crash for diagnostics.
    WRAPPER_SCRIPT="/tmp/caelestia_tmux_wrapper.sh"
    printf -v args_str '%q ' "$0" "$@"
    cat > "$WRAPPER_SCRIPT" <<WRAPPER_EOF
#!/usr/bin/env bash
bash $args_str
ec=\$?
echo ""
echo "============================================================"
echo "  installer session ended (exit code: \$ec)"
echo "============================================================"
echo ""
echo "Press Enter to close this window..."
read -r
exit \$ec
WRAPPER_EOF
    chmod +x "$WRAPPER_SCRIPT"

    tmux new-session -d -s caelestia_install "bash $WRAPPER_SCRIPT"
    # Keep pane visible on failure; close normally on success.
    tmux set-option -t caelestia_install remain-on-exit failed
    tmux set-option -t caelestia_install mouse on

    tmux attach-session -t caelestia_install
    _tmux_exit=$?

    # Always restore terminal state immediately after tmux exits, regardless of
    # how it exited (normal, Ctrl+C, crash).  The TUI binary may have left the
    # terminal in raw mode / alt-screen; without this the shell appears "stuck".
    stty sane 2>/dev/null || true
    tput cnorm 2>/dev/null || true
    printf '\033[0m\033[?1049l\033[?25h' 2>/dev/null || true

    # Surface inner-script diagnostics in the outer terminal.
    _needs_pause=0
    if [[ -s /tmp/caelestia_installer_err.log ]]; then
        _reached_done=0
        if grep -q '\[installer\] done (success)' /tmp/caelestia_installer_err.log 2>/dev/null; then
            _reached_done=1
        fi
        if [[ $_reached_done -eq 0 ]]; then
            stty sane 2>/dev/null || true
            tput cnorm 2>/dev/null || true
            echo ""
            echo "============================================================"
            echo "  INSTALLER DID NOT COMPLETE"
            echo "============================================================"
            echo ""
            echo "--- stderr output from installer ---"
            cat /tmp/caelestia_installer_err.log
            echo "--- end stderr ---------------------"
            echo ""
            _needs_pause=1
        fi
    elif [[ $_tmux_exit -ne 0 ]]; then
        stty sane 2>/dev/null || true
        tput cnorm 2>/dev/null || true
        echo ""
        echo "============================================================"
        echo "  INSTALLER SESSION ENDED (exit code: $_tmux_exit)"
        echo "  No stderr log was produced — the binary may have crashed"
        echo "  or the tmux session may have failed to start entirely"
        echo "  (check for a stale tmux server or /tmp/caelestia_cmd issues)."
        echo "============================================================"
        echo ""
        _needs_pause=1
    fi

    # Prevent terminal from auto-closing before the user can read output.
    if [[ $_needs_pause -eq 1 ]]; then
        echo "Press Enter to close this window..."
        read -r
    fi

    exit $_tmux_exit
fi

if [[ ! -x "$BIN" ]]; then
    echo ""
    echo "============================================================"
    echo "  FATAL: Installer binary missing: $BIN"
    echo "  C++ compilation likely failed — check g++, cmake, make."
    echo "============================================================"
    echo ""
    echo "Press Enter to close this window..."
    read -r
    exit 1
fi

_installer_start=$(date +%s)
"$BIN" "$@" 2>/tmp/caelestia_installer_err.log
_exit_code=$?
_installer_elapsed=$(($(date +%s) - _installer_start))

_reached_done=0
if grep -q '\[installer\] done (success)' /tmp/caelestia_installer_err.log 2>/dev/null; then
    _reached_done=1
fi

_show_diagnostic=0
_diag_title=""

if [[ $_exit_code -ne 0 ]]; then
    _show_diagnostic=1
    _diag_title="INSTALLER FAILED (exit code: $_exit_code)"
elif [[ $_reached_done -eq 0 ]]; then
    _show_diagnostic=1
    if [[ $_installer_elapsed -lt 3 ]]; then
        _diag_title="INSTALLER EXITED PREMATURELY (ran ${_installer_elapsed}s, exit 0)"
    else
        _diag_title="INSTALLER EXITED UNEXPECTEDLY (no completion marker)"
    fi
elif [[ -s /tmp/caelestia_installer_err.log ]]; then
    _show_diagnostic=1
    _diag_title="INSTALLER COMPLETED (stderr output captured below)"
fi

if [[ $_show_diagnostic -eq 1 ]]; then
    # Reset terminal in case the binary left it in raw/alt-screen mode
    stty sane 2>/dev/null || true
    tput cnorm 2>/dev/null || true
    printf '\033[0m\033[?1049l\033[?25h' 2>/dev/null || true

    echo ""
    echo "============================================================"
    echo "  $_diag_title"
    echo "============================================================"
    echo ""

    if [[ -s /tmp/caelestia_installer_err.log ]]; then
        echo "--- stderr output ---"
        cat /tmp/caelestia_installer_err.log
        echo "--- end stderr ------"
        echo ""
    else
        echo "(no stderr output captured)"
        echo ""
    fi

    if [[ $_exit_code -eq 139 ]]; then
        echo "Exit code 139 = SIGSEGV (segmentation fault / memory crash)."
    elif [[ $_exit_code -eq 127 ]]; then
        echo "Exit code 127 = command not found (missing shared library or binary)."
    elif [[ $_exit_code -eq 134 ]] || [[ $_exit_code -eq 135 ]]; then
        echo "Exit code $_exit_code = SIGABRT (aborted, possible assertion failure)."
    elif [[ $_exit_code -eq 0 ]] && [[ $_reached_done -eq 0 ]]; then
        echo "Binary exited cleanly (code 0) but never reached the summary screen."
        echo "This usually means it returned early before phase 6."
    fi
    echo ""

    echo "Press Enter to close this window..."
    read -r
fi

exit $_exit_code
