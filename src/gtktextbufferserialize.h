/* gtktextbufferserialize.h
 *
 * Copyright (C) 2004 Nokia Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

gboolean
almanah_deserialise_entry_gtk_text_buffer (GtkTextBuffer *content_buffer,
                                           GtkTextIter *iter,
                                           const guint8 *text,
                                           gsize length,
                                           gboolean create_tags,
                                           GError **error);
