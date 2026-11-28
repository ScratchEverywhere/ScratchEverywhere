#!/usr/bin/env bash
# Builds a macOS .app bundle from a compiled scratch-pc binary and wraps it in a .dmg
# (drag-to-Applications). Works on macOS (hdiutil) and cross-compiling hosts
# (mkfs.hfsplus + hfsplus + dmg, see install-dmg-tools.sh).
#
# Usage:
#   scripts/macos/package.sh --binary build/scratch-pc [options]
#
# Options:
#   --binary FILE        Compiled (thin or universal) macOS executable. Required.
#                        Its file name becomes the bundle executable.
#   --out DIR            Output directory (default: ./macos-package)
#   --version X.Y.Z      Version shown in Finder / the installer (default: 1.0.0)
#   --name NAME          Display name (default: "Scratch Everywhere!")
#   --min-macos VER      LSMinimumSystemVersion (default: 14.0)
#   --app-only           Only create the .app, skip the .dmg
#   --sign ID            codesign identity for the app (default: "-", ad-hoc). On
#                        non-macOS hosts only ad-hoc is supported by this script.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
GFX_DIR="${REPO_ROOT}/gfx/macOS"

BINARY=""
OUT_DIR="macos-package"
VERSION="1.0.0"
DISPLAY_NAME="Scratch Everywhere!"
MIN_MACOS="14.0"
BUNDLE_ID="com.se.scratcheverywhere"
APP_ONLY=0
SIGN_ID="-"

die() { echo "error: $*" >&2; exit 1; }

while [ $# -gt 0 ]; do
    case "$1" in
        --binary) BINARY="${2:?}"; shift 2 ;;
        --out) OUT_DIR="${2:?}"; shift 2 ;;
        --version) VERSION="${2:?}"; shift 2 ;;
        --name) DISPLAY_NAME="${2:?}"; shift 2 ;;
        --min-macos) MIN_MACOS="${2:?}"; shift 2 ;;
        --app-only) APP_ONLY=1; shift ;;
        --sign) SIGN_ID="${2:?}"; shift 2 ;;
        -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

[ -n "$BINARY" ] || die "--binary is required"
[ -f "$BINARY" ] || die "binary not found: $BINARY"
[ -f "${GFX_DIR}/Info.plist.in" ] || die "missing ${GFX_DIR}/Info.plist.in"
[ -f "${GFX_DIR}/Icon.icns" ] || die "missing ${GFX_DIR}/Icon.icns"

FILE_NAME="$(printf '%s' "$DISPLAY_NAME" | tr -d '!/:\\' | sed 's/[[:space:]]*$//')"
EXEC_NAME="$(basename "$BINARY")"
APP_DIR="${OUT_DIR}/${FILE_NAME}.app"

esc() { printf '%s' "$1" | sed -e 's/[&|\\]/\\&/g'; }

rm -rf "$APP_DIR"
mkdir -p "${APP_DIR}/Contents/MacOS" "${APP_DIR}/Contents/Resources"

cp "$BINARY" "${APP_DIR}/Contents/MacOS/${EXEC_NAME}"
chmod 755 "${APP_DIR}/Contents/MacOS/${EXEC_NAME}"
cp "${GFX_DIR}/Icon.icns" "${APP_DIR}/Contents/Resources/Icon.icns"
printf 'APPL????' > "${APP_DIR}/Contents/PkgInfo"

sed \
    -e "s|@SE_APP_NAME@|$(esc "$DISPLAY_NAME")|g" \
    -e "s|@SE_BUNDLE_ID@|$(esc "$BUNDLE_ID")|g" \
    -e "s|@SE_APP_VERSION@|$(esc "$VERSION")|g" \
    -e "s|@SE_EXECUTABLE_NAME@|$(esc "$EXEC_NAME")|g" \
    -e "s|@SE_MIN_MACOS@|$(esc "$MIN_MACOS")|g" \
    "${GFX_DIR}/Info.plist.in" > "${APP_DIR}/Contents/Info.plist"

echo "Created ${APP_DIR}"

if [ "$(uname -s)" = "Darwin" ]; then
    codesign --force --deep --sign "$SIGN_ID" "$APP_DIR"
    codesign --verify --deep --strict "$APP_DIR"
elif command -v rcodesign >/dev/null 2>&1; then
    [ "$SIGN_ID" = "-" ] || die "--sign with an identity is only supported on macOS (rcodesign is used for ad-hoc signing here)"
    rcodesign sign "$APP_DIR"
else
    echo "warning: rcodesign not found, leaving ${APP_DIR} unsigned" >&2
fi

if [ "$APP_ONLY" -eq 1 ]; then
    exit 0
fi

DMG="${OUT_DIR}/${EXEC_NAME}-macOS.dmg"
rm -f "$DMG"

STAGE="$(mktemp -d)"
chmod 755 "$STAGE"
trap 'rm -rf "$STAGE"' EXIT
cp -R "$APP_DIR" "$STAGE/"

if [ "$(uname -s)" = "Darwin" ]; then
    ln -s /Applications "$STAGE/Applications"
    hdiutil create -volname "$DISPLAY_NAME" -srcfolder "$STAGE" -ov -format UDZO "$DMG"
else
    for tool in mkfs.hfsplus hfsplus dmg; do
        command -v "$tool" >/dev/null || die "$tool is required to build a .dmg on non-macOS hosts (see scripts/macos/install-dmg-tools.sh)"
    done
    HFS="${STAGE}.hfs"
    trap 'rm -rf "$STAGE" "$HFS"' EXIT
    SIZE_KB="$(du -sk "$STAGE" | cut -f1)"
    truncate -s "$(( SIZE_KB * 12 / 10 / 1024 + 8 ))M" "$HFS"
    mkfs.hfsplus -v "$FILE_NAME" "$HFS" >/dev/null
    hfsplus "$HFS" addall "$STAGE" >/dev/null
    hfsplus "$HFS" symlink Applications /Applications >/dev/null
    dmg build "$HFS" "$DMG" >/dev/null
fi
echo "Created ${DMG}"
