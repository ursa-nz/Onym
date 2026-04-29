# SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
# SPDX-License-Identifier: GPL-3.0-or-later

# The vendored WordNet engine is built on its own with warnings silenced, so its third party code
# does not clutter the build and stays untouched.
libonym_wni = static_library('onym-wni',
  'engine/wni.c',
  dependencies: [glib_dep, wordnet_dep],
  c_args: ['-w'],
  install: false,
)

libonym_sources = files(
  'onym-result.c',
  'onym-engine.c',
  'onym-lookup.c',
  'wn-index.c',
)

# Sources scanned for introspection: only the files that define public types.
libonym_introspectable_sources = files(
  'onym-result.c',
  'onym-engine.c',
)

libonym_public_headers = files(
  'onym.h',
  'onym-engine.h',
  'onym-result.h',
)

# Exported so the unit tests can compile the pure helpers directly.
wn_index_sources = files('wn-index.c')

libonym_deps = [glib_dep, gobject_dep, gio_dep, wordnet_dep]
libonym_inc = include_directories('.')

libonym = library('onym',
  libonym_sources,
  dependencies: libonym_deps,
  link_with: libonym_wni,
  include_directories: libonym_inc,
  version: meson.project_version(),
  soversion: '0',
  install: true,
)

install_headers(libonym_public_headers, subdir: 'onym')

libonym_dep = declare_dependency(
  link_with: libonym,
  include_directories: libonym_inc,
  dependencies: libonym_deps,
)

pkgconfig = import('pkgconfig')
pkgconfig.generate(libonym,
  name: 'onym',
  description: 'A GObject WordNet lexical model library',
  subdirs: 'onym',
  requires: ['glib-2.0', 'gobject-2.0', 'gio-2.0'],
)

gir_scanner = find_program('g-ir-scanner', required: get_option('introspection'))
if gir_scanner.found()
  gnome.generate_gir(libonym,
    sources: libonym_introspectable_sources + libonym_public_headers,
    namespace: 'Onym',
    nsversion: '0',
    identifier_prefix: 'Onym',
    symbol_prefix: 'onym',
    export_packages: 'onym',
    includes: ['GObject-2.0', 'Gio-2.0'],
    header: 'onym.h',
    install: true,
  )
endif

subdir('tests')
