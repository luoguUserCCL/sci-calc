#!/usr/bin/env bash
# scripts/release.sh — build all artifacts and publish a GitHub Release.
# Usage: ./scripts/release.sh <tag> [name]
set -euo pipefail

TAG="${1:-v1.0.0}"
NAME="${2:-sci-calc $TAG}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

TOKEN_FILE="${HOME}/.gh-token"
if [ ! -f "$TOKEN_FILE" ]; then echo "missing ~/.gh-token (PAT)"; exit 1; fi
TOKEN="$(cat "$TOKEN_FILE")"
REPO="luoguUserCCL/sci-calc"
API="https://api.github.com/repos/$REPO"

export JAVA_HOME="/home/z/my-project/tools/jdk-21.0.11+10"
export PATH="/home/z/my-project/tools/gradle-8.10.2/bin:$JAVA_HOME/bin:$PATH"

echo "==> Building all artifacts..."
./gradlew buildLinuxExe buildWindowsExe buildMacosExe buildLinuxGui :jni:buildJar -q || {
    echo "build failed (macOS may be skipped if Zig absent)"; ./gradlew buildLinuxExe buildWindowsExe buildLinuxGui :jni:buildJar -q;
}

DIST="$ROOT/build/distributions"
JAR="$ROOT/jni/build/libs/sci-calc-1.0.0.jar"
STAGE="$(mktemp -d)"
cp "$DIST/sci-calc-linux"        "$STAGE/sci-calc-linux"
cp "$DIST/sci-calc-windows.exe"  "$STAGE/sci-calc-windows.exe"
[ -f "$DIST/sci-calc-macos" ]    && cp "$DIST/sci-calc-macos" "$STAGE/sci-calc-macos"
[ -f "$DIST/sci-calc-linux-gui" ] && cp "$DIST/sci-calc-linux-gui" "$STAGE/sci-calc-linux-gui"
cp "$JAR"                         "$STAGE/sci-calc-1.0.0.jar"
[ -f "$ROOT/assets/gui-screenshot.png" ] && cp "$ROOT/assets/gui-screenshot.png" "$STAGE/"

echo "==> Creating release $TAG..."
BODY="Cross-platform high-precision scientific calculator.

Four artifacts (all JVM-free natives + a cross-platform Jar):
- sci-calc-linux (native ELF, GCC)
- sci-calc-windows.exe (native PE32+, MinGW-w64 ucrt64, statically linked)
- sci-calc-macos (native Mach-O, Zig cc -target x86_64-macos-gnu)
- sci-calc-1.0.0.jar (Java + JNI, embeds .so/.dll/.dylib for all platforms)

Plus sci-calc-linux-gui (Dear ImGui GUI) and a rendered-formula screenshot.
"
BODY_ESC=$(python3 -c "import json,sys; print(json.dumps(sys.argv[1]))" "$BODY")
PAYLOAD="{\"tag_name\":\"$TAG\",\"name\":$(python3 -c "import json,sys;print(json.dumps(sys.argv[1]))" "$NAME"),\"body\":$BODY_ESC,\"draft\":false,\"prerelease\":false}"

REL_JSON=$(curl -s -X POST -H "Authorization: token $TOKEN" -H "Content-Type: application/json" -d "$PAYLOAD" "$API/releases")
REL_ID=$(echo "$REL_JSON" | python3 -c "import sys,json; print(json.load(sys.stdin).get('id',''))")
UPLOAD_URL=$(echo "$REL_JSON" | python3 -c "import sys,json; print(json.load(sys.stdin).get('upload_url','').replace('{?name,label}',''))")
if [ -z "$REL_ID" ] || [ -z "$UPLOAD_URL" ]; then
    echo "release create failed:"; echo "$REL_JSON" | head -20; exit 1
fi
echo "release id=$REL_ID  upload=$UPLOAD_URL"

echo "==> Uploading assets..."
for f in "$STAGE"/*; do
    name=$(basename "$f")
    echo "  uploading $name ..."
    curl -s -X POST -H "Authorization: token $TOKEN" \
         -H "Content-Type: application/octet-stream" \
         --data-binary @"$f" \
         "$UPLOAD_URL?name=$name" > /dev/null
done

rm -rf "$STAGE"
echo "==> Release $TAG published: https://github.com/$REPO/releases/tag/$TAG"
