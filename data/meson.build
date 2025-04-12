service_conf = configuration_data()
service_conf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))

service = 'org.gnome.Almanah.service'
configure_file(
  input: service + '.in',
  output: service,
  install: true,
  install_dir: join_paths(datadir, 'dbus-1', 'services'),
  configuration: service_conf,
)

desktop = 'org.gnome.Almanah.desktop'
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

appdata = 'org.gnome.Almanah.metainfo.xml'
i18n.merge_file(
  input: appdata + '.in',
  output: appdata,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(datadir, 'metainfo'),
)
if (appstreamcli.found())
  test(
    'Validating ' + appdata,
    appstreamcli,
    args: ['validate', '--no-net', '--explain', appdata],
    workdir: meson.current_build_dir(),
  )
endif

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
