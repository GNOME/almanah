import os
import subprocess
import sys

if len(sys.argv) < 3:
  sys.exit("usage: post_install.py <icondir> <schemadir>")

icondir = sys.argv[1]
schemadir = sys.argv[2]

if not os.environ.get('DESTDIR'):
  print('Update icon cache…')
  subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])

  print('Compiling gsettings schemas…')
  subprocess.call(['glib-compile-schemas', schemadir])
