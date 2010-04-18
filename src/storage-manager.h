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

#include "entry.h"
#include "definition.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_STORAGE_MANAGER		(almanah_storage_manager_get_type ())
#define ALMANAH_STORAGE_MANAGER_ERROR		(almanah_storage_manager_error_quark ())
#define ALMANAH_STORAGE_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_STORAGE_MANAGER, AlmanahStorageManager))
#define ALMANAH_STORAGE_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_STORAGE_MANAGER, AlmanahStorageManagerClass))
#define ALMANAH_IS_STORAGE_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_STORAGE_MANAGER))
#define ALMANAH_IS_STORAGE_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_STORAGE_MANAGER))
#define ALMANAH_STORAGE_MANAGER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_STORAGE_MANAGER, AlmanahStorageManagerClass))

typedef struct _AlmanahStorageManagerPrivate	AlmanahStorageManagerPrivate;

typedef struct {
	GObject parent;
	AlmanahStorageManagerPrivate *priv;
} AlmanahStorageManager;

typedef struct {
	GObjectClass parent;
} AlmanahStorageManagerClass;

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

typedef gint (*AlmanahQueryCallback) (gpointer user_data, gint columns, gchar **data, gchar **column_names);

typedef struct {
	gchar **data;
	gint rows;
	gint columns;
} AlmanahQueryResults;

GType almanah_storage_manager_get_type (void);
GQuark almanah_storage_manager_error_quark (void);
AlmanahStorageManager *almanah_storage_manager_new (const gchar *filename);

gboolean almanah_storage_manager_connect (AlmanahStorageManager *self, GError **error);
gboolean almanah_storage_manager_disconnect (AlmanahStorageManager *self, GError **error);

AlmanahQueryResults *almanah_storage_manager_query (AlmanahStorageManager *self, const gchar *query, GError **error, ...);
void almanah_storage_manager_free_results (AlmanahQueryResults *results);
gboolean almanah_storage_manager_query_async (AlmanahStorageManager *self, const gchar *query, const AlmanahQueryCallback callback, gpointer user_data, GError **error, ...);

gboolean almanah_storage_manager_get_statistics (AlmanahStorageManager *self, guint *entry_count, guint *definition_count);

gboolean almanah_storage_manager_entry_exists (AlmanahStorageManager *self, GDate *date);
AlmanahEntry *almanah_storage_manager_get_entry (AlmanahStorageManager *self, GDate *date);
gboolean almanah_storage_manager_set_entry (AlmanahStorageManager *self, AlmanahEntry *entry);
gint almanah_storage_manager_search_entries (AlmanahStorageManager *self, const gchar *search_string, GDate *matches[]);
GSList *almanah_storage_manager_get_entries (AlmanahStorageManager *self) G_GNUC_WARN_UNUSED_RESULT;

gboolean *almanah_storage_manager_get_month_marked_days (AlmanahStorageManager *self, GDateYear year, GDateMonth month, guint *num_days);
gboolean *almanah_storage_manager_get_month_important_days (AlmanahStorageManager *self, GDateYear year, GDateMonth month, guint *num_days);

GSList *almanah_storage_manager_get_definitions (AlmanahStorageManager *self);
AlmanahDefinition *almanah_storage_manager_get_definition (AlmanahStorageManager *self, const gchar *definition_text);
gboolean almanah_storage_manager_add_definition (AlmanahStorageManager *self, AlmanahDefinition *definition);
gboolean almanah_storage_manager_remove_definition (AlmanahStorageManager *self, const gchar *definition_text);

G_END_DECLS

#endif /* !ALMANAH_STORAGE_MANAGER_H */
