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

gschema_xml = 'org.gnome.almanah.gschema.xml'
install_data(
  gschema_xml,
  install_dir: schemadir,
)

gnome.compile_schemas(
  depend_files: [
    gschema_xml,
  ],
)

subdir('icons')
