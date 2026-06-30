#!/bin/sh
# AlzScript Native installer
# curl -fsSL https://raw.githubusercontent.com/alz-tech/alz-native/main/install.sh | bash

set -e

REPO="alz-tech/alz-native"
BIN="alzc"

echo "🚀 AlzScript Native Installer"
echo ""
echo "Detecting platform..."

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
