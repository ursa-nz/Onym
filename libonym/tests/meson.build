# SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
# SPDX-License-Identifier: GPL-3.0-or-later

# Lookups against the real database. Skips itself when the data is absent.
test_lookup = executable('test-lookup',
  'test-lookup.c',
  dependencies: [glib_dep, gobject_dep, libonym_dep],
  include_directories: libonym_inc,
)
test('lookup', test_lookup,
  env: ['WNSEARCHDIR=@0@'.format(wn_default_dir)],
)
