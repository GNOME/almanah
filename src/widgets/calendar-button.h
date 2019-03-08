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

#define ALMANAH_TYPE_CALENDAR_BUTTON        (almanah_calendar_button_get_type ())

G_DECLARE_DERIVABLE_TYPE (AlmanahCalendarButton, almanah_calendar_button, ALMANAH, CALENDAR_BUTTON, GtkToggleButton)

struct _AlmanahCalendarButtonClass {
	GtkToggleButtonClass parent;
	void (* day_selected) (AlmanahCalendarButton *self);
	void (* select_date_clicked) (AlmanahCalendarButton *self);
};

GtkWidget *almanah_calendar_button_new (AlmanahStorageManager *storage_manager) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
void almanah_calendar_button_set_storage_manager (AlmanahCalendarButton *self, AlmanahStorageManager *storage_manager);
void almanah_calendar_button_select_date (AlmanahCalendarButton *self, GDate *date);
void almanah_calendar_button_get_date (AlmanahCalendarButton *self, GDate *date);
void almanah_calendar_button_popdown (AlmanahCalendarButton *self);
void almanah_calendar_button_select_today (AlmanahCalendarButton *self);

G_END_DECLS

#endif /* !ALMANAH_CALENDAR_BUTTON_H */
