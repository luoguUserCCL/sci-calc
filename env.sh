#!/usr/bin/env bash
# Environment setup for sci-calc cross-platform build.
# Source this file:  source ./env.sh
# All tools live under /home/z/my-project/tools (user-writable, no sudo).

set -e
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLS="/home/z/my-project/tools"

# --- JDK (Temurin 21, portable) ---
JDK_DIR="$(ls -d "$TOOLS"/jdk-21* 2>/dev/null | head -1)"
if [ -n "$JDK_DIR" ]; then
  export JAVA_HOME="$JDK_DIR"
  export PATH="$JAVA_HOME/bin:$PATH"
fi

# --- Zig (for macOS cross-compilation) ---
ZIG_DIR="$(ls -d "$TOOLS"/zig-x86_64-linux-* 2>/dev/null | head -1)"
if [ -n "$ZIG_DIR" ]; then
  export PATH="$ZIG_DIR:$PATH"
fi

# --- MinGW-w64 (ucrt64, for Windows cross-compilation) ---
MINGW_PREFIX="$TOOLS/mingw-root/usr"
if [ -d "$MINGW_PREFIX/bin" ]; then
  export PATH="$MINGW_PREFIX/bin:$PATH"
fi

# --- Native gcc (Linux host) is already in /usr/bin ---

# --- Project-wide constants ---
export SCI_CALC_ROOT="$ROOT"
export GCC_NATIVE="/usr/bin/gcc"
export GXX_NATIVE="/usr/bin/g++"

echo "[env] JAVA_HOME=$JAVA_HOME"
echo "[env] zig:    $(command -v zig 2>/dev/null || echo MISSING)"
echo "[env] javac:  $(command -v javac 2>/dev/null || echo MISSING)"
echo "[env] mingw:  $(command -v x86_64-w64-mingw32ucrt-g++ 2>/dev/null || echo MISSING)"
echo "[env] gcc:    $(command -v gcc 2>/dev/null || echo MISSING)"
