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

#include <gtk/gtk.h>

#ifndef ALMANAH_INTERFACE_H
#define ALMANAH_INTERFACE_H

G_BEGIN_DECLS

const gchar *almanah_get_interface_filename (void);
GtkWidget *almanah_create_interface (void);
void almanah_interface_embolden_label (GtkLabel *label);
void almanah_interface_create_text_tags (GtkTextBuffer *text_buffer, gboolean connect_events);
void almanah_calendar_month_changed_cb (GtkCalendar *calendar, gpointer user_data);

G_END_DECLS

#endif /* ALMANAH_INTERFACE_H */
