SUBDIRS = icons

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
ui_DATA = almanah.ui

###############################################################################
# Desktop file
###############################################################################

@INTLTOOL_DESKTOP_RULE@

desktop_in_files = almanah.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

###############################################################################
# GConf schemas
###############################################################################

@INTLTOOL_SCHEMAS_RULE@

schemasdir = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = almanah.schemas.in
schemas_DATA = $(schemas_in_files:.schemas.in=.schemas)

install-data-local:
if GCONF_SCHEMAS_INSTALL
if ENCRYPTION
	@GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) gconftool-2 \
		--makefile-install-rule $(schemas_DATA) || \
		(echo ;\
		echo "*****************************************************"; \
		echo "Installation of schemas failed: install them manually"; \
		echo "*****************************************************";)
	@true
endif
endif

###############################################################################
# General
###############################################################################

EXTRA_DIST = \
	$(ui_DATA)		\
	$(desktop_in_files)	\
	$(schemas_in_files)
CLEANFILES = \
	$(desktop_DATA)		\
	$(schemas_DATA)
