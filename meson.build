# SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
# SPDX-License-Identifier: GPL-3.0-or-later

project('onym', 'c',
  version: '0.1.0',
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

cc = meson.get_compiler('c')
wordnet_dep = cc.find_library('wordnet')
if not cc.has_header('wn.h')
  error('WordNet header wn.h was not found. Install the WordNet development files.')
endif

subdir('libonym')
subdir('tools')
