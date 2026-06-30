#!/bin/sh
# AlzScript Native installer
# curl -fsSL https://raw.githubusercontent.com/alz-tech/alz-native/main/install.sh | bash

set -e

REPO="alz-tech/alz-native"
BIN="alzc"

echo "🚀 AlzScript Native Installer"
echo ""
echo "Detecting platform..."

# Termux sets $PREFIX to its sandboxed root (e.g. /data/data/com.termux/files/usr).
# Binaries built by GitHub Actions' generic Ubuntu runner -- even when statically
# linked -- get killed by Android's seccomp filter with SIGSYS / "Bad system call"
# on startup, because they issue syscalls Termux's policy doesn't expect. The only
# reliable fix is building from source with Termux's OWN clang, which is compiled
# specifically to stay inside that sandbox.
if [ -n "$PREFIX" ] && [ -d "$PREFIX" ] && echo "$PREFIX" | grep -q "com.termux"; then
    echo "Platform: Termux (Android) -- building from source"
    echo "(prebuilt binaries trigger Android's seccomp filter; this is the reliable path)"
    echo ""

    if ! command -v clang > /dev/null 2>&1 && ! command -v gcc > /dev/null 2>&1; then
        echo "Installing build tools (clang)..."
        pkg install -y clang git > /dev/null 2>&1
    fi
    CC="clang"
    command -v clang > /dev/null 2>&1 || CC="gcc"

    SRC_DIR="$HOME/.alz-native-src"
    if [ -d "$SRC_DIR" ]; then
        rm -rf "$SRC_DIR"
    fi
    echo "Fetching source..."
    git clone --depth 1 "https://github.com/$REPO.git" "$SRC_DIR" > /dev/null 2>&1

    echo "Compiling (this takes under a minute)..."
    cd "$SRC_DIR"
    LOCAL_BIN="$HOME/.local/bin"
    mkdir -p "$LOCAL_BIN"
    $CC -std=c11 -O2 -Iinclude \
        src/value.c src/chunk.c src/vm.c src/lexer.c \
        src/compiler.c src/stdlib.c src/http.c src/db.c src/main.c \
        -o "$LOCAL_BIN/$BIN" -lm

    chmod +x "$LOCAL_BIN/$BIN"
    echo "✅ Built and installed to $LOCAL_BIN/$BIN"

    if ! echo "$PATH" | grep -q "$LOCAL_BIN"; then
        SHELL_RC="$HOME/.bashrc"
        [ -f "$HOME/.zshrc" ] && SHELL_RC="$HOME/.zshrc"
        echo "export PATH=\"\$PATH:$LOCAL_BIN\"" >> "$SHELL_RC"
        echo "   Added to PATH in $SHELL_RC"
        export PATH="$PATH:$LOCAL_BIN"
    fi

    echo ""
    if command -v alzc > /dev/null 2>&1; then
        alzc --version
        echo ""
        echo "Quick start:"
        echo "  echo 'print \"Hello World!\"' > hello.az && alzc hello.az"
        echo ""
        echo "Note: alzc verified above, but THIS terminal session will not"
        echo "see it on PATH until you open a new terminal, or run:"
        echo "  source $HOME/.bashrc"
    else
        echo "Done! Restart terminal then run: alzc --version"
    fi
    exit 0
fi

OS="$(uname -s)"
ARCH="$(uname -m)"

case "$OS" in
    Linux)
        case "$ARCH" in
            x86_64)                      FILE="alzc-linux-x64" ;;
            aarch64|arm64)               FILE="alzc-linux-arm64" ;;
            armv7l|armv6l|armv8l|arm*)   FILE="alzc-linux-arm" ;;
            *)
                echo "❌ Unsupported Linux architecture: $ARCH"
                echo "Please download manually from: https://github.com/$REPO/releases"
                exit 1 ;;
        esac ;;
    Darwin)
        case "$ARCH" in
            arm64)   FILE="alzc-macos-arm64" ;;
            x86_64)  FILE="alzc-macos-x64" ;;
            *)
                echo "❌ Unsupported Mac architecture: $ARCH"
                exit 1 ;;
        esac ;;
    *)
        echo "❌ Unsupported OS: $OS"
        echo "Windows users: download alzc-windows-x64.exe from:"
        echo "  https://github.com/$REPO/releases"
        exit 1 ;;
esac

echo "Platform: $OS/$ARCH → $FILE"

# Get latest version
LATEST=$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" \
    | grep '"tag_name"' | sed 's/.*"tag_name": *"\(.*\)".*/\1/')

if [ -z "$LATEST" ]; then
    echo "❌ Could not fetch latest release. Check your internet connection."
    exit 1
fi

echo "Version:  $LATEST"

URL="https://github.com/$REPO/releases/download/$LATEST/$FILE"

# Decide install location FIRST, then download straight into it.
# Termux's /tmp is a separate tmpfs from $HOME — staging there and
# mv'ing across that boundary can get killed by Android's storage
# sandbox mid-write. Writing the final file directly avoids that.
if [ -w "/usr/local/bin" ]; then
    DEST="/usr/local/bin/$BIN"
else
    LOCAL_BIN="$HOME/.local/bin"
    mkdir -p "$LOCAL_BIN"
    DEST="$LOCAL_BIN/$BIN"
fi

echo "Downloading from GitHub..."
curl -fL "$URL" -o "$DEST"
chmod +x "$DEST"
echo "✅ Installed to $DEST"

if [ "$DEST" = "$HOME/.local/bin/$BIN" ] && ! echo "$PATH" | grep -q "$HOME/.local/bin"; then
    SHELL_RC="$HOME/.bashrc"
    [ -f "$HOME/.zshrc" ] && SHELL_RC="$HOME/.zshrc"
    echo "export PATH=\"\$PATH:$HOME/.local/bin\"" >> "$SHELL_RC"
    echo "   Added to PATH in $SHELL_RC"
    # Also export it right now, just for this script's own verification
    # below. This does NOT persist to your interactive shell -- that is a
    # real shell limitation, not something a curl|bash script can work
    # around. You will still need a new terminal (or `source` it yourself)
    # before alzc works without the full path -- same as nvm/rustup/cargo.
    export PATH="$PATH:$HOME/.local/bin"
fi

echo ""
if command -v alzc > /dev/null 2>&1; then
    alzc --version
    echo ""
    echo "Quick start:"
    echo "  echo 'print \"Hello World!\"' > hello.az && alzc hello.az"
    if [ "$DEST" = "$HOME/.local/bin/$BIN" ]; then
        echo ""
        echo "Note: alzc verified above, but THIS terminal session will not"
        echo "see it on PATH until you open a new terminal, or run:"
        echo "  source $HOME/.bashrc"
    fi
else
    echo "Done! Restart terminal then run: alzc --version"
fi
