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

schemas_in_files = org.gnome.almanah.gschema.xml.in
gsettings_SCHEMAS = org.gnome.almanah.gschema.xml

@GSETTINGS_RULES@

# Data migration tool
convertdir = $(datadir)/GConf/gsettings
convert_DATA = almanah.convert
EXTRA_DIST += $(convert_DATA)

###############################################################################
# AppData file
###############################################################################

# AppData
@APPDATA_XML_RULES@
@INTLTOOL_XML_RULE@

appdata_in_files = almanah.appdata.xml.in
appdata_XML = $(appdata_in_files:.appdata.xml.in=.appdata.xml)

EXTRA_DIST += $(appdata_in_files)
CLEANFILES += $(appdata_XML)

DISTCHECK_CONFIGURE_FLAGS = --enable-appdata-validate

###############################################################################
# General
###############################################################################

EXTRA_DIST += entry-2.0.rnc

-include $(top_srcdir)/git.mk
