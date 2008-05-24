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
	gchar *(*format_value_func) (const DiaryLink *link);
	gboolean (*view_func) (const DiaryLink *link);
	void (*build_dialog_func) (const gchar *type, GtkTable *dialog_table);
	void (*get_values_func) (DiaryLink *link);
} DiaryLinkType;

void diary_populate_link_model (GtkListStore *list_store, guint type_column, guint name_column, guint icon_name_column);
gboolean diary_validate_link_type (const gchar *type);
const DiaryLinkType *diary_link_get_type (const gchar *type);
gchar *diary_link_format_value (const DiaryLink *link);
gboolean diary_link_view (const DiaryLink *link);
void diary_link_build_dialog (const DiaryLinkType *link_type);
void diary_link_get_values (DiaryLink *link);

G_END_DECLS

#endif /* !DIARY_LINK_H */
