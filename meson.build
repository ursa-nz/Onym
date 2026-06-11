# SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
# SPDX-License-Identifier: GPL-3.0-or-later

project('onym', 'c',
  version: '0.2.0',
  license: 'GPL-3.0-or-later',
  # 1.1.0 is the floor for generated objects in the objects: keyword, which carries the engine.
  meson_version: '>= 1.1.0',
  default_options: [
    'c_std=gnu11',
    'warning_level=1',
  ],
)

# The WordNet database directory used when WNSEARCHDIR and WNHOME are unset.
wn_default_dir = get_option('wordnet_data_dir')
add_project_arguments(
  '-DONYM_WN_DEFAULT_DIR="@0@"'.format(wn_default_dir),
  language: 'c',
)

glib_dep = dependency('glib-2.0', version: '>= 2.74')
gobject_dep = dependency('gobject-2.0')
gio_dep = dependency('gio-2.0')

gnome = import('gnome')

add_project_arguments(
  '-DONYM_VERSION="@0@"'.format(meson.project_version()),
  language: 'c',
)

# The lookup engine is the shared Rust core from the onym-engine repository, linked in as one
# relocatable object through its C ABI. The checkout is the sibling directory by default, the same
# convention its conformance kit uses; packagers point -Donym_engine_dir at a vendored copy
# instead.
fs = import('fs')
onym_engine_dir = get_option('onym_engine_dir')
if onym_engine_dir == ''
  onym_engine_dir = fs.parent(meson.project_source_root()) / 'onym-engine'
elif not fs.is_absolute(onym_engine_dir)
  onym_engine_dir = meson.project_source_root() / onym_engine_dir
endif
if not fs.is_dir(onym_engine_dir / 'crates' / 'onym-engine-ffi')
  error('No onym-engine checkout at "@0@". Clone it beside this repository or set -Donym_engine_dir.'.format(onym_engine_dir))
endif

cargo = find_program('cargo')
onym_core_o = custom_target('onym-engine-ffi',
  output: 'onym_engine_ffi.o',
  command: [
    find_program('build-aux/cargo-build.sh'),
    onym_engine_dir,
    meson.current_build_dir() / 'cargo-target',
    '@OUTPUT@',
  ],
  # Cargo tracks the crate sources itself, so hand it the decision on every build; a no-change
  # rebuild is its fast no-op.
  build_always_stale: true,
)

# Consumers compile against the hand-written C header; the engine object itself joins libonym
# through objects:, and these are the native libraries rustc names for a static Rust archive.
onym_core_dep = declare_dependency(
  compile_args: '-I@0@'.format(onym_engine_dir / 'crates' / 'onym-engine-ffi' / 'include'),
  link_args: ['-lgcc_s', '-lutil', '-lrt', '-lpthread', '-lm', '-ldl'],
)

# The application layer. The library itself does not need GTK, so these are optional.
gtk_dep = dependency('gtk4', version: '>= 4.14', required: get_option('app'))
adw_dep = dependency('libadwaita-1', version: '>= 1.5', required: get_option('app'))
build_app = gtk_dep.found() and adw_dep.found()

subdir('libonym')
subdir('tools')
if build_app
  subdir('data')
  subdir('src')
endif
