desktop = 'almanah.desktop'
i18n.merge_file(
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(datadir, 'applications'),
)
if desktop_file_validate.found()
  test(
    'Validating @0@'.format(desktop),
    desktop_file_validate,
    args: [desktop],
    workdir: meson.current_build_dir(),
  )
endif

appdata = 'almanah.appdata.xml'
i18n.merge_file(
  input: appdata + '.in',
  output: appdata,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(datadir, 'metainfo'),
)
if (appstream_util.found())
  test(
    'Validating ' + appdata,
    appstream_util,
    args: ['validate', '--nonet', appdata],
    workdir: meson.current_build_dir(),
  )
endif

install_data(
  'almanah.convert',
  install_dir: convertdir,
)

schema_conf = configuration_data()
schema_conf.set('GETTEXT_PACKAGE', meson.project_name())
configure_file(
  input: 'org.gnome.almanah.gschema.xml.in',
  output: '@BASENAME@',
  configuration: schema_conf,
  install_dir: schemadir,
)

subdir('icons')
