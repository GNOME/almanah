/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Diary
 * Copyright (C) Philip Withnall 2008 <philip@tecnocode.co.uk>
 * 
 * Diary is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Diary is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Diary.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <gtk/gtk.h>
#include <glib.h>
#ifdef ENABLE_ENCRYPTION
#include <gconf/gconf-client.h>
#endif /* ENABLE_ENCRYPTION */

#include "storage-manager.h"

#ifndef DIARY_MAIN_H
#define DIARY_MAIN_H

G_BEGIN_DECLS

typedef struct {
	AlmanahStorageManager *storage_manager;
#ifdef ENABLE_ENCRYPTION
	GConfClient *gconf_client;
#endif /* ENABLE_ENCRYPTION */

	GtkWidget *main_window;
	GtkWidget *add_link_dialog;
	GtkWidget *search_dialog;

	GdkAtom native_serialisation_atom;

	gboolean debug;
} Diary;

Diary *diary;

void diary_quit (void);

G_END_DECLS

#endif /* DIARY_MAIN_H */
