/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2011 <philip@tecnocode.co.uk>
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

#ifndef ALMANAH_HYPERLINK_TAG_H
#define ALMANAH_HYPERLINK_TAG_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ALMANAH_TYPE_HYPERLINK_TAG      (almanah_hyperlink_tag_get_type ())

G_DECLARE_FINAL_TYPE (AlmanahHyperlinkTag, almanah_hyperlink_tag, ALMANAH, HYPERLINK_TAG, GtkTextTag)

AlmanahHyperlinkTag *almanah_hyperlink_tag_new (const gchar *uri) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

const gchar *almanah_hyperlink_tag_get_uri (AlmanahHyperlinkTag *self) G_GNUC_PURE;
void almanah_hyperlink_tag_set_uri (AlmanahHyperlinkTag *self, const gchar *uri);

G_END_DECLS

#endif /* !ALMANAH_HYPERLINK_TAG_H */
