icon_sizes = [
  '16x16',
  '22x22',
  '32x32',
  '48x48',
  '256x256',
]

foreach icon_size : icon_sizes
  install_data(
    join_paths(icon_size, 'org.gnome.Almanah.png'),
    install_dir: join_paths(icondir, icon_size, 'apps'),
  )
endforeach

install_data(
  join_paths('scalable', 'org.gnome.Almanah-tags-symbolic.svg'),
  install_dir: join_paths(icondir, 'scalable', 'actions'),
)

install_data(
  join_paths('scalable', 'org.gnome.Almanah-symbolic.svg'),
  install_dir: join_paths(icondir, 'scalable', 'apps'),
)
