/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Álvaro Peña 2012 <alvaropg@gmail.com>
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

#ifndef ALMANAH_CALENDAR_WINDOW_H
#define ALMANAH_CALENDAR_WINDOW_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "application.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_CALENDAR_WINDOW            (almanah_calendar_window_get_type ())
#define ALMANAH_CALENDAR_WINDOW(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_CALENDAR_WINDOW, AlmanahCalendarWindow))
#define ALMANAH_CALENDAR_WINDOW_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_CALENDAR_WINDOW, AlmanahCalendarWindowClass))
#define ALMANAH_IS_CALENDAR_WINDOW(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_CALENDAR_WINDOW))
#define ALMANAH_IS_CALENDAR_WINDOW_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_CALENDAR_WINDOW))
#define ALMANAH_CALENDAR_WINDOW_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_CALENDAR_WINDOW, AlmanahCalendarWindowClass))

typedef struct _AlmanahCalendarWindow        AlmanahCalendarWindow;
typedef struct _AlmanahCalendarWindowClass   AlmanahCalendarWindowClass;
typedef struct _AlmanahCalendarWindowPrivate AlmanahCalendarWindowPrivate;

struct _AlmanahCalendarWindow{
	GtkWindow parent;
	AlmanahCalendarWindowPrivate *priv;
};

struct _AlmanahCalendarWindowClass {
	GtkWindowClass parent;
};

GType almanah_calendar_window_get_type (void) G_GNUC_CONST;

GtkWidget *almanah_calendar_window_new (void) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

void almanah_calendar_window_popup (AlmanahCalendarWindow *self);
void almanah_calendar_window_popdown (AlmanahCalendarWindow *self);

G_END_DECLS

#endif /* !ALMANAH_CALENDAR_WINDOW_H */
