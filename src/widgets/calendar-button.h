/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Álvaro Peña 2011 <alvaropg@gmail.com>
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

#ifndef ALMANAH_CALENDAR_BUTTON_H
#define ALMANAH_CALENDAR_BUTTON_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "widgets/calendar.h"
#include "storage-manager.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_CALENDAR_BUTTON		(almanah_calendar_button_get_type ())
#define ALMANAH_CALENDAR_BUTTON(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_CALENDAR_BUTTON, AlmanahCalendarButton))
#define ALMANAH_CALENDAR_BUTTON_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_CALENDAR_BUTTON, AlmanahCalendarButtonClass))
#define ALMANAH_IS_CALENDAR_BUTTON(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_CALENDAR_BUTTON))
#define ALMANAH_IS_CALENDAR_BUTTON_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_CALENDAR_BUTTON))
#define ALMANAH_CALENDAR_BUTTON_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_CALENDAR_BUTTON, AlmanahCalendarButtonClass))

typedef struct {
	GtkToggleButton parent;
} AlmanahCalendarButton;

typedef struct {
	GtkToggleButtonClass parent;
	void (* day_selected) (AlmanahCalendarButton *self);
	void (* select_date_clicked) (AlmanahCalendarButton *self);
} AlmanahCalendarButtonClass;

GType almanah_calendar_button_get_type (void) G_GNUC_CONST;
GtkWidget *almanah_calendar_button_new (AlmanahStorageManager *storage_manager) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
void almanah_calendar_button_set_storage_manager (AlmanahCalendarButton *self, AlmanahStorageManager *storage_manager);
void almanah_calendar_button_select_date (AlmanahCalendarButton *self, GDate *date);
void almanah_calendar_button_get_date (AlmanahCalendarButton *self, GDate *date);
void almanah_calendar_button_popdown (AlmanahCalendarButton *self);
void almanah_calendar_button_select_today (AlmanahCalendarButton *self);

G_END_DECLS

#endif /* !ALMANAH_CALENDAR_BUTTON_H */
