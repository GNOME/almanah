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

#ifndef DIARY_STORAGE_MANAGER_H
#define DIARY_STORAGE_MANAGER_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define DIARY_TYPE_STORAGE_MANAGER		(diary_storage_manager_get_type ())
#define DIARY_STORAGE_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DIARY_TYPE_STORAGE_MANAGER, DiaryStorageManager))
#define DIARY_STORAGE_MANAGER_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), DIARY_TYPE_STORAGE_MANAGER, DiaryStorageManagerClass))
#define DIARY_IS_STORAGE_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DIARY_TYPE_STORAGE_MANAGER))
#define DIARY_IS_STORAGE_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DIARY_TYPE_STORAGE_MANAGER))
#define DIARY_STORAGE_MANAGER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DIARY_TYPE_STORAGE_MANAGER, DiaryStorageManagerClass))

typedef struct _DiaryStorageManagerPrivate	DiaryStorageManagerPrivate;

typedef struct {
	GObject parent;
	DiaryStorageManagerPrivate *priv;
	/* TODO */
} DiaryStorageManager;

typedef struct {
	GObjectClass parent;
} DiaryStorageManagerClass;

typedef gint (*QueryCallback) (gpointer user_data, gint columns, gchar **data, gchar **column_names);

typedef struct {
	gchar **data;
	gint rows;
	gint columns;
} QueryResults;

typedef struct {
	gchar *type;
	gchar *value;
} EntryLink;

GType diary_storage_manager_get_type (void);
DiaryStorageManager *diary_storage_manager_new (const gchar *filename);
QueryResults *diary_storage_manager_query (DiaryStorageManager *self, const gchar *query, ...);
void diary_storage_manager_free_results (QueryResults *results);
gboolean diary_storage_manager_query_async (DiaryStorageManager *self, const gchar *query, const QueryCallback callback, gpointer user_data, ...);
gchar *diary_storage_manager_get_diary_entry (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day);
gboolean diary_storage_manager_set_diary_entry (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day, const gchar *content);
gboolean *diary_storage_manager_get_month_marked_days (DiaryStorageManager *self, GDateYear year, GDateMonth month);
EntryLink **diary_storage_manager_get_entry_links (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day);
gboolean diary_storage_manager_add_entry_link (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day, const gchar *link_type, const gchar *link_value);
gboolean diary_storage_manager_remove_entry_link (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day, const gchar *link_type);

G_END_DECLS

#endif /* !DIARY_STORAGE_MANAGER_H */
