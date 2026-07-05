#!/usr/bin/env bash
# scripts/bootstrap-tools.sh — idempotent toolchain bootstrap with fault tolerance.
# Re-creates /home/z/my-project/tools/ if the sandbox reset it.
#
# Usage:
#   ./scripts/bootstrap-tools.sh            # full ensure
#   ./scripts/bootstrap-tools.sh --check    # only fix missing pieces, quiet
set -uo pipefail

TOOLS="/home/z/my-project/tools"
mkdir -p "$TOOLS"
CHECK=0
[ "${1:-}" = "--check" ] && CHECK=1

log(){ [ "$CHECK" = 0 ] && echo "[bootstrap] $*"; }

# ---------- JDK (Temurin 21 portable) ----------
ensure_jdk() {
  local jdkdir
  jdkdir=$(ls -d "$TOOLS"/jdk-21* 2>/dev/null | head -1)
  if [ -n "$jdkdir" ] && "$jdkdir/bin/javac" -version >/dev/null 2>&1; then
    log "jdk: OK ($jdkdir)"; return 0
  fi
  log "jdk: downloading Temurin 21..."
  curl -L --retry 5 -o "$TOOLS/jdk.tar.gz" \
    "https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.11%2B10/OpenJDK21U-jdk_x64_linux_hotspot_21.0.11_10.tar.gz" 2>/dev/null
  ( cd "$TOOLS" && tar -xzf jdk.tar.gz && rm jdk.tar.gz )
}

# ---------- Zig 0.16.0 ----------
ensure_zig() {
  local zigdir
  zigdir=$(ls -d "$TOOLS"/zig-x86_64-linux-* 2>/dev/null | head -1)
  if [ -n "$zigdir" ] && "$zigdir/zig" version >/dev/null 2>&1; then
    log "zig: OK ($zigdir)"; return 0
  fi
  log "zig: downloading 0.16.0..."
  curl -L --retry 10 --retry-delay 5 -o "$TOOLS/zig.tar.xz" \
    "https://ziglang.org/download/0.16.0/zig-x86_64-linux-0.16.0.tar.xz" 2>/dev/null
  ( cd "$TOOLS" && tar -xf zig.tar.xz && rm zig.tar.xz )
}

# ---------- MinGW-w64 ucrt64 (extract .deb without root) ----------
ensure_mingw() {
  local mp="$TOOLS/mingw-root/usr/bin/x86_64-w64-mingw32ucrt-g++"
  if [ -x "$mp" ]; then log "mingw: OK"; return 0; fi
  log "mingw: downloading + extracting debs..."
  local tmpd; tmpd=$(mktemp -d)
  local URLS="http://ftp.cn.debian.org/debian/pool/main/m/mingw-w64/mingw-w64_12.0.0-5_all.deb http://ftp.cn.debian.org/debian/pool/main/m/mingw-w64/mingw-w64-ucrt64-dev_12.0.0-5_all.deb http://ftp.cn.debian.org/debian/pool/main/m/mingw-w64/mingw-w64-common_12.0.0-5_all.deb http://ftp.cn.debian.org/debian/pool/main/b/binutils-mingw-w64/binutils-mingw-w64-ucrt64_2.44-3+12+b1_amd64.deb http://ftp.cn.debian.org/debian/pool/main/g/gcc-mingw-w64/gcc-mingw-w64-ucrt64-runtime_14.2.0-19+27+b1_amd64.deb http://ftp.cn.debian.org/debian/pool/main/g/gcc-mingw-w64/gcc-mingw-w64-ucrt64_14.2.0-19+27+b1_amd64.deb http://ftp.cn.debian.org/debian/pool/main/g/gcc-mingw-w64/g++-mingw-w64-ucrt64_14.2.0-19+27+b1_amd64.deb"
  for u in $URLS; do curl -sL --retry 5 --max-time 180 "$u" -o "$tmpd/$(basename $u)"; done
  mkdir -p "$TOOLS/mingw-root"
  for deb in "$tmpd"/*.deb; do
    ( cd "$tmpd" && ar x "$deb" 2>/dev/null
      for f in data.tar.* data.tar; do [ -f "$f" ] && tar -xf "$f" -C "$TOOLS/mingw-root" 2>/dev/null && rm -f "$f"; done
      rm -f control.tar.* debian-binary 2>/dev/null )
  done
  rm -rf "$tmpd"
}

# ---------- Gradle 8.10.2 ----------
ensure_gradle() {
  local gdir
  gdir=$(ls -d "$TOOLS"/gradle-8.* 2>/dev/null | head -1)
  if [ -n "$gdir" ] && "$gdir/bin/gradle" -version >/dev/null 2>&1; then
    log "gradle: OK ($gdir)"; return 0
  fi
  log "gradle: downloading 8.10.2..."
  curl -L --retry 5 -o "$TOOLS/gradle.zip" \
    "https://services.gradle.org/distributions/gradle-8.10.2-bin.zip" 2>/dev/null
  ( cd "$TOOLS" && unzip -q -o gradle.zip && rm gradle.zip )
}

ensure_jdk
ensure_zig
ensure_mingw
ensure_gradle
log "bootstrap complete."
