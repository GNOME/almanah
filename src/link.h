/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Diary
 * Copyright (C) Philip Withnall 2008 <philip@tecnocode.co.uk>
 * 
 * Diary is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * Diary is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Diary.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <glib.h>
#include <gtk/gtk.h>

#ifndef DIARY_LINK_H
#define DIARY_LINK_H

G_BEGIN_DECLS

typedef struct {
	gchar *type;
	gchar *value;
	gchar *value2;
} DiaryLink;

typedef struct {
	gchar *type;
	gchar *name;
	gchar *description;
	gchar *icon_name;
	guint columns;
} DiaryLinkType;

void diary_populate_link_model (GtkListStore *list_store, guint type_column, guint name_column);
gboolean diary_validate_link_type (const gchar *type);
const DiaryLinkType *diary_link_get_type (const gchar *type);
gchar *diary_link_format_value_for_display (DiaryLink *link);
void diary_link_view (DiaryLink *link);

G_END_DECLS

#endif /* !DIARY_LINK_H */
