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

#ifndef ALMANAH_SEARCH_DIALOG_H
#define ALMANAH_SEARCH_DIALOG_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define ALMANAH_TYPE_SEARCH_DIALOG      (almanah_search_dialog_get_type ())

G_DECLARE_FINAL_TYPE (AlmanahSearchDialog, almanah_search_dialog, ALMANAH, SEARCH_DIALOG, GtkDialog)

AlmanahSearchDialog *almanah_search_dialog_new (void);

G_END_DECLS

#endif /* !ALMANAH_SEARCH_DIALOG_H */
