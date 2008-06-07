# GtkBuilder UI file

uidir = $(datadir)/diary
ui_DATA = diary.ui
EXTRA_DIST = $(ui_DATA)

# Desktop file

desktop_in_files = diary.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

CLEANFILES = \
	$(desktop_in_files)	\
	$(desktop_DATA)
