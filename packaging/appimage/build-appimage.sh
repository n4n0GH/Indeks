#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 n4n0
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Builds an Indeks AppImage:
#   packaging/appimage/build-appimage.sh [--no-build]
#
# Builds Indeks in release mode and bundles it with Qt, Qt PDF and the KDE libraries and QML
# modules it uses into dist/Indeks-<version>-<arch>.AppImage, using linuxdeploy and
# linuxdeploy-plugin-qt (downloaded on first use into build-appimage/tools, or taken from
# $LINUXDEPLOY / $LINUXDEPLOY_PLUGIN_QT). Beside it goes a .zsync file: published with the
# AppImage, it is what lets AppImageUpdate, Gear Lever, AppImageLauncher and the like find
# and fetch a newer Indeks.
#
# Not bundled, and used from the system where they are installed: Tesseract (text
# recognition), LibreOffice (Office documents as they look), Python 3 and bubblewrap
# (scripts), and an icon theme.
#
# Portability: it runs on systems with an older glibc than the build system's by carrying
# its own: see add_own_glibc.
#
# Environment:
#   QMAKE            qmake of the Qt to bundle (default: qmake6)
#   ARCH             target architecture (default: uname -m)
#   INDEKS_TOOLS_DIR where linuxdeploy is cached (default: build-appimage/tools)
#   INDEKS_DIST_DIR  where the AppImage goes (default: dist)
#   LDAI_UPDATE_INFORMATION  where a newer Indeks is found (default: this project's
#                    releases on GitHub; empty leaves it out)
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
ARCH=${ARCH:-$(uname -m)}
QMAKE=${QMAKE:-$(command -v qmake6 || command -v qmake)}
WORK=$ROOT/build-appimage
TOOLS=${INDEKS_TOOLS_DIR:-$WORK/tools}
BUILD_DIR=$WORK/build
APPDIR=$WORK/AppDir
DIST=${INDEKS_DIST_DIR:-$ROOT/dist}
VERSION=$(sed -n 's/^project(indeks VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -1)
ID=io.github.n4n0gh.indeks
BUILD=1
for arg in "$@"; do
    case $arg in
        --no-build) BUILD=0 ;;
        *) echo "usage: $0 [--no-build]" >&2; exit 2 ;;
    esac
done
[[ -n $VERSION ]] || { echo "cannot read the version from CMakeLists.txt" >&2; exit 1; }

say() { printf '\033[1m==> %s\033[0m\n' "$*"; }

QT_PLUGINS=$("$QMAKE" -query QT_INSTALL_PLUGINS)

# ---- tools -------------------------------------------------------------------
fetch() { # name url
    local dest=$TOOLS/$1
    if [[ ! -x $dest ]]; then
        say "Downloading $1" >&2
        mkdir -p "$TOOLS"
        curl -fL --retry 3 -o "$dest.part" "$2"
        chmod +x "$dest.part"
        mv "$dest.part" "$dest"
    fi
    echo "$dest"
}
LD_URL=https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH.AppImage
LDQT_URL=https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$ARCH.AppImage
LINUXDEPLOY=${LINUXDEPLOY:-$(fetch "linuxdeploy-$ARCH.AppImage" "$LD_URL")}
LINUXDEPLOY_PLUGIN_QT=${LINUXDEPLOY_PLUGIN_QT:-$(fetch "linuxdeploy-plugin-qt-$ARCH.AppImage" "$LDQT_URL")}
# The AppImage runtime, fetched here rather than by appimagetool (whose own download can hang).
RUNTIME_URL=https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-$ARCH
export LDAI_RUNTIME_FILE=${LDAI_RUNTIME_FILE:-$(fetch "runtime-$ARCH" "$RUNTIME_URL")}
# linuxdeploy finds plugins named linuxdeploy-plugin-* next to itself or on PATH.
mkdir -p "$TOOLS"
ln -sf "$LINUXDEPLOY_PLUGIN_QT" "$TOOLS/linuxdeploy-plugin-qt"
export PATH=$TOOLS:$PATH
# Work without FUSE (containers, CI).
export APPIMAGE_EXTRACT_AND_RUN=1

# ---- build -----------------------------------------------------------------------
if (( BUILD )); then
    say "Building Indeks $VERSION (release)"
    cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=OFF
    cmake --build "$BUILD_DIR"
fi
[[ -x $BUILD_DIR/bin/indeks ]] || { echo "missing $BUILD_DIR/bin/indeks (build without --no-build first)" >&2; exit 1; }

# ---- AppDir ------------------------------------------------------------------------
say "Preparing AppDir"
rm -rf "$APPDIR"
DESTDIR=$APPDIR cmake --install "$BUILD_DIR" > /dev/null
BIN=$APPDIR/usr/bin/indeks
# The helper that reads files goes beside the program, where Indeks looks first: the
# folder it was built to be installed in is not where the AppImage has it.
HELPER=$(find "$APPDIR/usr" -name indeks-extractor -type f | head -1)
[[ -n $HELPER ]] || { echo "the helper program was not installed" >&2; exit 1; }
[[ $HELPER == "$APPDIR/usr/bin/indeks-extractor" ]] || mv "$HELPER" "$APPDIR/usr/bin/indeks-extractor"
# "Send to Indeks" belongs to an Indeks that is installed with the system.
rm -rf "$APPDIR/usr/share/kio"

# .DirIcon, the picture file managers and AppImage catalogs show for the file itself. It has
# to be a PNG, which neither linuxdeploy nor appimagetool makes from an SVG.
DIR_ICON=usr/share/icons/hicolor/256x256/apps/$ID.png
render_icon() { # svg, png, size
    if command -v rsvg-convert >/dev/null; then
        rsvg-convert -w "$3" -h "$3" -o "$2" "$1"
    elif command -v inkscape >/dev/null; then
        inkscape --export-type=png --export-filename="$2" -w "$3" -h "$3" "$1" >/dev/null 2>&1
    else
        return 1
    fi
}
NO_DIR_ICON="No .DirIcon: it needs rsvg-convert or inkscape to make a PNG of the icon. The AppImage works without it, but has no picture of its own in file managers, and AppImage catalogs reject it."
mkdir -p "$APPDIR/$(dirname "$DIR_ICON")"
if render_icon "$ROOT/$ID.svg" "$APPDIR/$DIR_ICON" 256 && [[ -s $APPDIR/$DIR_ICON ]]; then
    ln -s "$DIR_ICON" "$APPDIR/.DirIcon"
    NO_DIR_ICON=
else
    rm -rf "$APPDIR/usr/share/icons/hicolor/256x256"
    say "$NO_DIR_ICON"
fi

# Things Indeks loads at runtime that no QML file of its own imports, so the QML scanner
# cannot see them: the org.kde.desktop Qt Quick Controls style and what it builds on.
EXTRA_QML=$WORK/extra-qml
mkdir -p "$EXTRA_QML"
cat > "$EXTRA_QML/RuntimeImports.qml" <<'QML'
import QtQuick
import org.kde.desktop
import org.kde.qqc2desktopstyle.private
Item {}
QML
copy_plugins() { # dir or file under the Qt plugin path
    [[ -e $QT_PLUGINS/$1 ]] || return 0
    mkdir -p "$APPDIR/usr/plugins/$(dirname "$1")"
    cp -a "$QT_PLUGINS/$1" "$APPDIR/usr/plugins/$1"
}
copy_plugins kf6/kirigami/platform
# KDE's look, icons and file dialogs. The system's cannot load against the bundled Qt.
copy_plugins platformthemes
copy_plugins styles
# How KDE's libraries talk to Wayland and X11.
copy_plugins kf6/kwindowsystem
for d in wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration; do copy_plugins "$d"; done
# What files are reached through: local ones (the file dialog lists folders with it), the
# desktop's trash (documents made in Indeks go there), archives, and servers.
for worker in kio_file kio_trash kio_ftp kio_http archive sftp smb fish nfs; do copy_plugins "kf6/kio/$worker.so"; done
# What reads the metadata of files (author, title, dimensions). Not the one for audio and
# video, which would bring a media library along for files Indeks does not show.
copy_plugins kf6/kfilemetadata
rm -f "$APPDIR/usr/plugins/kf6/kfilemetadata/kfilemetadata_ffmpegextractor.so"
rm -rf "$APPDIR/usr/plugins/kf6/kfilemetadata/writers"

# Some distributions (openSUSE) keep ICU's data in a file of its own, and libicudata is
# then a stub.
if [[ -d /usr/share/icu ]]; then
    mkdir -p "$APPDIR/usr/share"
    cp -a /usr/share/icu "$APPDIR/usr/share/"
fi

# On a system with an older glibc than this one's, which could not load what is bundled,
# Indeks runs on a copy of this one, through its loader: see add_own_glibc below. Everywhere
# else it is left out of it, and the system's drivers can need a glibc newer than this.
GLIBC_VERSION=$(getconf GNU_LIBC_VERSION | cut -d' ' -f2)
LOADER=$(basename "$(readelf -l "$BIN" | sed -n 's/.*interpreter: \(.*\)]/\1/p')")
APPRUN=$WORK/AppRun
cat > "$APPRUN" <<EOF
#!/bin/sh
HERE="\$(dirname "\$(readlink -f "\$0")")"
for d in "\$HERE"/usr/share/icu/*; do [ -d "\$d" ] && export ICU_DATA="\$d"; done
# The manual, which Indeks looks for among the data of installed programs.
export XDG_DATA_DIRS="\${XDG_DATA_DIRS:-/usr/local/share:/usr/share}:\$HERE/usr/share"
# Older than $GLIBC_VERSION, or not telling? INDEKS_OWN_GLIBC=1 or 0 decides instead, for testing.
own=\${INDEKS_OWN_GLIBC:-}
if [ -z "\$own" ]; then
    have=\$(getconf GNU_LIBC_VERSION 2>/dev/null | cut -d' ' -f2)
    major=\${have%%.*}; minor=\${have#*.}; minor=\${minor%%.*}
    case "\$major\$minor" in
        ''|*[!0-9]*) own=1 ;;
        *) if [ "\$major" -lt ${GLIBC_VERSION%%.*} ] || { [ "\$major" -eq ${GLIBC_VERSION%%.*} ] && [ "\$minor" -lt ${GLIBC_VERSION#*.} ]; }; then own=1; else own=0; fi ;;
    esac
fi
if [ "\$own" = 1 ]; then
    # (Read by the wrapper of the helper program.)
    export INDEKS_OWN_GLIBC=1
    exec "\$HERE/usr/glibc/$LOADER" --library-path "\$HERE/usr/glibc:\$HERE/usr/lib" --argv0 "\$HERE/usr/bin/indeks" "\$HERE/usr/bin/indeks" "\$@"
fi
export INDEKS_OWN_GLIBC=0
exec "\$HERE/usr/bin/indeks" "\$@"
EOF
chmod +x "$APPRUN"

# Some distributions (openSUSE) keep builds of libraries for newer CPUs too, and the system's
# own loader, which the scan asks, picks those on this CPU. Bundled, they would crash on any
# CPU without what they were built for, so the plain builds go in instead.
use_plain_cpu_builds() {
    local libdir file name variant
    libdir=$(dirname "$(ldd "$BIN" | awk '$1 == "libc.so.6" {print $3}')")
    for file in "$APPDIR"/usr/lib/*; do
        [[ -f $file && ! -L $file ]] || continue
        name=$(basename "$file")
        for variant in "$libdir"/glibc-hwcaps/*/"$name"; do
            [[ -e $variant && -e $libdir/$name ]] || continue
            cp -L --remove-destination "$libdir/$name" "$file"
            break
        done
    done
}

# What Indeks runs on with a system too old for it. Beside its other libraries it would be
# found on every system, so it has a folder of its own, only ever named by the loader. The
# loader is then what the system sees as running, and Qt looks for qt.conf beside that.
# It is not inherited: programs Indeks starts (Tesseract, LibreOffice, Python) use the
# system's libraries, as they should. The helper program is Indeks' own and goes the same
# way as Indeks itself.
add_own_glibc() {
    local own=$APPDIR/usr/glibc lib libdir
    libdir=$(dirname "$(ldd "$BIN" | awk '$1 == "libc.so.6" {print $3}')")
    mkdir -p "$own"
    cp -L "$libdir/$LOADER" "$own/"
    for lib in "${OWN_GLIBC_LIBS[@]}"; do
        [[ -e $libdir/$lib ]] && cp -L "$libdir/$lib" "$own/"
    done
    # An old system's versions of these are also too old for what is bundled. What is tied
    # to its hardware and session stays the system's, with what those depend on.
    local path name
    for lib in "${OWN_SYSTEM_LIBS[@]}"; do
        [[ -e $libdir/$lib ]] || continue
        cp -L "$libdir/$lib" "$own/"
        while read -r name _ path _; do
            [[ $path == /* && -e $path ]] || continue
            [[ -e $APPDIR/usr/lib/$name || -e $own/$name ]] && continue
            [[ -e $libdir/$name ]] && path=$libdir/$name
            [[ $name =~ ^lib(GL|EGL|GLX|OpenGL|gbm|drm|xcb|X11|Xau|Xdmcp|asound|ffi|bsd|md) ]] && continue
            cp -L "$path" "$own/"
        done < <(ldd "$libdir/$lib")
    done
    [[ -f $APPDIR/usr/bin/qt.conf ]] && cp "$APPDIR/usr/bin/qt.conf" "$own/"
    local helper=$APPDIR/usr/bin/indeks-extractor
    mv "$helper" "$helper.bin"
    cat > "$helper" <<WRAPPER
#!/bin/sh
HERE="\$(dirname "\$(readlink -f "\$0")")"
if [ "\$INDEKS_OWN_GLIBC" = 1 ]; then
    exec "\$HERE/../glibc/$LOADER" --library-path "\$HERE/../glibc:\$HERE/../lib" --argv0 "\$0" "\$HERE/indeks-extractor.bin" "\$@"
fi
exec "\$HERE/indeks-extractor.bin" "\$@"
WRAPPER
    chmod +x "$helper"
}
OWN_GLIBC_LIBS=(libc.so.6 libm.so.6 libmvec.so.1 libdl.so.2 libpthread.so.0 librt.so.1 libutil.so.1 libresolv.so.2 libanl.so.1 libnss_compat.so.2 libgcc_s.so.1 libstdc++.so.6)
OWN_SYSTEM_LIBS=(libharfbuzz.so.0 libfreetype.so.6 libfontconfig.so.1 libfribidi.so.0 libexpat.so.1 libz.so.1 libgmp.so.10 libcom_err.so.2 libICE.so.6 libSM.so.6 libwayland-client.so.0 libwayland-cursor.so.0 libwayland-egl.so.1)

# ---- bundle -------------------------------------------------------------------------
say "Bundling with linuxdeploy"
export QMAKE
export QML_SOURCES_PATHS="$ROOT/src/app/qml:$EXTRA_QML"
export EXTRA_PLATFORM_PLUGINS="libqwayland.so"
export LDAI_OUTPUT="$WORK/Indeks-$VERSION-$ARCH.AppImage"
export OUTPUT=$LDAI_OUTPUT
# Where AppImageUpdate and the like find a newer Indeks: the newest release on GitHub, with
# pre-releases, which "latest" passes over. They fetch it through the .zsync file made here,
# so that has to be published beside the AppImage.
: "${LDAI_UPDATE_INFORMATION=gh-releases-zsync|n4n0GH|Indeks|latest-all|Indeks-*-$ARCH.AppImage.zsync}"
if [[ -n $LDAI_UPDATE_INFORMATION ]]; then export LDAI_UPDATE_INFORMATION; else unset LDAI_UPDATE_INFORMATION; fi
deps_only=()
for d in "$APPDIR"/usr/plugins/kf6/kirigami/platform "$APPDIR"/usr/plugins/kf6/kio "$APPDIR"/usr/plugins/kf6/kwindowsystem "$APPDIR"/usr/plugins/kf6/kfilemetadata "$APPDIR"/usr/plugins/platformthemes "$APPDIR"/usr/plugins/styles "$APPDIR"/usr/plugins/wayland-*; do
    [[ -d $d ]] && deps_only+=(--deploy-deps-only "$d")
done
(cd "$WORK" && "$LINUXDEPLOY" --appdir "$APPDIR" \
    --executable "$BIN" \
    --executable "$APPDIR/usr/bin/indeks-extractor" \
    --custom-apprun "$APPRUN" \
    --desktop-file "$APPDIR/usr/share/applications/$ID.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/$ID.svg" \
    "${deps_only[@]}" \
    --plugin qt)
# Indeks keeps its index in SQLite; drivers for database servers are of no use to it.
if [[ -d $APPDIR/usr/plugins/sqldrivers ]]; then
    find "$APPDIR/usr/plugins/sqldrivers" -type f ! -name 'libqsqlite.so' -delete
fi
use_plain_cpu_builds
add_own_glibc
(cd "$WORK" && "$LINUXDEPLOY" --appdir "$APPDIR" --output appimage)

mkdir -p "$DIST"
mv -f "$LDAI_OUTPUT" "$DIST/"
say "Done: $DIST/$(basename "$LDAI_OUTPUT")"
if [[ -f $LDAI_OUTPUT.zsync ]]; then
    mv -f "$LDAI_OUTPUT.zsync" "$DIST/"
    say "With $(basename "$LDAI_OUTPUT").zsync, to publish beside it"
fi
[[ -z $NO_DIR_ICON ]] || say "$NO_DIR_ICON"
