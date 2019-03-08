cflags = [
  '-DALMANAH_TESTING',
]

vfs_test = executable(
  'vfs-test',
  sources: [
    '../src/vfs.c',
    'vfs.c',
    config,
  ],
  dependencies: almanah_deps,
  include_directories: include_directories('..'),
  c_args: cflags,
)
