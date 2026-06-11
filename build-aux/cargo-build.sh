#!/bin/sh
# SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the onym-engine C ABI for meson, as one relocatable object.
#
# Usage: cargo-build.sh ENGINE_DIR TARGET_DIR OUTPUT
#
# Runs cargo from inside the engine checkout so a rustup-managed cargo
# honours the repository's rust-toolchain.toml pin; a plain system
# cargo ignores the pin and builds with its own stable toolchain, which
# the crates support. The engine has zero dependencies, so the build
# needs no network; --offline makes that a hard promise, which matters
# inside the network-isolated Flatpak build.
#
# The cargo staticlib is merged into a single relocatable object so
# meson can take it through the objects: keyword, which works for a
# shared and a static libonym alike and leaves the installed static
# library self-contained.

set -eu

engine_dir=$1
target_dir=$2
output=$3

# Ninja hands the output path relative to the build directory; resolve it before leaving.
case $output in
  /*) ;;
  *) output=$PWD/$output ;;
esac

cd "$engine_dir"
cargo build --locked --offline --release -p onym-engine-ffi --target-dir "$target_dir"
${CC:-cc} -r -nostdlib \
  -Wl,--whole-archive "$target_dir/release/libonym_engine_ffi.a" -Wl,--no-whole-archive \
  -o "$output"
