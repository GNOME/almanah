enum_headers = [
  'event-factory.h',
]

sources = [
  'main.c',
  'application.c',
  'interface.c',
  'main-window.c',
  'storage-manager.c',
  'search-dialog.c',
  'preferences-dialog.c',
  'printing.c',
  'entry.c',
  'event.c',
  'event-factory.c',
  'event-manager.c',
  'date-entry-dialog.c',
  'import-export-dialog.c',
  'import-operation.c',
  'export-operation.c',
  'uri-entry-dialog.c',
  'widgets/calendar.c',
  'widgets/calendar-button.c',
  'widgets/entry-tags-area.c',
  'widgets/hyperlink-tag.c',
  'widgets/tag-accessible.c',
  'widgets/tag.c',
  'widgets/tag-entry.c',
  'vfs.c',
  config,
]

if have_evolution
  sources += [
    'event-factories/calendar.c',
    'event-factories/calendar-client.c',
    'event-factories/calendar-sources.c',
    'event-factories/e-cell-renderer-color.c',
    'event-factories/e-source-selector.c',
    'events/calendar-appointment.c',
    'events/calendar-task.c',
  ]
endif

sources += gnome.compile_resources(
  'almanah-resources',
  'almanah.gresource.xml',
  source_dir: [
    data_dir,
  ],
  c_name: 'almanah',
  export: true,
)

sources += gnome.mkenums_simple(
  'enums',
  sources: enum_headers,
)

inc = include_directories(
  '..',
  'events',
  'event-factories',
)

almanah = executable(
  meson.project_name(),
  sources,
  dependencies: almanah_deps,
  include_directories: inc,
  install: true,
)
