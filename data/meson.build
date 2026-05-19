# SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
# SPDX-License-Identifier: GPL-3.0-or-later

install_data('nz.ursa.Onym.gschema.xml',
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas',
)

# Compile the schema in the build tree so the app can run uninstalled, and recompile the system
# schema directory after install.
gnome.compile_schemas(build_by_default: true, depend_files: files('nz.ursa.Onym.gschema.xml'))
gnome.post_install(glib_compile_schemas: true, gtk_update_icon_cache: true)

subdir('icons')

# AppStream metainfo, merged with translations and installed.
i18n = import('i18n')
metainfo_file = i18n.merge_file(
  input: 'nz.ursa.Onym.metainfo.xml.in',
  output: 'nz.ursa.Onym.metainfo.xml',
  type: 'xml',
  po_dir: meson.project_source_root() / 'po',
  install: true,
  install_dir: get_option('datadir') / 'metainfo',
)

# Validate the metainfo as part of the test suite. --no-net keeps it offline; the screenshot URL is
# not fetched.
appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test('metainfo', appstreamcli,
    args: ['validate', '--strict', '--no-net', metainfo_file],
  )
endif

# Desktop entry, merged with translations and installed.
desktop_file = i18n.merge_file(
  input: 'nz.ursa.Onym.desktop.in',
  output: 'nz.ursa.Onym.desktop',
  type: 'desktop',
  po_dir: meson.project_source_root() / 'po',
  install: true,
  install_dir: get_option('datadir') / 'applications',
)

desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test('desktop-file', desktop_file_validate, args: [desktop_file])
endif
