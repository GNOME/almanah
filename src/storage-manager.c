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
#include <glib/gi18n.h>
#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "interface.h"
#include "link.h"
#include "storage-manager.h"

static void diary_storage_manager_init (DiaryStorageManager *self);
static void diary_storage_manager_finalize (GObject *object);
static void diary_storage_manager_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void diary_storage_manager_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

static void create_tables (DiaryStorageManager *self);

struct _DiaryStorageManagerPrivate {
	gchar *filename;
	sqlite3 *connection;
};

enum {
	PROP_FILENAME = 1
};

G_DEFINE_TYPE (DiaryStorageManager, diary_storage_manager, G_TYPE_OBJECT)
#define DIARY_STORAGE_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), DIARY_TYPE_STORAGE_MANAGER, DiaryStorageManagerPrivate))

static void
diary_storage_manager_class_init (DiaryStorageManagerClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DiaryStorageManagerPrivate));

	gobject_class->set_property = diary_storage_manager_set_property;
	gobject_class->get_property = diary_storage_manager_get_property;
	gobject_class->finalize = diary_storage_manager_finalize;

	g_object_class_install_property (gobject_class, PROP_FILENAME,
				g_param_spec_string ("filename",
					"Database filename", "The path and filename for the SQLite database.",
					NULL,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
diary_storage_manager_init (DiaryStorageManager *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DIARY_TYPE_STORAGE_MANAGER, DiaryStorageManagerPrivate);
	self->priv->filename = NULL;
}

/**
 * diary_storage_manager_new:
 * @filename: database filename to open
 *
 * Creates a new #DiaryStorageManager, connected to the given database @filename.
 *
 * Return value: the new #DiaryStorageManager
 **/
DiaryStorageManager *
diary_storage_manager_new (const gchar *filename)
{
	return g_object_new (DIARY_TYPE_STORAGE_MANAGER, "filename", filename, NULL);
}

static void
diary_storage_manager_finalize (GObject *object)
{
	DiaryStorageManagerPrivate *priv = DIARY_STORAGE_MANAGER_GET_PRIVATE (object);

	sqlite3_close (priv->connection);
	g_free (priv->filename);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (diary_storage_manager_parent_class)->finalize (object);
}

static void
diary_storage_manager_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	DiaryStorageManagerPrivate *priv = DIARY_STORAGE_MANAGER_GET_PRIVATE (object);

	switch (property_id) {
	  	case PROP_FILENAME:
	  		g_value_set_string (value, g_strdup (priv->filename));
	  		break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
diary_storage_manager_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	DiaryStorageManagerPrivate *priv = DIARY_STORAGE_MANAGER_GET_PRIVATE (object);
	gchar *error_message;

	switch (property_id) {
		case PROP_FILENAME:
			priv->filename = g_strdup (g_value_get_string (value));

			if (sqlite3_open (priv->filename, &(priv->connection)) != SQLITE_OK) {
				error_message = g_strdup_printf (_("Could not open database \"%s\". SQLite provided the following error message: %s"), priv->filename, sqlite3_errmsg (priv->connection));
				diary_interface_error (error_message, diary->main_window);
				g_free (error_message);
				exit (1);
			}

			/* Can't hurt to create the tables now */
			create_tables ((DiaryStorageManager*) object);

			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

DiaryQueryResults *
diary_storage_manager_query (DiaryStorageManager *self, const gchar *query, ...)
{
	DiaryStorageManagerPrivate *priv = DIARY_STORAGE_MANAGER_GET_PRIVATE (self);
	gchar *error_message, *new_query;
	va_list params;
	DiaryQueryResults *results;

	va_start (params, query);
	new_query = sqlite3_vmprintf (query, params);
	va_end (params);

	results = g_slice_new (DiaryQueryResults);

	if (diary->debug)
		g_debug ("Database query: %s", new_query);
	if (sqlite3_get_table (priv->connection, new_query, &(results->data), &(results->rows), &(results->columns), NULL) != SQLITE_OK) {
		error_message = g_strdup_printf (_("Could not run query \"%s\". SQLite provided the following error message: %s"), new_query, sqlite3_errmsg (priv->connection));
		diary_interface_error (error_message, diary->main_window);
		g_free (error_message);
		g_free (new_query);
		exit (1);
	}

	g_free (new_query);

	return results;
}

void
diary_storage_manager_free_results (DiaryQueryResults *results)
{
	sqlite3_free_table (results->data);
	g_slice_free (DiaryQueryResults, results);
}

gboolean
diary_storage_manager_query_async (DiaryStorageManager *self, const gchar *query, const DiaryQueryCallback callback, gpointer user_data, ...)
{
	DiaryStorageManagerPrivate *priv = DIARY_STORAGE_MANAGER_GET_PRIVATE (self);
	gchar *error_message, *new_query;
	va_list params;

	va_start (params, user_data);
	new_query = sqlite3_vmprintf (query, params);
	va_end (params);

	if (diary->debug)
		g_debug ("Database query: %s", new_query);
	if (sqlite3_exec (priv->connection, new_query, callback, user_data, NULL) != SQLITE_OK) {
		error_message = g_strdup_printf (_("Could not run query \"%s\". SQLite provided the following error message: %s"), new_query, sqlite3_errmsg (priv->connection));
		diary_interface_error (error_message, diary->main_window);
		g_free (error_message);
		g_free (new_query);
		exit (1);
	}

	g_free (new_query);

	return TRUE;
}

static void
create_tables (DiaryStorageManager *self)
{
	/* Dates are stored in ISO 8601 format, sort of */
	guint i;
	const gchar *queries[] = {
		"CREATE TABLE IF NOT EXISTS entries (year INTEGER, month INTEGER, day INTEGER, content TEXT, PRIMARY KEY (year, month, day))",
		"CREATE TABLE IF NOT EXISTS entry_links (year INTEGER, month INTEGER, day INTEGER, link_type TEXT, link_value TEXT, link_value2 TEXT, PRIMARY KEY (year, month, day, link_type, link_value, link_value2))",
		"CREATE TABLE IF NOT EXISTS entry_attachments (year INTEGER, month INTEGER, day INTEGER, attachment_type TEXT, attachment_data BLOB, PRIMARY KEY (year, month, day, attachment_type))",
		NULL
	};

	i = 0;
	while (queries[i] != NULL)
		diary_storage_manager_query_async (self, queries[i++], NULL, NULL);
}

gboolean
diary_storage_manager_entry_is_editable (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day)
{
	GDate *current_date, *entry_date;
	gint days_between;

	current_date = g_date_new ();
	g_date_set_time_t (current_date, time (NULL));
	entry_date = g_date_new_dmy (day, month, year);

	/* Entries can't be edited before they've happened, or after 7 days after they've happened */
	days_between = g_date_days_between (entry_date, current_date);
	g_date_free (entry_date);
	g_date_free (current_date);

	if (days_between < 0 || days_between > 7)
		return FALSE;
	else
		return TRUE;
}

/* NOTE: Free results with g_free */
gchar *
diary_storage_manager_get_entry (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day)
{
	gchar *content;
	DiaryQueryResults *results;

	results = diary_storage_manager_query (self, "SELECT content FROM entries WHERE year = %u AND month = %u AND day = %u", year, month, day);

	if (results->rows != 1) {
		/* Invalid number of rows returned. */
		diary_storage_manager_free_results (results);
		return NULL;
	}

	content = g_strdup (results->data[1]);
	diary_storage_manager_free_results (results);

	return content;
}

gboolean
diary_storage_manager_set_entry (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day, const gchar *content)
{
	/* Make sure they're editable */
	if (diary_storage_manager_entry_is_editable (self, year, month, day) == FALSE)
		return TRUE;

	/* Can't nullify entries without permission */
	if (content == NULL || strcmp (content, "") == 0) {
		GDate *date;
		gchar date_string[100];
		GtkWidget *dialog;

		date = g_date_new_dmy (day, month, year);
		g_date_strftime (date_string, sizeof (date_string), "%A, %e %B %Y", date);

		dialog = gtk_message_dialog_new (GTK_WINDOW (diary->main_window),
							    GTK_DIALOG_MODAL,
							    GTK_MESSAGE_QUESTION,
							    GTK_BUTTONS_NONE,
							    _("Are you sure you want to delete this diary entry for %s?"),
							    date_string);
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					GTK_STOCK_DELETE, GTK_RESPONSE_ACCEPT,
					NULL);
		g_date_free (date);

		gtk_widget_show_all (dialog);
		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT) {
			gtk_widget_destroy (dialog);
			return TRUE;
		}
		gtk_widget_destroy (dialog);
	}

	diary_storage_manager_query_async (self, "REPLACE INTO entries (year, month, day, content) VALUES (%u, %u, %u, '%q')", NULL, NULL, year, month, day, content);

	return TRUE;
}

/* NOTE: Free results with g_slice_free */
gboolean *
diary_storage_manager_get_month_marked_days (DiaryStorageManager *self, GDateYear year, GDateMonth month)
{
	DiaryQueryResults *results;
	guint i;
	gboolean *days = g_slice_alloc0 (sizeof (gboolean) * 32);

	results = diary_storage_manager_query (self, "SELECT day FROM entries WHERE year = %u AND month = %u", year, month);

	for (i = 1; i <= results->rows; i++)
		days[atoi (results->data[i])] = TRUE;

	diary_storage_manager_free_results (results);

	return days;
}

/* NOTE: Free array with g_free and each element with g_slice_free, *after* freeing ->type and ->value with g_free */
DiaryLink **
diary_storage_manager_get_entry_links (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day)
{
	DiaryQueryResults *results;
	DiaryLink **links;
	guint i;

	results = diary_storage_manager_query (self, "SELECT link_type, link_value, link_value2 FROM entry_links WHERE year = %u AND month = %u AND day = %u", year, month, day);

	if (results->rows == 0) {
		diary_storage_manager_free_results (results);
		/* Return empty array */
		links = (DiaryLink**) g_new (gpointer, 1);
		links[0] = NULL;
		return links;
	}

	links = (DiaryLink**) g_new (gpointer, results->rows + 1);
	for (i = 0; i < results->rows; i++) {
		links[i] = g_slice_new (DiaryLink);
		links[i]->type = g_strdup (results->data[(i + 1) * results->columns]);
		links[i]->value = g_strdup (results->data[(i + 1) * results->columns + 1]);
		links[i]->value2 = g_strdup (results->data[(i + 1) * results->columns + 2]);
	}
	links[i] = NULL;

	diary_storage_manager_free_results (results);

	return links;
}

gboolean
diary_storage_manager_add_entry_link (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day, const gchar *link_type, const gchar *link_value, const gchar *link_value2)
{
	g_assert (diary_validate_link_type (link_type));
	if (link_value2 == NULL)
		return diary_storage_manager_query_async (self, "REPLACE INTO entry_links (year, month, day, link_type, link_value) VALUES (%u, %u, %u, '%q', '%q')", NULL, NULL, year, month, day, link_type, link_value);
	else
		return diary_storage_manager_query_async (self, "REPLACE INTO entry_links (year, month, day, link_type, link_value, link_value2) VALUES (%u, %u, %u, '%q', '%q', '%q')", NULL, NULL, year, month, day, link_type, link_value, link_value2);
}

gboolean
diary_storage_manager_remove_entry_link (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day, const gchar *link_type)
{
	return diary_storage_manager_query_async (self, "DELETE FROM entry_links WHERE year = %u AND month = %u AND day = %u AND link_type = '%q'", NULL, NULL, year, month, day, link_type);
}
