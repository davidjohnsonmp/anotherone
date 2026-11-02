#!/bin/bash
# Builds Interval.component (hiccup audio), installs it for the current user and validates it with auval.
#   bash build_mac.sh            native build for this Mac (fastest)
#   bash build_mac.sh universal  Apple silicon + Intel
set -euo pipefail
cd "$(dirname "$0")"
bold() { printf '\n\033[1m%s\033[0m\n' "$1"; }

[ "$(uname)" = "Darwin" ] || { echo "Audio Units can only be built on macOS."; exit 1; }

if ! xcode-select -p >/dev/null 2>&1 || ! command -v clang++ >/dev/null 2>&1; then
    bold "Apple's Command Line Tools are needed. Opening the installer..."
    xcode-select --install || true
    echo "When that finishes, run this script again."
    exit 1
fi

CMAKE="$(command -v cmake || true)"
if [ -z "$CMAKE" ]; then
    CMAKE_VER=3.31.6
    CMAKE_DIR="$PWD/.tools/cmake-$CMAKE_VER-macos-universal"
    if [ ! -x "$CMAKE_DIR/CMake.app/Contents/bin/cmake" ]; then
        bold "Downloading CMake $CMAKE_VER (kept inside this folder, nothing is installed system-wide)"
        mkdir -p .tools
        curl -fL --retry 3 -o .tools/cmake.tar.gz "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VER/cmake-$CMAKE_VER-macos-universal.tar.gz"
        tar -xzf .tools/cmake.tar.gz -C .tools
        rm .tools/cmake.tar.gz
    fi
    CMAKE="$CMAKE_DIR/CMake.app/Contents/bin/cmake"
fi

ARCHS="$(uname -m)"
[ "${1:-}" = "universal" ] && ARCHS="arm64;x86_64"

bold "Configuring ($ARCHS). The first run also downloads the JUCE framework."
"$CMAKE" -S . -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=$ARCHS" >/dev/null

bold "Compiling. A few minutes the first time."
"$CMAKE" --build build --config Release --target interval_AU interval_Standalone -j "$(sysctl -n hw.ncpu)"

COMPONENT="$HOME/Library/Audio/Plug-Ins/Components/Interval.component"
BUILT="$(find build -type d -name 'Interval.component' -path '*AU*' | head -1)"
# The first build was called hiccup and shares this plugin's AU codes, so the two must not sit side by side.
rm -rf "$HOME/Library/Audio/Plug-Ins/Components/hiccup.component"
[ -n "$BUILT" ] || { echo "Build finished but no component was found."; exit 1; }
if [ "$(cd "$BUILT" && pwd)" != "$COMPONENT" ]; then
    rm -rf "$COMPONENT"; mkdir -p "$(dirname "$COMPONENT")"; cp -R "$BUILT" "$COMPONENT"
fi
codesign --force --deep --sign - "$COMPONENT" >/dev/null 2>&1 || true

# ask macOS to rescan Audio Units
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
sleep 1

bold "Validating with Apple's auval"
if auval -v aufx Hcp1 Hcup > build/auval.log 2>&1 && grep -q "AU VALIDATION SUCCEEDED" build/auval.log; then
    bold "Done. auval passed."
    echo "Installed: $COMPONENT"
    echo "Standalone test app: $(find build -type d -name 'Interval.app' | head -1)"
    echo "Open Logic, GarageBand or Ableton and look under hiccup audio > Interval."
else
    bold "auval reported a problem. The last lines of build/auval.log:"
    tail -25 build/auval.log
    exit 1
fi
