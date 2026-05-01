# SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
# SPDX-License-Identifier: GPL-3.0-or-later

install_data('nz.ursa.Onym.gschema.xml',
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas',
)

# Compile the schema in the build tree so the app can run uninstalled, and recompile the system
# schema directory after install.
gnome.compile_schemas(build_by_default: true, depend_files: files('nz.ursa.Onym.gschema.xml'))
gnome.post_install(glib_compile_schemas: true)
