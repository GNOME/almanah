/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2008–2010 <philip@tecnocode.co.uk>
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

#include <config.h>
#include <errno.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "entry.h"
#include "storage-manager.h"
#include "vfs.h"

static void almanah_storage_manager_finalize (GObject *object);
static void almanah_storage_manager_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_storage_manager_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static gboolean simple_query (AlmanahStorageManager *self, const gchar *query, GError **error, ...);

typedef struct {
	gchar *filename;
	sqlite3 *connection;
	GSettings *settings;
} AlmanahStorageManagerPrivate;

struct _AlmanahStorageManager {
	GObject parent;
};

enum {
	PROP_FILENAME = 1,
	PROP_SETTINGS
};

enum {
	SIGNAL_DISCONNECTED,
	SIGNAL_ENTRY_ADDED,
	SIGNAL_ENTRY_MODIFIED,
	SIGNAL_ENTRY_REMOVED,
	SIGNAL_ENTRY_TAG_ADDED,
	SIGNAL_ENTRY_TAG_REMOVED,
	LAST_SIGNAL
};

static guint storage_manager_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahStorageManager, almanah_storage_manager, G_TYPE_OBJECT)

GQuark
almanah_storage_manager_error_quark (void)
{
	return g_quark_from_static_string ("almanah-storage-manager-error-quark");
}

static void
almanah_storage_manager_class_init (AlmanahStorageManagerClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->set_property = almanah_storage_manager_set_property;
	gobject_class->get_property = almanah_storage_manager_get_property;
	gobject_class->finalize = almanah_storage_manager_finalize;

	g_object_class_install_property (gobject_class, PROP_FILENAME,
	                                 g_param_spec_string ("filename",
	                                                      "Database filename", "The path and filename for the unencrypted SQLite database.",
	                                                      NULL,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_SETTINGS,
	                                 g_param_spec_object ("settings",
	                                                      "The application settings object", "The application settings object to retrieve encryption key.",
	                                                      G_TYPE_SETTINGS,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	storage_manager_signals[SIGNAL_DISCONNECTED] = g_signal_new ("disconnected",
	                                                             G_TYPE_FROM_CLASS (klass),
	                                                             G_SIGNAL_RUN_LAST,
	                                                             0, NULL, NULL,
	                                                             NULL,
	                                                             G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
	storage_manager_signals[SIGNAL_ENTRY_ADDED] = g_signal_new ("entry-added",
	                                                            G_TYPE_FROM_CLASS (klass),
	                                                            G_SIGNAL_RUN_LAST,
	                                                            0, NULL, NULL,
	                                                            g_cclosure_marshal_VOID__OBJECT,
	                                                            G_TYPE_NONE, 1, ALMANAH_TYPE_ENTRY);
	storage_manager_signals[SIGNAL_ENTRY_MODIFIED] = g_signal_new ("entry-modified",
	                                                               G_TYPE_FROM_CLASS (klass),
	                                                               G_SIGNAL_RUN_LAST,
	                                                               0, NULL, NULL,
	                                                               g_cclosure_marshal_VOID__OBJECT,
	                                                               G_TYPE_NONE, 1, ALMANAH_TYPE_ENTRY);
	storage_manager_signals[SIGNAL_ENTRY_REMOVED] = g_signal_new ("entry-removed",
	                                                              G_TYPE_FROM_CLASS (klass),
	                                                              G_SIGNAL_RUN_LAST,
	                                                              0, NULL, NULL,
	                                                              g_cclosure_marshal_VOID__BOXED,
	                                                              G_TYPE_NONE, 1, G_TYPE_DATE);
	storage_manager_signals[SIGNAL_ENTRY_TAG_ADDED] = g_signal_new ("entry-tag-added",
									G_TYPE_FROM_CLASS (klass),
									G_SIGNAL_RUN_LAST,
									0, NULL, NULL,
									NULL,
									G_TYPE_NONE, 2, ALMANAH_TYPE_ENTRY, G_TYPE_STRING);
	storage_manager_signals[SIGNAL_ENTRY_TAG_REMOVED] = g_signal_new ("entry-tag-removed",
									  G_TYPE_FROM_CLASS (klass),
									  G_SIGNAL_RUN_LAST,
									  0, NULL, NULL,
									  NULL,
									  G_TYPE_NONE, 2, ALMANAH_TYPE_ENTRY, G_TYPE_STRING);
}

static void
almanah_storage_manager_init (AlmanahStorageManager *self)
{
	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);

	priv->filename = NULL;
}

/**
 * almanah_storage_manager_new:
 * @filename: database filename to open
 * @settings: the %GSettings used to retrieve encryption key
 *
 * Creates a new #AlmanahStorageManager, connected to the given database @filename, using
 * GSettings to retrieve the encryption key configured by the user in order to encrypt
 * the database.
 *
 * Return value: the new #AlmanahStorageManager
 **/
AlmanahStorageManager *
almanah_storage_manager_new (const gchar *filename, GSettings *settings)
{
	AlmanahStorageManager *sm;

	sm = g_object_new (ALMANAH_TYPE_STORAGE_MANAGER,
	                   "filename", filename,
			   "settings", settings,
	                   NULL);

	return sm;
}

static void
almanah_storage_manager_finalize (GObject *object)
{
	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (ALMANAH_STORAGE_MANAGER (object));

	g_free (priv->filename);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_storage_manager_parent_class)->finalize (object);
}

static void
almanah_storage_manager_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (ALMANAH_STORAGE_MANAGER (object));

	switch (property_id) {
		case PROP_FILENAME:
			g_value_set_string (value, g_strdup (priv->filename));
			break;
		case PROP_SETTINGS:
			g_value_set_object (value, priv->settings);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_storage_manager_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (ALMANAH_STORAGE_MANAGER (object));

	switch (property_id) {
		case PROP_FILENAME:
			if (priv->filename)
				g_free (priv->filename);
			priv->filename = g_strdup (g_value_get_string (value));
			break;
		case PROP_SETTINGS:
			g_set_object (&priv->settings, g_value_get_object (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
create_tables (AlmanahStorageManager *self)
{
	/* Dates are stored in ISO 8601 format…sort of */
	guint i;
	const gchar *queries[] = {
		"CREATE TABLE IF NOT EXISTS entries (year INTEGER, month INTEGER, day INTEGER, content TEXT, PRIMARY KEY (year, month, day))",
		"ALTER TABLE entries ADD COLUMN is_important INTEGER", /* added in 0.7.0 */
		"ALTER TABLE entries ADD COLUMN edited_year INTEGER", /* added in 0.8.0 */
		"ALTER TABLE entries ADD COLUMN edited_month INTEGER", /* added in 0.8.0 */
		"ALTER TABLE entries ADD COLUMN edited_day INTEGER", /* added in 0.8.0 */
		"ALTER TABLE entries ADD COLUMN version INTEGER DEFAULT 1", /* added in 0.8.0 */
		"CREATE TABLE IF NOT EXISTS entry_tag (year INTEGER, month INTEGER, day INTEGER, tag TEXT)", /* added in 0.10.0 */
		"CREATE INDEX idx_tag ON entry_tag(tag)", /* added in 0.10.0, for information take a look at: http://www.sqlite.org/queryplanner.html */
		NULL
	};

	i = 0;
	while (queries[i] != NULL)
		simple_query (self, queries[i++], NULL);
}

gboolean
almanah_storage_manager_connect (AlmanahStorageManager *self, GError **error)
{
	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);

	/* Our beautiful SQLite VFS */
	almanah_vfs_init(priv->settings);

	/* Open the plain database */
	if (sqlite3_open_v2 (priv->filename, &(priv->connection), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "almanah") != SQLITE_OK) {
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_OPENING_FILE,
		             _("Could not open database \"%s\". SQLite provided the following error message: %s"),
		             priv->filename, sqlite3_errmsg (priv->connection));
		return FALSE;
	}

	/* Can't hurt to create the tables now */
	create_tables (self);

	return TRUE;
}

gboolean
almanah_storage_manager_disconnect (AlmanahStorageManager *self, __attribute__ ((unused)) GError **error)
{
	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);
	int sqlite_ret;

	sqlite_ret = sqlite3_close (priv->connection);

	if (sqlite_ret != SQLITE_OK)
		g_signal_emit (self, storage_manager_signals[SIGNAL_DISCONNECTED], 0, NULL, "Something goes wrong closing the database");
	else
		g_signal_emit (self, storage_manager_signals[SIGNAL_DISCONNECTED], 0, NULL, NULL);

	almanah_vfs_finish();

	return TRUE;
}

static gboolean
simple_query (AlmanahStorageManager *self, const gchar *query, GError **error, ...)
{
	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);
	gchar *new_query;
	va_list params;

	va_start (params, error);
	new_query = sqlite3_vmprintf (query, params);
	va_end (params);

	g_debug ("Database query: %s", new_query);

	if (sqlite3_exec (priv->connection, new_query, NULL, NULL, NULL) != SQLITE_OK) {
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_RUNNING_QUERY,
		             _("Could not run query \"%s\". SQLite provided the following error message: %s"),
		             new_query, sqlite3_errmsg (priv->connection));
		sqlite3_free (new_query);
		return FALSE;
	}

	sqlite3_free (new_query);

	return TRUE;
}

gboolean
almanah_storage_manager_get_statistics (AlmanahStorageManager *self, guint *entry_count)
{
	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);
	sqlite3_stmt *statement;

	*entry_count = 0;

	/* Get the number of entries and the number of letters */
	if (sqlite3_prepare_v2 (priv->connection, "SELECT COUNT (year) FROM entries", -1, &statement, NULL) != SQLITE_OK)
		return FALSE;

	if (sqlite3_step (statement) != SQLITE_ROW) {
		sqlite3_finalize (statement);
		return FALSE;
	}

	*entry_count = sqlite3_column_int (statement, 0);
	sqlite3_finalize (statement);

	return TRUE;
}

gboolean
almanah_storage_manager_entry_exists (AlmanahStorageManager *self, GDate *date)
{
	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);
	sqlite3_stmt *statement;
	gboolean exists = FALSE;

	if (sqlite3_prepare_v2 (priv->connection, "SELECT day FROM entries WHERE year = ? AND month = ? AND day = ? LIMIT 1", -1,
	                        &statement, NULL) != SQLITE_OK) {
		return FALSE;
	}

	sqlite3_bind_int (statement, 1, g_date_get_year (date));
	sqlite3_bind_int (statement, 2, g_date_get_month (date));
	sqlite3_bind_int (statement, 3, g_date_get_day (date));

	/* If there's a result, this'll return SQLITE_ROW; it'll return SQLITE_DONE otherwise */
	if (sqlite3_step (statement) == SQLITE_ROW)
		exists = TRUE;

	sqlite3_finalize (statement);

	return exists;
}

static AlmanahEntry *
build_entry_from_statement (sqlite3_stmt *statement)
{
	GDate date, last_edited;
	AlmanahEntry *entry;

	/* Assumes query for SELECT content, is_important, day, month, year, edited_day, edited_month, edited_year, version, ... FROM entries ... */

	/* Get the date */
	g_date_set_dmy (&date,
	                sqlite3_column_int (statement, 2),
	                sqlite3_column_int (statement, 3),
	                sqlite3_column_int (statement, 4));

	/* Get the content */
	entry = almanah_entry_new (&date);
	almanah_entry_set_data (entry, sqlite3_column_blob (statement, 0), sqlite3_column_bytes (statement, 0), sqlite3_column_int (statement, 8));
	almanah_entry_set_is_important (entry, (sqlite3_column_int (statement, 1) == 1) ? TRUE : FALSE);

	/* Set the last-edited date if possible (for backwards-compatibility, we have to assume that not all entries have valid last-edited dates set,
	 * since last-edited support was only added in 0.8.0). */
	if (g_date_valid_dmy (sqlite3_column_int (statement, 5),
	                      sqlite3_column_int (statement, 6),
	                      sqlite3_column_int (statement, 7)) == TRUE) {
		g_date_set_dmy (&last_edited,
		                sqlite3_column_int (statement, 5),
		                sqlite3_column_int (statement, 6),
		                sqlite3_column_int (statement, 7));
		almanah_entry_set_last_edited (entry, &last_edited);
	}

	return entry;
}

/**
 * almanah_storage_manager_get_entry:
 * @self: an #AlmanahStorageManager
 * @date: the date of the entry
 *
 * Gets the entry for the specified day from the database. If an entry can't be found it will return %NULL.
 *
 * Return value: an #AlmanahEntry or %NULL
 **/
AlmanahEntry *
almanah_storage_manager_get_entry (AlmanahStorageManager *self, GDate *date)
{
	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);
	AlmanahEntry *entry;
	sqlite3_stmt *statement;

	/* Prepare the statement */
	if (sqlite3_prepare_v2 (priv->connection,
	                        "SELECT content, is_important, day, month, year, edited_day, edited_month, edited_year, version FROM entries "
	                        "WHERE year = ? AND month = ? AND day = ?", -1,
	                        &statement, NULL) != SQLITE_OK) {
		return NULL;
	}

	/* Bind parameters */
	sqlite3_bind_int (statement, 1, g_date_get_year (date));
	sqlite3_bind_int (statement, 2, g_date_get_month (date));
	sqlite3_bind_int (statement, 3, g_date_get_day (date));

	/* Execute the statement */
	if (sqlite3_step (statement) != SQLITE_ROW) {
		sqlite3_finalize (statement);
		return NULL;
	}

	/* Get the data */
	entry = build_entry_from_statement (statement);
	sqlite3_finalize (statement);

	return entry;
}

/**
 * almanah_storage_manager_set_entry:
 * @self: an #AlmanahStorageManager
 * @entry: an #AlmanahEntry
 *
 * Saves the specified @entry in the database synchronously. If the @entry's content is empty, it will delete @entry's rows in the database.
 *
 * The entry's last-edited date should be manually updated before storing it in the database, if desired.
 *
 * Return value: %TRUE on success, %FALSE otherwise
 **/
gboolean
almanah_storage_manager_set_entry (AlmanahStorageManager *self, AlmanahEntry *entry)
{
	GDate date;

	almanah_entry_get_date (entry, &date);

	if (almanah_entry_is_empty (entry) == TRUE) {
		/* Delete the entry */
		gboolean success = simple_query (self, "DELETE FROM entries WHERE year = %u AND month = %u AND day = %u", NULL,
		                                 g_date_get_year (&date),
		                                 g_date_get_month (&date),
		                                 g_date_get_day (&date));

		/* Signal of the operation */
		g_signal_emit (self, storage_manager_signals[SIGNAL_ENTRY_REMOVED], 0, &date);

		return success;
	} else {
		AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);
		const guint8 *data;
		gsize length;
		sqlite3_stmt *statement;
		GDate last_edited;
		gboolean existed_before;
		guint version;

		existed_before = almanah_storage_manager_entry_exists (self, &date);

		/* Prepare the statement */
		if (sqlite3_prepare_v2 (priv->connection,
		                        "REPLACE INTO entries "
		                        "(year, month, day, content, is_important, edited_day, edited_month, edited_year, version) "
		                        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)", -1,
		                        &statement, NULL) != SQLITE_OK) {
			return FALSE;
		}

		/* Bind parameters */
		sqlite3_bind_int (statement, 1, g_date_get_year (&date));
		sqlite3_bind_int (statement, 2, g_date_get_month (&date));
		sqlite3_bind_int (statement, 3, g_date_get_day (&date));

		data = almanah_entry_get_data (entry, &length, &version);
		sqlite3_bind_blob (statement, 4, data, length, SQLITE_TRANSIENT);
		sqlite3_bind_int (statement, 9, version);

		sqlite3_bind_int (statement, 5, almanah_entry_is_important (entry));

		almanah_entry_get_last_edited (entry, &last_edited);
		sqlite3_bind_int (statement, 6, g_date_get_day (&last_edited));
		sqlite3_bind_int (statement, 7, g_date_get_month (&last_edited));
		sqlite3_bind_int (statement, 8, g_date_get_year (&last_edited));

		/* Execute the statement */
		if (sqlite3_step (statement) != SQLITE_DONE) {
			sqlite3_finalize (statement);
			return FALSE;
		}

		sqlite3_finalize (statement);

		/* Signal of the operation */
		if (existed_before == TRUE)
			g_signal_emit (self, storage_manager_signals[SIGNAL_ENTRY_MODIFIED], 0, entry);
		else
			g_signal_emit (self, storage_manager_signals[SIGNAL_ENTRY_ADDED], 0, entry);

		return TRUE;
	}
}

/**
 * almanah_storage_manager_iter_init:
 * @iter: an #AlmanahStorageManagerIter to initialise
 *
 * Initialises the given iterator so it can be used by #AlmanahStorageManager functions. Typically, initialisers are allocated on the stack, so need
 * explicitly initialising before being passed to functions such as almanah_storage_manager_get_entries().
 *
 * Since: 0.8.0
 **/
void
almanah_storage_manager_iter_init (AlmanahStorageManagerIter *iter)
{
	g_return_if_fail (iter != NULL);

	iter->statement = NULL;
	iter->user_data = NULL;
	iter->finished = FALSE;
}

typedef struct {
	GtkTextBuffer *text_buffer;
	gchar *search_string;
} SearchData;

/**
 * almanah_storage_manager_search_entries:
 * @self: an #AlmanahStorageManager
 * @search_string: string for which to search in entry content
 * @iter: an #AlmanahStorageManagerIter to keep track of the query
 *
 * Searches for @search_string in the content in entries in the database, and returns the results iteratively. @iter should be initialised with
 * almanah_storage_manager_iter_init() and passed to almanah_storage_manager_search_entries(). This will then return a matching #AlmanahEntry every
 * time it's called with the same @iter until it reaches the end of the result set, when it will return %NULL. It will also finish and return %NULL on
 * error or if there are no results.
 *
 * The results are returned in descending date order.
 *
 * Calling functions must get every result from the result set (i.e. not stop calling almanah_storage_manager_search_entries() until it returns
 * %NULL).
 *
 * Return value: an #AlmanahEntry, or %NULL; unref with g_object_unref()
 **/
AlmanahEntry *
almanah_storage_manager_search_entries (AlmanahStorageManager *self, const gchar *search_string, AlmanahStorageManagerIter *iter)
{
	sqlite3_stmt *statement;
	GtkTextBuffer *text_buffer;

	g_return_val_if_fail (ALMANAH_IS_STORAGE_MANAGER (self), NULL);
	g_return_val_if_fail (iter != NULL, NULL);
	g_return_val_if_fail (iter->statement != NULL || search_string != NULL, NULL);
	g_return_val_if_fail (iter->statement == NULL || iter->user_data != NULL, NULL);

	if (iter->finished == TRUE)
		return NULL;

	if (iter->statement == NULL) {
		AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);
		SearchData *data;

		/* Prepare the statement. */
		if (sqlite3_prepare_v2 (priv->connection,
					"SELECT e.content, e.is_important, e.day, e.month, e.year, e.edited_day, e.edited_month, e.edited_year, e.version, GROUP_CONCAT(et.tag) AS tags FROM entries AS e "
					"LEFT JOIN entry_tag AS et ON (e.day=et.day AND e.month=et.month AND e.year=et.year) "
					"GROUP BY e.year, e.month, e.day "
					"ORDER BY e.year DESC, e.month DESC, e.day DESC", -1,
					(sqlite3_stmt**) &(iter->statement), NULL) != SQLITE_OK) {
			return NULL;
		}

		/* Set up persistent data for the operation */
		data = g_slice_new (SearchData);
		data->text_buffer = gtk_text_buffer_new (NULL);
		data->search_string = g_strdup (search_string);
		iter->user_data = data;
	}

	statement = iter->statement;
	text_buffer = ((SearchData*) iter->user_data)->text_buffer;
	search_string = ((SearchData*) iter->user_data)->search_string;

	/* Execute the statement */
	switch (sqlite3_step (statement)) {
		case SQLITE_ROW: {
			GtkTextIter text_iter;
			AlmanahEntry *entry = build_entry_from_statement (statement);
			const gchar *tags = sqlite3_column_text (statement, 9);

			/* Deserialise the entry into our buffer */
			gtk_text_buffer_set_text (text_buffer, "", 0);
			if (almanah_entry_get_content (entry, text_buffer, TRUE, NULL) == FALSE) {
				/* Error: return the next entry instead */
				g_object_unref (entry);
				g_warning (_("Error deserializing entry into buffer while searching."));
				return almanah_storage_manager_search_entries (self, NULL, iter);
			}

			/* Perform the search */
			gtk_text_buffer_get_start_iter (text_buffer, &text_iter);
			if (gtk_text_iter_forward_search (&text_iter, search_string,
			                                  GTK_TEXT_SEARCH_VISIBLE_ONLY | GTK_TEXT_SEARCH_TEXT_ONLY | GTK_TEXT_SEARCH_CASE_INSENSITIVE,
			                                  NULL, NULL, NULL) == TRUE) {
				/* A match was found! */
				return entry;
			} else if (tags != NULL && (strstr (tags, search_string) != NULL)) {
				/* A match in an entry tag */
				return entry;
			}

			/* Free stuff up and return the next match instead */
			g_object_unref (entry);
			return almanah_storage_manager_search_entries (self, NULL, iter);
		}
		case SQLITE_DONE:
		case SQLITE_ERROR:
		case SQLITE_BUSY:
		case SQLITE_LOCKED:
		case SQLITE_NOMEM:
		case SQLITE_IOERR:
		case SQLITE_CORRUPT:
		default: {
			/* Clean up the iter and return */
			sqlite3_finalize (statement);
			iter->statement = NULL;
			g_object_unref (((SearchData*) iter->user_data)->text_buffer);
			g_free (((SearchData*) iter->user_data)->search_string);
			g_slice_free (SearchData, iter->user_data);
			iter->user_data = NULL;
			iter->finished = TRUE;

			return NULL;
		}
	}

	g_assert_not_reached ();
}

typedef struct {
	gchar *search_string;
	AlmanahStorageManagerSearchCallback progress_callback;
	gpointer progress_user_data;
	GDestroyNotify progress_user_data_destroy;
	guint count;
} SearchAsyncData;

static void
search_async_data_free (SearchAsyncData *data)
{
	g_free (data->search_string);

	g_slice_free (SearchAsyncData, data);
}

typedef struct {
	AlmanahStorageManagerSearchCallback callback;
	AlmanahStorageManager *storage_manager;
	AlmanahEntry *entry;
	gpointer user_data;
} ProgressCallbackData;

static void
progress_callback_data_free (ProgressCallbackData *data)
{
	g_object_unref (data->entry);
	g_object_unref (data->storage_manager);

	g_slice_free (ProgressCallbackData, data);
}

static gboolean
search_entry_async_progress_cb (ProgressCallbackData *data)
{
	data->callback (data->storage_manager, data->entry, data->user_data);

	return FALSE;
}

static void
search_entries_async_thread (GTask *task, AlmanahStorageManager *storage_manager, gpointer task_data, GCancellable *cancellable)
{
	AlmanahStorageManagerIter iter;
	AlmanahEntry *entry;
	SearchAsyncData *search_data = (SearchAsyncData *) task_data;
	ProgressCallbackData *progress_data;
	GError *error = NULL;

	almanah_storage_manager_iter_init (&iter);
	while ((entry = almanah_storage_manager_search_entries (storage_manager, search_data->search_string, &iter)) != NULL) {
		/* Don't do any unnecessary work */
		if (cancellable != NULL && g_cancellable_set_error_if_cancelled (cancellable, &error)) {
			g_task_return_error (task, error);
			return;
		}

		search_data->count++;

		/* Queue a progress callback for the task */
		progress_data = g_slice_new (ProgressCallbackData);
		progress_data->callback = search_data->progress_callback;
		progress_data->storage_manager = g_object_ref (storage_manager);
		progress_data->entry = g_object_ref (entry);
		progress_data->user_data = search_data->progress_user_data;

		/* We have to use G_PRIORITY_DEFAULT here to contend with the GAsyncReadyCallback for the whole search operation. All the progress
		 * callbacks must have been made before the finished callback. */
		g_idle_add_full (G_PRIORITY_DEFAULT, (GSourceFunc) search_entry_async_progress_cb,
		                 progress_data, (GDestroyNotify) progress_callback_data_free);
	}
}

/**
 * almanah_storage_manager_search_entries_async_finish:
 * @self: an #AlmanahStorageManager
 * @task: a #GTask
 * @error: a #GError or %NULL
 *
 * Finish an asynchronous search started with almanah_storage_manager_search_entries_async().
 *
 * Return value: the number of entries which matched the search string, or <code class="literal">-1</code> on error
 */
gint
almanah_storage_manager_search_entries_async_finish (AlmanahStorageManager *self, GAsyncResult *task, GError **error)
{
	SearchAsyncData *search_data = NULL;
	gint retval = -1;

	g_return_val_if_fail (ALMANAH_IS_STORAGE_MANAGER (self), -1);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (task), -1);
	g_return_val_if_fail (error == NULL || *error == NULL, -1);

	if (g_task_is_valid (task, G_OBJECT (self)) == FALSE) {
		return -1;
	}

	/* Check for errors */
	search_data = g_task_get_task_data (G_TASK (task));

	/* Extract the number of results */
	if (g_async_result_legacy_propagate_error (G_ASYNC_RESULT (task), error) == FALSE) {
		retval = search_data->count;
	}

	/* Notify of destruction of the user data. We have to do this here so that we can guarantee that all of the progress callbacks have
	 * been executed. */
	if (search_data != NULL && search_data->progress_user_data_destroy != NULL) {
		search_data->progress_user_data_destroy (search_data->progress_user_data);
	}

	return retval;
}

/**
 * almanah_storage_manager_search_entries_async:
 * @self: an #AlmanahStorageManager
 * @search_string: the string of search terms being queried against
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @progress_callback: (scope notified) (allow-none) (closure progress_user_data): a function to call for each task as it's found, or %NULL
 * @progress_user_data: (closure): data to pass to @progress_callback
 * @progress_user_data_destroy: (allow-none): a function to destroy the @progress_user_data when it will not be used any more, or %NULL
 * @callback: a #GAsyncReadyCallback to call once the search is complete
 * @user_data: the data to pass to @callback
 *
 * Launch an asynchronous search for @search_string in the content in entries in the database.
 *
 * When the @search_string was found in an entry, @progess_callback will be called with the #AlmanahEntry which was found.
 *
 * When the search finishes, @callback will be called, then you can call almanah_storage_manager_search_entries_async_finish() to get the number of
 * entries found in total by the operation.
 */
void
almanah_storage_manager_search_entries_async (AlmanahStorageManager *self, const gchar *search_string, GCancellable *cancellable,
                                              AlmanahStorageManagerSearchCallback progress_callback, gpointer progress_user_data,
                                              GDestroyNotify progress_user_data_destroy,
                                              GAsyncReadyCallback callback, gpointer user_data)
{
	g_autoptr (GTask) task = NULL;
	SearchAsyncData *search_data;

	g_return_if_fail (ALMANAH_IS_STORAGE_MANAGER (self));
	g_return_if_fail (search_string != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (callback != NULL);

	task = g_task_new (G_OBJECT (self), cancellable, callback, user_data);
	g_task_set_source_tag (task, almanah_storage_manager_search_entries_async);

	search_data = g_slice_new (SearchAsyncData);
	search_data->search_string = g_strdup (search_string);
	search_data->progress_callback = progress_callback;
	search_data->progress_user_data = progress_user_data;
	search_data->progress_user_data_destroy = progress_user_data_destroy;
	search_data->count = 0;

	g_task_set_task_data (task, search_data, (GDestroyNotify) search_async_data_free);
	g_task_run_in_thread (task, (GTaskThreadFunc) search_entries_async_thread);
}

/**
 * almanah_storage_manager_get_entries:
 * @self: an #AlmanahStorageManager
 * @iter: an #AlmanahStorageManagerIter to keep track of the query
 *
 * Iterates through every single #AlmanahEntry in the database using the given #AlmanahStorageManagerIter. @iter should be initialised with
 * almanah_storage_manager_iter_init() and passed to almanah_storage_manager_get_entries(). This will then return an #AlmanahEntry every time it's
 * called with the same @iter until it reaches the end of the result set, when it will return %NULL. It will also finish and return %NULL on error.
 *
 * Calling functions must get every result from the result set (i.e. not stop calling almanah_storage_manager_get_entries() until it returns %NULL).
 *
 * Return value: an #AlmanahEntry, or %NULL; unref with g_object_unref()
 **/
AlmanahEntry *
almanah_storage_manager_get_entries (AlmanahStorageManager *self, AlmanahStorageManagerIter *iter)
{
	sqlite3_stmt *statement;

	g_return_val_if_fail (ALMANAH_IS_STORAGE_MANAGER (self), NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	if (iter->finished == TRUE)
		return NULL;

	if (iter->statement == NULL) {
		AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);
		/* Prepare the statement */
		if (sqlite3_prepare_v2 (priv->connection,
		                        "SELECT content, is_important, day, month, year, edited_day, edited_month, edited_year, version "
		                        "FROM entries", -1,
		                        (sqlite3_stmt**) &(iter->statement), NULL) != SQLITE_OK) {
			return NULL;
		}
	}

	statement = iter->statement;

	/* Execute the statement */
	switch (sqlite3_step (statement)) {
		case SQLITE_ROW:
			return build_entry_from_statement (statement);
		case SQLITE_DONE:
		case SQLITE_ERROR:
		case SQLITE_BUSY:
		case SQLITE_LOCKED:
		case SQLITE_NOMEM:
		case SQLITE_IOERR:
		case SQLITE_CORRUPT:
		default: {
			/* Clean up the iter and return */
			sqlite3_finalize (statement);
			iter->statement = NULL;
			iter->finished = TRUE;

			return NULL;
		}
	}

	g_assert_not_reached ();
}

/* NOTE: Free results with g_free. Return value is 0-based. */
gboolean *
almanah_storage_manager_get_month_marked_days (AlmanahStorageManager *self, GDateYear year, GDateMonth month, guint *num_days)
{
	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);
	sqlite3_stmt *statement;
	gint i, result;
	gboolean *days;

	/* Build the result array */
	i = g_date_get_days_in_month (month, year);
	if (num_days != NULL)
		*num_days = i;
	days = g_malloc0 (sizeof (gboolean) * i);

	/* Prepare and run the query */
	if (sqlite3_prepare_v2 (priv->connection, "SELECT day FROM entries WHERE year = ? AND month = ?", -1, &statement, NULL) != SQLITE_OK) {
		g_free (days);
		return NULL;
	}

	sqlite3_bind_int (statement, 1, year);
	sqlite3_bind_int (statement, 2, month);

	/* For each day which is returned, mark it in the array of days */
	while ((result = sqlite3_step (statement)) == SQLITE_ROW)
		days[sqlite3_column_int (statement, 0) - 1] = TRUE;

	sqlite3_finalize (statement);

	if (result != SQLITE_DONE) {
		/* Error */
		g_free (days);
		return NULL;
	}

	return days;
}

/* NOTE: Free results with g_free. Return value is 0-based. */
gboolean *
almanah_storage_manager_get_month_important_days (AlmanahStorageManager *self, GDateYear year, GDateMonth month, guint *num_days)
{
	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);
	sqlite3_stmt *statement;
	gint i, result;
	gboolean *days;

	/* Build the result array */
	i = g_date_get_days_in_month (month, year);
	if (num_days != NULL)
		*num_days = i;
	days = g_malloc0 (sizeof (gboolean) * i);

	/* Prepare and run the query */
	if (sqlite3_prepare_v2 (priv->connection, "SELECT day FROM entries WHERE year = ? AND month = ? AND is_important = 1", -1,
	                        &statement, NULL) != SQLITE_OK) {
		g_free (days);
		return NULL;
	}

	sqlite3_bind_int (statement, 1, year);
	sqlite3_bind_int (statement, 2, month);

	/* For each day which is returned, mark it in the array of days */
	while ((result = sqlite3_step (statement)) == SQLITE_ROW)
		days[sqlite3_column_int (statement, 0) - 1] = TRUE;

	sqlite3_finalize (statement);

	if (result != SQLITE_DONE) {
		/* Error */
		g_free (days);
		return NULL;
	}

	return days;
}

const gchar *
almanah_storage_manager_get_filename (AlmanahStorageManager *self)
{
	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);

	return priv->filename;
}

/**
 * almanah_storage_manager_entry_add_tag:
 * @self: an #AlmanahStorageManager
 * @entry: an #AlmanahEntry
 * @tag: a string
 *
 * Append the string in @tag as a tag for the entry @entry. If the @tag is empty or the @entry don't be previuslly saved, returns %FALSE
 *
 * Return value: %TRUE on success, %FALSE otherwise
 */
gboolean
almanah_storage_manager_entry_add_tag (AlmanahStorageManager *self, AlmanahEntry *entry, const gchar *tag)
{
	GDate entry_date;
	sqlite3_stmt *statement;
	gint result_error;

	g_return_val_if_fail (ALMANAH_IS_STORAGE_MANAGER (self), FALSE);
	g_return_val_if_fail (ALMANAH_IS_ENTRY (entry), FALSE);
	g_return_val_if_fail (g_utf8_strlen (tag, 1) == 1, FALSE);

	almanah_entry_get_date (entry, &entry_date);
	if (g_date_valid (&entry_date) != TRUE) {
		g_debug ("Invalid entry date");
		return FALSE;
	}

	/* Don't duplicate tags */
	if (almanah_storage_manager_entry_check_tag (self, entry, tag)) {
		g_debug ("Duplicated tag now allowed");
		return FALSE;
	}

	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);

	if ((result_error = sqlite3_prepare_v2 (priv->connection,
						"INSERT INTO entry_tag (year, month, day, tag) VALUES (?, ?, ?, ?)",
						-1, &statement, NULL)) != SQLITE_OK) {
		g_debug ("Can't prepare statement. SQLite error code: %d", result_error);
		return FALSE;
	}

	sqlite3_bind_int (statement, 1, g_date_get_year (&entry_date));
	sqlite3_bind_int (statement, 2, g_date_get_month (&entry_date));
	sqlite3_bind_int (statement, 3, g_date_get_day (&entry_date));
	/* @TODO: STATIC or TRANSIENT */
	sqlite3_bind_text (statement, 4, tag, -1, SQLITE_STATIC);

	if (sqlite3_step (statement) != SQLITE_DONE) {
		sqlite3_finalize (statement);
		g_debug ("Can't save tag");
		return FALSE;
	}

	sqlite3_finalize (statement);

	g_signal_emit (self, storage_manager_signals[SIGNAL_ENTRY_TAG_ADDED], 0, entry, g_strdup (tag));

	return TRUE;
}

/**
 * almanah_storage_manager_entry_remove_tag:
 * @self: an #AlmanahStorageManager
 * @entry: an #AlmanahEntry
 * @tag: a string with the tag to be removed
 *
 * Remove the tag with the given string in @tag as a tag for the entry @entry.
 *
 * Return value: %TRUE on success, %FALSE otherwise
 */
gboolean
almanah_storage_manager_entry_remove_tag (AlmanahStorageManager *self, AlmanahEntry *entry, const gchar *tag)
{
	GDate date;
	gboolean result;

	g_return_val_if_fail (ALMANAH_IS_STORAGE_MANAGER (self), FALSE);
	g_return_val_if_fail (ALMANAH_IS_ENTRY (entry), FALSE);
	g_return_val_if_fail (g_utf8_strlen (tag, 1) == 1, FALSE);

	almanah_entry_get_date (entry, &date);

	result = simple_query (self, "DELETE FROM entry_tag WHERE year = %u AND month = %u AND day = %u AND tag = '%s'", NULL,
			       g_date_get_year (&date),
			       g_date_get_month (&date),
			       g_date_get_day (&date),
			       tag);

	if (result)
		g_signal_emit (self, storage_manager_signals[SIGNAL_ENTRY_TAG_REMOVED], 0, entry, tag);

	return result;
}

/**
 * almanah_storage_manager_entry_get_tags:
 * @self: an #AlmanahStorageManager
 * @entry: an #AlmanahEntry
 *
 * Gets the tags added to an entry by the user from the database.
 */
GList *
almanah_storage_manager_entry_get_tags (AlmanahStorageManager *self, AlmanahEntry *entry)
{
	GList *tags = NULL;
	GDate date;
	sqlite3_stmt *statement;
	gint result;

	g_return_val_if_fail (ALMANAH_IS_STORAGE_MANAGER (self), FALSE);
	g_return_val_if_fail (ALMANAH_IS_ENTRY (entry), FALSE);

	almanah_entry_get_date (entry, &date);
	if (g_date_valid (&date) != TRUE) {
		g_debug ("Invalid entry date");
		return NULL;
	}

	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);

	if (sqlite3_prepare_v2 (priv->connection,
				"SELECT DISTINCT tag FROM entry_tag WHERE year = ? AND month = ? AND day = ?",
				-1, &statement, NULL) != SQLITE_OK) {
		g_debug ("Can't prepare statement");
		return NULL;
	}

	sqlite3_bind_int (statement, 1, g_date_get_year (&date));
	sqlite3_bind_int (statement, 2, g_date_get_month (&date));
	sqlite3_bind_int (statement, 3, g_date_get_day (&date));

	while ((result = sqlite3_step (statement)) == SQLITE_ROW) {
		tags = g_list_append (tags, g_strdup ((const gchar*) sqlite3_column_text (statement, 0)));
	}

	sqlite3_finalize (statement);

	if (result != SQLITE_DONE) {
		g_debug ("Error querying for tags from database: %s", sqlite3_errmsg (priv->connection));
		g_list_free_full (tags, (GDestroyNotify) g_free);
		tags = NULL;
	}

	return tags;
}

/**
 * almanah_storage_manager_entry_check_tag:
 * @self: an #AlmanahStorageManager
 * @entry: an #AlmanahEntry to check into it
 * @tag: the tag to be checked
 *
 * Check if a tag has been added to an entry
 *
 * Return value: TRUE if the tag already added to the entry, FALSE otherwise
 */
gboolean
almanah_storage_manager_entry_check_tag (AlmanahStorageManager *self, AlmanahEntry *entry, const gchar *tag)
{
	gboolean result, q_result;
	sqlite3_stmt *statement;
	GDate date;

	g_return_val_if_fail (ALMANAH_IS_STORAGE_MANAGER (self), FALSE);
	g_return_val_if_fail (ALMANAH_IS_ENTRY (entry), FALSE);
	g_return_val_if_fail (g_utf8_strlen (tag, 1) == 1, FALSE);

	result = FALSE;

	almanah_entry_get_date (entry, &date);
	if (g_date_valid (&date) != TRUE) {
		g_debug ("Invalid entry date");
		return FALSE;
	}

	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);

	if (sqlite3_prepare_v2 (priv->connection, 
				"SELECT count(1) FROM entry_tag WHERE year = ? AND month = ? AND day = ? AND tag = ?",
				-1, &statement, NULL) != SQLITE_OK) {
		g_debug ("Can't prepare statement");
		return FALSE;
	}

	sqlite3_bind_int (statement, 1, g_date_get_year (&date));
	sqlite3_bind_int (statement, 2, g_date_get_month (&date));
	sqlite3_bind_int (statement, 3, g_date_get_day (&date));
	sqlite3_bind_text (statement, 4, tag, -1, SQLITE_STATIC);

	if ((q_result  = sqlite3_step (statement)) == SQLITE_ROW) {
		if (sqlite3_column_int (statement, 0) > 0)
			result = TRUE;
	}

	if (q_result != SQLITE_DONE) {
		g_debug ("Error querying for a tag from database: %s", sqlite3_errmsg (priv->connection));
	}

	sqlite3_finalize (statement);

	return result;
}


/**
 * almanah_storage_manager_get_tags:
 * @self: an #AlmanahStorageManager
 *
 * Gets all the tags added to entries by the user from the database.
 *
 * Return value: #GList with all the tags.
 */
GList *
almanah_storage_manager_get_tags (AlmanahStorageManager *self)
{
	GList *tags = NULL;
	sqlite3_stmt *statement;
	gint result;

	g_return_val_if_fail (ALMANAH_IS_STORAGE_MANAGER (self), FALSE);

	AlmanahStorageManagerPrivate *priv = almanah_storage_manager_get_instance_private (self);

	if ((result = sqlite3_prepare_v2 (priv->connection, "SELECT DISTINCT tag FROM entry_tag", -1, &statement, NULL)) != SQLITE_OK) {
		g_debug ("Can't prepare statement, error code: %d", result);
		return NULL;
	}

	while ((result = sqlite3_step (statement)) == SQLITE_ROW) {
		tags = g_list_append (tags, g_strdup (sqlite3_column_text (statement, 0)));
	}

	sqlite3_finalize (statement);

	if (result != SQLITE_DONE) {
		g_debug ("Error querying for tags from database: %s", sqlite3_errmsg (priv->connection));
		g_free (tags);
		tags = NULL;
	}

	return tags;
}
