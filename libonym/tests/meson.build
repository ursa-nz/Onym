# SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
# SPDX-License-Identifier: GPL-3.0-or-later

# Pure logic, compiled against the helper source directly. Runs with no WordNet data.
test_index = executable('test-index',
  'test-index.c',
  wn_index_sources,
  dependencies: [glib_dep],
  include_directories: libonym_inc,
)
test('index', test_index)

# Lookups against the real database. Skips itself when the data is absent.
test_lookup = executable('test-lookup',
  'test-lookup.c',
  dependencies: [glib_dep, gobject_dep, libonym_dep],
  include_directories: libonym_inc,
)
test('lookup', test_lookup,
  env: ['WNSEARCHDIR=@0@'.format(wn_default_dir)],
)
