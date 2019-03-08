/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Álvaro Peña 2013 <alvaropg@gmail.com>
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

#ifndef ALMANAH_TAG_H
#define ALMANAH_TAG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ALMANAH_TYPE_TAG         (almanah_tag_get_type ())

G_DECLARE_FINAL_TYPE (AlmanahTag, almanah_tag, ALMANAH, TAG, GtkDrawingArea)

GtkWidget   *almanah_tag_new      (const gchar *tag);
const gchar *almanah_tag_get_tag  (AlmanahTag *tag_widget);
void         almanah_tag_remove   (AlmanahTag *tag_widget);

G_END_DECLS

#endif /* !ALMANAH_TAG_H */
