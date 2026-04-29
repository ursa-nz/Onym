# SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
# SPDX-License-Identifier: GPL-3.0-or-later

onym_resources = gnome.compile_resources('onym-resources',
  'onym.gresource.xml',
  source_dir: '.',
  c_name: 'onym',
)

onym_app_sources = files(
  'main.c',
  'onym-application.c',
  'onym-window.c',
  'onym-result-view.c',
)

executable('onym',
  onym_app_sources,
  onym_resources,
  dependencies: [glib_dep, gobject_dep, gtk_dep, adw_dep, libonym_dep],
  install: true,
)
