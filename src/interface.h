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

#include <gtk/gtk.h>

#ifndef DIARY_INTERFACE_H
#define DIARY_INTERFACE_H

G_BEGIN_DECLS

GtkWidget *diary_create_interface (void);
void diary_interface_embolden_label (GtkLabel *label);
void diary_interface_error (const gchar *message, GtkWidget *parent_window);
void diary_calendar_month_changed_cb (GtkCalendar *calendar, gpointer user_data);

G_END_DECLS

#endif /* DIARY_INTERFACE_H */
