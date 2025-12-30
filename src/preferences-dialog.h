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

#pragma once

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ALMANAH_TYPE_PREFERENCES_DIALOG (almanah_preferences_dialog_get_type ())

G_DECLARE_FINAL_TYPE (AlmanahPreferencesDialog, almanah_preferences_dialog, ALMANAH, PREFERENCES_DIALOG, GtkDialog)

AlmanahPreferencesDialog *almanah_preferences_dialog_new (GSettings *settings) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

G_END_DECLS
