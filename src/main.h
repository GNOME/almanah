/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2008-2009 <philip@tecnocode.co.uk>
 * 
 * Almanah is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Almanah is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Almanah.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <gconf/gconf-client.h>

#include "storage-manager.h"
#include "event-manager.h"

#ifndef ALMANAH_MAIN_H
#define ALMANAH_MAIN_H

#ifdef ENABLE_ENCRYPTION
#define ENCRYPTION_KEY_GCONF_PATH "/apps/almanah/encryption_key"
#endif /* ENABLE_ENCRYPTION */

G_BEGIN_DECLS

typedef struct {
	AlmanahStorageManager *storage_manager;
	AlmanahEventManager *event_manager;
	GConfClient *gconf_client;
	GtkPrintSettings *print_settings;
	GtkPageSetup *page_setup;

	GtkWidget *main_window;
	GtkWidget *add_definition_dialog;
	GtkWidget *search_dialog;
	GtkWidget *definition_manager_window;
#ifdef ENABLE_ENCRYPTION
	GtkWidget *preferences_dialog;
#endif /* ENABLE_ENCRYPTION */

	gboolean debug;
	gboolean import_mode;
} Almanah;

extern Almanah *almanah;

void almanah_quit (void);

G_END_DECLS

#endif /* ALMANAH_MAIN_H */
