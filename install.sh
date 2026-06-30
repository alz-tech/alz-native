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
TMP="/tmp/alzc-download"

echo "Downloading from GitHub..."
curl -fsSL "$URL" -o "$TMP"
chmod +x "$TMP"

# Try /usr/local/bin first, then ~/.local/bin for Termux/no-root
if [ -w "/usr/local/bin" ]; then
    mv "$TMP" "/usr/local/bin/$BIN"
    echo "✅ Installed to /usr/local/bin/$BIN"
else
    LOCAL_BIN="$HOME/.local/bin"
    mkdir -p "$LOCAL_BIN"
    mv "$TMP" "$LOCAL_BIN/$BIN"
    echo "✅ Installed to $LOCAL_BIN/$BIN"
    if ! echo "$PATH" | grep -q "$LOCAL_BIN"; then
        SHELL_RC="$HOME/.bashrc"
        [ -f "$HOME/.zshrc" ] && SHELL_RC="$HOME/.zshrc"
        echo "export PATH=\"\$PATH:$LOCAL_BIN\"" >> "$SHELL_RC"
        echo "   Added to PATH in $SHELL_RC"
        echo "   Run: source $SHELL_RC"
    fi
fi

echo ""
if command -v alzc > /dev/null 2>&1; then
    alzc --version
    echo ""
    echo "Quick start:"
    echo "  echo 'print \"Hello World!\"' > hello.az && alzc hello.az"
else
    echo "Done! Restart terminal then run: alzc --version"
fi
