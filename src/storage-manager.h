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

#ifndef DIARY_STORAGE_MANAGER_H
#define DIARY_STORAGE_MANAGER_H

#include <glib.h>
#include <glib-object.h>

#include "link.h"

G_BEGIN_DECLS

#define DIARY_TYPE_STORAGE_MANAGER		(diary_storage_manager_get_type ())
#define DIARY_STORAGE_MANAGER_ERROR		(diary_storage_manager_error_quark ())
#define DIARY_STORAGE_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DIARY_TYPE_STORAGE_MANAGER, DiaryStorageManager))
#define DIARY_STORAGE_MANAGER_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), DIARY_TYPE_STORAGE_MANAGER, DiaryStorageManagerClass))
#define DIARY_IS_STORAGE_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DIARY_TYPE_STORAGE_MANAGER))
#define DIARY_IS_STORAGE_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DIARY_TYPE_STORAGE_MANAGER))
#define DIARY_STORAGE_MANAGER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DIARY_TYPE_STORAGE_MANAGER, DiaryStorageManagerClass))

typedef struct _DiaryStorageManagerPrivate	DiaryStorageManagerPrivate;

typedef struct {
	GObject parent;
	DiaryStorageManagerPrivate *priv;
} DiaryStorageManager;

typedef struct {
	GObjectClass parent;
} DiaryStorageManagerClass;

typedef enum {
	DIARY_STORAGE_MANAGER_ERROR_UNSUPPORTED,
	DIARY_STORAGE_MANAGER_ERROR_OPENING_FILE,
	DIARY_STORAGE_MANAGER_ERROR_CREATING_CONTEXT,
	DIARY_STORAGE_MANAGER_ERROR_DECRYPTING,
	DIARY_STORAGE_MANAGER_ERROR_ENCRYPTING,
	DIARY_STORAGE_MANAGER_ERROR_GETTING_KEY
} DiaryStorageManagerError;

typedef enum {
	DIARY_ENTRY_EDITABLE = 1,
	DIARY_ENTRY_FUTURE = 2,
	DIARY_ENTRY_PAST = 0
} DiaryEntryEditable;

/* The number of days after which a diary entry requires confirmation to be edited */
#define DIARY_ENTRY_CUTOFF_AGE 14

typedef gint (*DiaryQueryCallback) (gpointer user_data, gint columns, gchar **data, gchar **column_names);

typedef struct {
	gchar **data;
	gint rows;
	gint columns;
} DiaryQueryResults;

GType diary_storage_manager_get_type (void);
GQuark diary_storage_manager_error_quark (void);
DiaryStorageManager *diary_storage_manager_new (const gchar *filename);

void diary_storage_manager_connect (DiaryStorageManager *self);
void diary_storage_manager_disconnect (DiaryStorageManager *self);

DiaryQueryResults *diary_storage_manager_query (DiaryStorageManager *self, const gchar *query, ...);
void diary_storage_manager_free_results (DiaryQueryResults *results);
gboolean diary_storage_manager_query_async (DiaryStorageManager *self, const gchar *query, const DiaryQueryCallback callback, gpointer user_data, ...);

/* TODO: Surely just passing in GDates to these functions would be easier? */
gboolean diary_storage_manager_get_statistics (DiaryStorageManager *self, guint *entry_count, guint *link_count, guint *character_count);

gboolean diary_storage_manager_entry_exists (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day);
DiaryEntryEditable diary_storage_manager_entry_is_editable (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day);
gchar *diary_storage_manager_get_entry (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day);
gboolean diary_storage_manager_set_entry (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day, const gchar *content);
guint diary_storage_manager_search_entries (DiaryStorageManager *self, const gchar *search_string, GDate *matches[]);

gboolean *diary_storage_manager_get_month_marked_days (DiaryStorageManager *self, GDateYear year, GDateMonth month);

DiaryLink **diary_storage_manager_get_entry_links (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day);
gboolean diary_storage_manager_add_entry_link (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day, const gchar *link_type, const gchar *link_value, const gchar *link_value2);
gboolean diary_storage_manager_remove_entry_link (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day, const gchar *link_type);

G_END_DECLS

#endif /* !DIARY_STORAGE_MANAGER_H */
