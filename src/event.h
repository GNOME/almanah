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

#ifndef ALMANAH_EVENT_H
#define ALMANAH_EVENT_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ALMANAH_TYPE_EVENT      (almanah_event_get_type ())

G_DECLARE_DERIVABLE_TYPE (AlmanahEvent, almanah_event, ALMANAH, EVENT, GObject)

struct _AlmanahEventClass {
	GObjectClass parent;

	const gchar *name;
	const gchar *description;
	const gchar *icon_name;

	const gchar *(*format_value) (AlmanahEvent *event);
	const gchar *(*format_time) (AlmanahEvent *event);
	gboolean (*view) (AlmanahEvent *event, GtkWindow *parent_window);
};

const gchar *almanah_event_format_value (AlmanahEvent *self);
const gchar *almanah_event_format_time (AlmanahEvent *self);
gboolean almanah_event_view (AlmanahEvent *self, GtkWindow *parent_window);
const gchar *almanah_event_get_name (AlmanahEvent *self);
const gchar *almanah_event_get_description (AlmanahEvent *self);
const gchar *almanah_event_get_icon_name (AlmanahEvent *self);

G_END_DECLS

#endif /* !ALMANAH_EVENT_H */
