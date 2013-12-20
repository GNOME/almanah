SUBDIRS = icons

EXTRA_DIST =
CLEANFILES =

###############################################################################
# Icons
###############################################################################

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating GTK+ icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

###############################################################################
# UI file
###############################################################################

uidir = $(datadir)/almanah
ui_DATA = \
	almanah.ui		\
	almanah-app-menu.ui	\
	almanah.css

EXTRA_DIST += $(ui_DATA)

###############################################################################
# Desktop file
###############################################################################

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = almanah.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST += $(desktop_in_files)
CLEANFILES += $(desktop_DATA)

###############################################################################
# GSettings schemas
###############################################################################

gsettings_SCHEMAS = org.gnome.almanah.gschema.xml

@GSETTINGS_RULES@
@INTLTOOL_XML_NOMERGE_RULE@

# Data migration tool
convertdir = $(datadir)/GConf/gsettings
convert_DATA = almanah.convert

EXTRA_DIST += \
	$(schemas_in_files) \
	$(convert_DATA) \
	$(NULL)
CLEANFILES += $(gsettings_SCHEMAS)

###############################################################################
# AppData file
###############################################################################

appdatadir = $(datadir)/appdata
appdata_DATA = almanah.appdata.xml

EXTRA_DIST += almanah.appdata.xml.in
CLEANFILES += $(appdata_DATA)

###############################################################################
# General
###############################################################################

EXTRA_DIST += entry-2.0.rnc

-include $(top_srcdir)/git.mk
