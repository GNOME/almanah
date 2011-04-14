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

#include <glib.h>
#include <gtk/gtk.h>

#include "storage-manager.h"

#ifndef ALMANAH_PRINTING_H
#define ALMANAH_PRINTING_H

G_BEGIN_DECLS

void almanah_print_entries (gboolean print_preview, GtkWindow *parent_window, GtkPageSetup **page_setup, GtkPrintSettings **print_settings,
                            AlmanahStorageManager *storage_manager);

G_END_DECLS

#endif /* !ALMANAH_PRINTING_H */
