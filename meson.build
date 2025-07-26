project(
  'almanah',
  'c',
  license: 'GPL3+',
  version: '0.12.4',
  default_options: [
    'b_ndebug=if-release',
    'c_std=gnu11',
  ],
  meson_version: '>= 0.59',
)

cc = meson.get_compiler('c')
gnome = import('gnome')
i18n = import('i18n')
data_dir = join_paths(meson.project_source_root(), 'data')
po_dir = join_paths(meson.project_source_root(), 'po')

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))
icondir = join_paths(datadir, 'icons', 'hicolor')
localedir = join_paths(prefix, get_option('localedir'))
pkgdatadir = join_paths(datadir, meson.project_name())
schemadir = join_paths(datadir, 'glib-2.0', 'schemas')

# Dependencies
glib_major = 2
glib_minor = 76
glib = dependency('glib-2.0', version: f'>= @glib_major@.@glib_minor@')

gtk_major = 4
gtk_minor = 3
gtk = dependency('gtk4', version: f'>= @gtk_major@.@gtk_minor@')

gmodule = dependency('gmodule-2.0')
gthread = dependency('gthread-2.0')
gio = dependency('gio-2.0')

gtksourceview_major = 5
gtksourceview_minor = 0
gtksourceview = dependency('gtksourceview-5', version: f'>= @gtksourceview_major@.@gtksourceview_minor@')

sqlite = dependency('sqlite3')
cairo = dependency('cairo')
gcr = dependency('gcr-4', version: '>= 3.90.0')

# Options
evolution_support = get_option('evolution')
spell_checking_support = get_option('spell_checking')

min_gpgme_version = '1.5.0'
gpgme = dependency('gpgme', version: '>= @0@'.format(min_gpgme_version))

libecal = dependency('libecal-2.0', version: '>= 3.33.2', required: evolution_support)
libedataserver = dependency('libedataserver-1.2', version: '>= 2.28', required: evolution_support)
have_evolution = libecal.found() and libedataserver.found()

have_spell_check = false

appstreamcli = find_program('appstreamcli', required: false)
cppcheck = find_program('cppcheck', required: false)
desktop_file_validate = find_program('desktop-file-validate', required: false)

almanah_deps = [
  glib,
  gtk,
  gmodule,
  gthread,
  gio,
  gtksourceview,
  sqlite,
  cairo,
  gcr,
  gpgme,
  libecal,
  libedataserver,
]

# configuration
conf = configuration_data()
conf.set_quoted('VERSION', '@VCS_TAG@')
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_URL', 'https://wiki.gnome.org/Apps/Almanah_Diary')
conf.set_quoted('PACKAGE_DATA_DIR', pkgdatadir)
conf.set_quoted('PACKAGE_LOCALE_DIR', localedir)
conf.set_quoted('MIN_GPGME_VERSION', min_gpgme_version)
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
if get_option('buildtype').contains('debug')
  conf.set('ENABLE_DEBUG', 1)
endif
if have_spell_check
  conf.set('ENABLE_SPELL_CHECKING', 1)
endif
if have_evolution
  conf.set('HAVE_EVO', 1)
  conf.set('EDS_DISABLE_DEPRECATED', 1)
endif

config = vcs_tag(
  input: configure_file(
    output: 'config.h.in',
    configuration: conf,
  ),
  output: 'config.h',
)

if glib_minor % 2 == 1
  glib_minor = glib_minor + 1
endif
if gtk_minor % 2 == 1
  gtk_minor = gtk_minor + 1
endif
if gtksourceview_minor % 2 == 1
  gtksourceview_minor = gtksourceview_minor + 1
endif

global_c_args = [
  f'-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_@glib_major@_@glib_minor@',
  f'-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_@glib_major@_@glib_minor@',
  f'-DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_@gtk_major@_@gtk_minor@',
  f'-DGDK_VERSION_MAX_ALLOWED=GDK_VERSION_@gtk_major@_@gtk_minor@',
  f'-DGTK_SOURCE_VERSION_MIN_REQUIRED=GTK_SOURCE_VERSION_@gtksourceview_major@_@gtksourceview_minor@',
  f'-DGTK_SOURCE_VERSION_MAX_ALLOWED=GTK_SOURCE_VERSION_@gtksourceview_major@_@gtksourceview_minor@',
]

add_project_arguments(
  global_c_args,
  language: 'c',
)

# Post-install scripts
gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: true,
)

# Subfolders
subdir('data')
subdir('src')
subdir('po')
subdir('tests')
