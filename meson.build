# SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
# SPDX-License-Identifier: GPL-3.0-or-later

project('onym', 'c',
  version: '0.1.1',
  license: 'GPL-3.0-or-later',
  meson_version: '>= 1.0.0',
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

cc = meson.get_compiler('c')
wordnet_dep = cc.find_library('wordnet')
if not cc.has_header('wn.h')
  error('WordNet header wn.h was not found. Install the WordNet development files.')
endif

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
