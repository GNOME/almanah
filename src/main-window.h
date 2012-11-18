/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2008 <philip@tecnocode.co.uk>
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

#ifndef ALMANAH_MAIN_WINDOW_H
#define ALMANAH_MAIN_WINDOW_H

#include <config.h>
#include <glib.h>
#include <glib-object.h>

#include "application.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_MAIN_WINDOW		(almanah_main_window_get_type ())
#define ALMANAH_MAIN_WINDOW(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_MAIN_WINDOW, AlmanahMainWindow))
#define ALMANAH_MAIN_WINDOW_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_MAIN_WINDOW, AlmanahMainWindowClass))
#define ALMANAH_IS_MAIN_WINDOW(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_MAIN_WINDOW))
#define ALMANAH_IS_MAIN_WINDOW_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_MAIN_WINDOW))
#define ALMANAH_MAIN_WINDOW_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_MAIN_WINDOW, AlmanahMainWindowClass))

typedef struct _AlmanahMainWindowPrivate	AlmanahMainWindowPrivate;

typedef struct {
	GtkApplicationWindow parent;
	AlmanahMainWindowPrivate *priv;
} AlmanahMainWindow;

typedef struct {
	GtkApplicationWindowClass parent;
} AlmanahMainWindowClass;

GType almanah_main_window_get_type (void);
AlmanahMainWindow *almanah_main_window_new (AlmanahApplication *application) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

void almanah_main_window_select_date (AlmanahMainWindow *self, GDate *date);
void almanah_main_window_save_current_entry (AlmanahMainWindow *self, gboolean prompt_user);

G_END_DECLS

#endif /* !ALMANAH_MAIN_WINDOW_H */
