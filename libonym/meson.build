# SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
# SPDX-License-Identifier: GPL-3.0-or-later

libonym_sources = files(
  'onym-result.c',
  'onym-engine.c',
  'onym-lookup.c',
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

libonym_deps = [glib_dep, gobject_dep, gio_dep, onym_core_dep]
libonym_inc = include_directories('.')

# The shared Rust core joins as one relocatable object, so a shared libonym absorbs it and a
# static libonym ships it inside the archive; either way the library is self-contained.
libonym = library('onym',
  libonym_sources,
  objects: onym_core_o,
  dependencies: libonym_deps,
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
