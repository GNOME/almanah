/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2010 <philip@tecnocode.co.uk>
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

#ifndef ALMANAH_CALENDAR_H
#define ALMANAH_CALENDAR_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "storage-manager.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_CALENDAR		(almanah_calendar_get_type ())
#define ALMANAH_CALENDAR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_CALENDAR, AlmanahCalendar))
#define ALMANAH_CALENDAR_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_CALENDAR, AlmanahCalendarClass))
#define ALMANAH_IS_CALENDAR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_CALENDAR))
#define ALMANAH_IS_CALENDAR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_CALENDAR))
#define ALMANAH_CALENDAR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_CALENDAR, AlmanahCalendarClass))

typedef struct {
	GtkCalendar parent;
} AlmanahCalendar;

typedef struct {
	GtkCalendarClass parent;
} AlmanahCalendarClass;

GType almanah_calendar_get_type (void) G_GNUC_CONST;

GtkWidget *almanah_calendar_new (AlmanahStorageManager *storage_manager) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

AlmanahStorageManager *almanah_calendar_get_storage_manager (AlmanahCalendar *self) G_GNUC_PURE;
void almanah_calendar_set_storage_manager (AlmanahCalendar *self, AlmanahStorageManager *storage_manager);

void almanah_calendar_select_date (AlmanahCalendar *self, GDate *date);
void almanah_calendar_select_today (AlmanahCalendar *self);
void almanah_calendar_get_date (AlmanahCalendar *self, GDate *date);

G_END_DECLS

#endif /* !ALMANAH_CALENDAR_H */
