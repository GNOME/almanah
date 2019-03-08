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

#define ALMANAH_TYPE_MAIN_WINDOW        (almanah_main_window_get_type ())

G_DECLARE_FINAL_TYPE (AlmanahMainWindow, almanah_main_window, ALMANAH, MAIN_WINDOW, GtkApplicationWindow)

AlmanahMainWindow *almanah_main_window_new (AlmanahApplication *application) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

void almanah_main_window_select_date (AlmanahMainWindow *self, GDate *date);
void almanah_main_window_save_current_entry (AlmanahMainWindow *self, gboolean prompt_user);

G_END_DECLS

#endif /* !ALMANAH_MAIN_WINDOW_H */
