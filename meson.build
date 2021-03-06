project(
  'almanah',
  'c',
  license: 'GPL3+',
  version: '0.12.3',
  default_options: [
    'b_ndebug=if-release',
  ],
  meson_version: '>= 0.51',
)

cc = meson.get_compiler('c')
gnome = import('gnome')
i18n = import('i18n')
python = import('python')
python3 = python.find_installation('python3').path()
data_dir = join_paths(meson.source_root(), 'data')
po_dir = join_paths(meson.source_root(), 'po')

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))
icondir = join_paths(datadir, 'icons', 'hicolor')
localedir = join_paths(prefix, get_option('localedir'))
pkgdatadir = join_paths(datadir, meson.project_name())
schemadir = join_paths(datadir, 'glib-2.0', 'schemas')
convertdir = join_paths(datadir, 'GConf', 'gsettings')

# Dependencies
glib = dependency('glib-2.0', version: '>= 2.62')
gtk = dependency('gtk+-3.0', version: '>= 3.15')
gmodule = dependency('gmodule-2.0')
gthread = dependency('gthread-2.0')
gio = dependency('gio-2.0', version: '>= 2.32')
gtksourceview = dependency('gtksourceview-3.0')
sqlite = dependency('sqlite3')
cairo = dependency('cairo')
atk = dependency('atk')
gcr = dependency('gcr-base-3')
cryptui = dependency('cryptui-0.0')

# Options
evolution_support = get_option('evolution')
spell_checking_support = get_option('spell_checking')

min_gpgme_version = '1.0.0'
gpgme = dependency('gpgme', version: '>= @0@'.format(min_gpgme_version))

libecal = dependency('libecal-2.0', version: '>= 3.33.2', required: evolution_support)
libedataserver = dependency('libedataserver-1.2', version: '>= 2.28', required: evolution_support)
have_evolution = libecal.found() and libedataserver.found()

gtkspell = dependency('gtkspell3-3.0', required: spell_checking_support)
have_spell_check = gtkspell.found()

appstream_util = find_program('appstream-util', required: false)
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
  atk,
  gcr,
  cryptui,
  gpgme,
  libecal,
  libedataserver,
  gtkspell,
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
  # FIXME: We haven’t kept up to date with the post-3.8 deprecations yet:
  # https://gitlab.gnome.org/GNOME/almanah/issues/1
  #conf.set('EDS_DISABLE_DEPRECATED', 1)
endif

config = vcs_tag(
  input: configure_file(
    output: 'config.h.in',
    configuration: conf,
  ),
  output: 'config.h',
)

# Post-install scripts
meson.add_install_script(
  python3,
  join_paths(meson.source_root(), 'build-aux', 'post_install.py'),
  icondir,
  schemadir,
)

# Subfolders
subdir('data')
subdir('src')
subdir('po')
subdir('tests')
