/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2008-2009 <philip@tecnocode.co.uk>
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

#ifndef ALMANAH_STORAGE_MANAGER_H
#define ALMANAH_STORAGE_MANAGER_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "entry.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_STORAGE_MANAGER		(almanah_storage_manager_get_type ())
#define ALMANAH_STORAGE_MANAGER_ERROR		(almanah_storage_manager_error_quark ())

G_DECLARE_FINAL_TYPE (AlmanahStorageManager, almanah_storage_manager, ALMANAH, STORAGE_MANAGER, GObject)

typedef enum {
	ALMANAH_STORAGE_MANAGER_ERROR_UNSUPPORTED,
	ALMANAH_STORAGE_MANAGER_ERROR_OPENING_FILE,
	ALMANAH_STORAGE_MANAGER_ERROR_CREATING_CONTEXT,
	ALMANAH_STORAGE_MANAGER_ERROR_DECRYPTING,
	ALMANAH_STORAGE_MANAGER_ERROR_ENCRYPTING,
	ALMANAH_STORAGE_MANAGER_ERROR_GETTING_KEY,
	ALMANAH_STORAGE_MANAGER_ERROR_RUNNING_QUERY,
	ALMANAH_STORAGE_MANAGER_ERROR_BAD_VERSION
} AlmanahStorageManagerError;

typedef struct {
	/*< private >*/
	gpointer /*sqlite3_stmt **/ statement;
	gboolean finished; /* TRUE if the query is finished and the iter has been cleaned up */
	gpointer user_data; /* to be used by #AlmanahStorageManager functions which need to associate data with a statement */
} AlmanahStorageManagerIter;

typedef void (*AlmanahStorageManagerSearchCallback) (AlmanahStorageManager *storage_manager, AlmanahEntry *entry, gpointer user_data);

GQuark almanah_storage_manager_error_quark (void);
AlmanahStorageManager *almanah_storage_manager_new (const gchar *filename, GSettings *settings) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

gboolean almanah_storage_manager_connect (AlmanahStorageManager *self, GError **error);
gboolean almanah_storage_manager_disconnect (AlmanahStorageManager *self, GError **error);

gboolean almanah_storage_manager_get_statistics (AlmanahStorageManager *self, guint *entry_count);

gboolean almanah_storage_manager_entry_exists (AlmanahStorageManager *self, GDate *date);
AlmanahEntry *almanah_storage_manager_get_entry (AlmanahStorageManager *self, GDate *date);
gboolean almanah_storage_manager_set_entry (AlmanahStorageManager *self, AlmanahEntry *entry);

void almanah_storage_manager_iter_init (AlmanahStorageManagerIter *iter);
AlmanahEntry *almanah_storage_manager_search_entries (AlmanahStorageManager *self, const gchar *search_string,
                                                      AlmanahStorageManagerIter *iter) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
void almanah_storage_manager_search_entries_async (AlmanahStorageManager *self, const gchar *search_string, GCancellable *cancellable,
                                                   AlmanahStorageManagerSearchCallback progress_callback, gpointer progress_user_data,
                                                   GDestroyNotify progress_user_data_destroy,
                                                   GAsyncReadyCallback callback_ready, gpointer user_data);
gint almanah_storage_manager_search_entries_async_finish (AlmanahStorageManager *self, GAsyncResult *result, GError **error);
AlmanahEntry *almanah_storage_manager_get_entries (AlmanahStorageManager *self,
                                                   AlmanahStorageManagerIter *iter) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

gboolean *almanah_storage_manager_get_month_marked_days (AlmanahStorageManager *self, GDateYear year, GDateMonth month, guint *num_days);
gboolean *almanah_storage_manager_get_month_important_days (AlmanahStorageManager *self, GDateYear year, GDateMonth month, guint *num_days);

const gchar *almanah_storage_manager_get_filename (AlmanahStorageManager *self);

gboolean almanah_storage_manager_entry_add_tag (AlmanahStorageManager *self, AlmanahEntry *entry, const gchar *tag);
gboolean almanah_storage_manager_entry_remove_tag (AlmanahStorageManager *self, AlmanahEntry *entry, const gchar *tag);
GList *almanah_storage_manager_entry_get_tags (AlmanahStorageManager *self, AlmanahEntry *entry);
gboolean almanah_storage_manager_entry_check_tag (AlmanahStorageManager *self, AlmanahEntry *entry, const gchar *tag);
GList *almanah_storage_manager_get_tags (AlmanahStorageManager *self);

G_END_DECLS

#endif /* !ALMANAH_STORAGE_MANAGER_H */
