#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build a self-contained Onym AppImage. It bundles GTK4, libadwaita, the WordNet
# runtime library and the WordNet database, so it runs on any reasonably current
# Linux desktop with nothing installed. The AppRun points the engine at the
# bundled database through WNSEARCHDIR, which onym-engine.c already honours, so
# no code change is needed to relocate the data.
#
# Usage: build-aux/appimage/build-appimage.sh
# Env:   WN_DATA_DIR   WordNet database to bundle (default /usr/share/wordnet)
#        ONYM_VERSION  version string for the output name (default 0.1.0)
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
work="${root}/_appimage"
appdir="${work}/AppDir"
tools="${work}/tools"
arch="$(uname -m)"
version="${ONYM_VERSION:-0.1.0}"

# Nested AppImages (linuxdeploy, appimagetool) must run without FUSE in CI.
export APPIMAGE_EXTRACT_AND_RUN=1

rm -rf "$work"
mkdir -p "$appdir" "$tools"

# 1. Build the app and stage it into the AppDir under /usr. Static libonym keeps
#    the tree to the binary plus its data.
meson setup "${work}/build" "$root" \
  --prefix=/usr --buildtype=release \
  -Dapp=enabled -Dintrospection=disabled -Ddefault_library=static \
  -Dwordnet_data_dir=/usr/share/wordnet
meson compile -C "${work}/build"
DESTDIR="$appdir" meson install -C "${work}/build"

# Drop the development files the install also lays down; the AppImage runs the
# app, it does not host a library.
rm -rf "${appdir}/usr/include" "${appdir}/usr/lib/"*/pkgconfig
find "${appdir}/usr/lib" -name 'libonym.a' -delete 2>/dev/null || true

# 2. Bundle the WordNet database.
wn_data="${WN_DATA_DIR:-/usr/share/wordnet}"
if [ ! -f "${wn_data}/index.noun" ]; then
  echo "WordNet data not found in ${wn_data}; set WN_DATA_DIR" >&2
  exit 1
fi
install -d "${appdir}/usr/share/wordnet"
cp -a "${wn_data}/." "${appdir}/usr/share/wordnet/"

# 3. AppRun hook: point the WordNet engine at the bundled database. linuxdeploy's
#    AppRun sources every script in apprun-hooks/ before launching the app.
install -d "${appdir}/apprun-hooks"
cat > "${appdir}/apprun-hooks/onym-wordnet.sh" <<'HOOK'
export WNSEARCHDIR="${APPDIR}/usr/share/wordnet"
HOOK

# 4. Fetch the build tools (cached between runs).
fetch() { # url dest
  if [ ! -x "$2" ]; then echo "fetching $(basename "$2")"; curl -fsSL "$1" -o "$2"; chmod +x "$2"; fi
}
fetch "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${arch}.AppImage" "${tools}/linuxdeploy"
fetch "https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh" "${tools}/linuxdeploy-plugin-gtk.sh"

# 5. Assemble. The GTK plugin bundles GTK4/libadwaita, the gdk-pixbuf loaders,
#    the GIO modules, the icon theme and the compiled GSettings schemas;
#    linuxdeploy bundles the binary's own libraries, the WordNet runtime among
#    them, and writes the final AppImage.
export DEPLOY_GTK_VERSION=4
export LINUXDEPLOY_OUTPUT_VERSION="$version"
export PATH="${tools}:${PATH}"
out="Onym-${version}-${arch}.AppImage"

# linuxdeploy writes the AppImage to the working directory, so run it from the
# work tree and then move the result up, rather than emit it into the repo root.
( cd "$work" && "${tools}/linuxdeploy" \
    --appdir "$appdir" \
    --executable "${appdir}/usr/bin/onym" \
    --desktop-file "${appdir}/usr/share/applications/nz.ursa.Onym.desktop" \
    --icon-file "${appdir}/usr/share/icons/hicolor/scalable/apps/nz.ursa.Onym.svg" \
    --plugin gtk \
    --output appimage )

mv -f "${work}/${out}" "${root}/${out}"
echo "built: ${root}/${out}"
