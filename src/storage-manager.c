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
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <gpgme.h>
#include <sys/stat.h>
#include <string.h>

#include "main.h"
#include "interface.h"
#include "link.h"
#include "storage-manager.h"
#include "config.h"

#define ENCRYPTION_KEY_GCONF_PATH "/desktop/pgp/default_key"

static void diary_storage_manager_init (DiaryStorageManager *self);
static void diary_storage_manager_finalize (GObject *object);
static void diary_storage_manager_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void diary_storage_manager_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

struct _DiaryStorageManagerPrivate {
	gchar *filename, *plain_filename;
	sqlite3 *connection;
	gboolean decrypted;
};

enum {
	PROP_FILENAME = 1
};

G_DEFINE_TYPE (DiaryStorageManager, diary_storage_manager, G_TYPE_OBJECT)
#define DIARY_STORAGE_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), DIARY_TYPE_STORAGE_MANAGER, DiaryStorageManagerPrivate))

GQuark
diary_storage_manager_error_quark (void)
{
  return g_quark_from_static_string ("diary-storage-manager-error-quark");
}

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
					"Database filename", "The path and filename for the unencrypted SQLite database.",
					NULL,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
diary_storage_manager_init (DiaryStorageManager *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DIARY_TYPE_STORAGE_MANAGER, DiaryStorageManagerPrivate);
	self->priv->filename = NULL;
	self->priv->plain_filename = NULL;
	self->priv->decrypted = FALSE;
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

	g_free (priv->filename);
	g_free (priv->plain_filename);

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

	switch (property_id) {
		case PROP_FILENAME:
			priv->plain_filename = g_strdup (g_value_get_string (value));
			priv->filename = g_strjoin (NULL, priv->plain_filename, ".encrypted", NULL);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
create_tables (DiaryStorageManager *self)
{
	/* Dates are stored in ISO 8601 format...sort of */
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

#ifdef ENABLE_ENCRYPTION
typedef struct {
	GIOChannel *cipher_io_channel;
	GIOChannel *plain_io_channel;
	gpgme_data_t gpgme_cipher;
	gpgme_data_t gpgme_plain;
	gpgme_ctx_t context;
} CipherOperation;

static gboolean
prepare_gpgme (DiaryStorageManager *self, gboolean encrypting, CipherOperation *operation, GError **error)
{
	gpgme_error_t gpgme_error;

	/* Check OpenPGP's supported */
	gpgme_error = gpgme_engine_check_version (GPGME_PROTOCOL_OpenPGP);
	if (gpgme_error != GPG_ERR_NO_ERROR) {
		g_set_error (error, DIARY_STORAGE_MANAGER_ERROR, DIARY_STORAGE_MANAGER_ERROR_UNSUPPORTED,
			     _("GPGME doesn't support OpenPGP: %s"),
			     gpgme_strerror (gpgme_error));
		return FALSE;
	}

	/* Set up for the operation */
	gpgme_error = gpgme_new (&(operation->context));
	if (gpgme_error != GPG_ERR_NO_ERROR) {
		g_set_error (error, DIARY_STORAGE_MANAGER_ERROR, DIARY_STORAGE_MANAGER_ERROR_CREATING_CONTEXT,
			     _("Error creating cipher context: %s"),
			     gpgme_strerror (gpgme_error));
		return FALSE;
	}

	gpgme_set_protocol (operation->context, GPGME_PROTOCOL_OpenPGP);
	gpgme_set_armor (operation->context, TRUE);
	gpgme_set_textmode (operation->context, FALSE);

	return TRUE;
}

static gboolean
open_db_files (DiaryStorageManager *self, gboolean encrypting, CipherOperation *operation, GError **error)
{
	GError *io_error = NULL;
	gpgme_error_t gpgme_error;

	/* Open the encrypted file */
	operation->cipher_io_channel = g_io_channel_new_file (self->priv->filename, encrypting ? "w" : "r", &io_error);
	if (operation->cipher_io_channel == NULL) {
		g_propagate_error (error, io_error);
		return FALSE;
	}

	/* Pass it to GPGME */
	gpgme_error = gpgme_data_new_from_fd (&(operation->gpgme_cipher), g_io_channel_unix_get_fd (operation->cipher_io_channel));
	if (gpgme_error != GPG_ERR_NO_ERROR) {
		g_set_error (error, DIARY_STORAGE_MANAGER_ERROR, DIARY_STORAGE_MANAGER_ERROR_OPENING_FILE,
			     _("Error opening encrypted database file \"%s\": %s"),
			     self->priv->filename,
			     gpgme_strerror (gpgme_error));
		return FALSE;
	}

	/* Open/Create the plain file */
	operation->plain_io_channel = g_io_channel_new_file (self->priv->plain_filename, encrypting ? "r" : "w", &io_error);
	if (operation->plain_io_channel == NULL) {
		g_propagate_error (error, io_error);
		return FALSE;
	}

	/* Ensure the permissions are restricted to only the current user */
	fchmod (g_io_channel_unix_get_fd (operation->plain_io_channel), S_IRWXU);

	/* Pass it to GPGME */
	gpgme_error = gpgme_data_new_from_fd (&(operation->gpgme_plain), g_io_channel_unix_get_fd (operation->plain_io_channel));
	if (gpgme_error != GPG_ERR_NO_ERROR) {
		g_set_error (error, DIARY_STORAGE_MANAGER_ERROR, DIARY_STORAGE_MANAGER_ERROR_OPENING_FILE,
			     _("Error opening plain database file \"%s\": %s"),
			     self->priv->plain_filename,
			     gpgme_strerror (gpgme_error));
		return FALSE;
	}

	return TRUE;
}

static void
cipher_operation_free (CipherOperation *operation)
{
	gpgme_data_release (operation->gpgme_cipher);
	gpgme_data_release (operation->gpgme_plain);

	if (operation->cipher_io_channel != NULL) {
		g_io_channel_flush (operation->cipher_io_channel, NULL);
		g_io_channel_unref (operation->cipher_io_channel);
	}

	if (operation->plain_io_channel != NULL) {
		g_io_channel_shutdown (operation->plain_io_channel, TRUE, NULL);
		g_io_channel_unref (operation->plain_io_channel);
	}

	gpgme_signers_clear (operation->context);
	gpgme_release (operation->context);
	g_free (operation);
}

static gboolean
database_idle_cb (CipherOperation *operation)
{
	gpgme_error_t gpgme_error;

	if (!(gpgme_wait (operation->context, &gpgme_error, FALSE) == NULL && gpgme_error == 0)) {
		/* Finished! */
		cipher_operation_free (operation);

		if (diary->quitting == TRUE)
			diary_quit_real ();

		return FALSE;
	}

	return TRUE;
}

static gboolean
decrypt_database (DiaryStorageManager *self, GError **error)
{
	GError *preparation_error = NULL;
	CipherOperation *operation;
	gpgme_error_t gpgme_error;

	operation = g_new0 (CipherOperation, 1);

	/* Set up */
	if (prepare_gpgme (self, FALSE, operation, &preparation_error) != TRUE ||
	    open_db_files (self, FALSE, operation, &preparation_error) != TRUE) {
		cipher_operation_free (operation);
		g_propagate_error (error, preparation_error);
		return FALSE;
	}

	/* Decrypt and verify! */
	gpgme_error = gpgme_op_decrypt_verify (operation->context, operation->gpgme_cipher, operation->gpgme_plain);
	if (gpgme_error != GPG_ERR_NO_ERROR) {
		cipher_operation_free (operation);
		g_set_error (error, DIARY_STORAGE_MANAGER_ERROR, DIARY_STORAGE_MANAGER_ERROR_DECRYPTING,
			     _("Error decrypting database: %s"),
			     gpgme_strerror (gpgme_error));
		return FALSE;
	}

	/* Do this one synchronously */
	cipher_operation_free (operation);

	return TRUE;
}

static gboolean
encrypt_database (DiaryStorageManager *self, const gchar *encryption_key, GError **error)
{
	GError *preparation_error = NULL;
	CipherOperation *operation;
	gpgme_error_t gpgme_error;
	gpgme_key_t gpgme_keys[2] = { NULL, };

	operation = g_new0 (CipherOperation, 1);

	/* Set up */
	if (prepare_gpgme (self, TRUE, operation, &preparation_error) != TRUE) {
		cipher_operation_free (operation);
		g_propagate_error (error, preparation_error);
		return FALSE;
	}

	/* Set up signing and the recipient */
	gpgme_error = gpgme_get_key (operation->context, encryption_key, &gpgme_keys[0], FALSE);
	if (gpgme_error != GPG_ERR_NO_ERROR || gpgme_keys[0] == NULL) {
		cipher_operation_free (operation);
		g_set_error (error, DIARY_STORAGE_MANAGER_ERROR, DIARY_STORAGE_MANAGER_ERROR_GETTING_KEY,
			     _("Error getting encryption key: %s"),
			     gpgme_strerror (gpgme_error));
		return FALSE;
	}

	gpgme_signers_add (operation->context, gpgme_keys[0]);

	if (open_db_files (self, TRUE, operation, &preparation_error) != TRUE) {
		cipher_operation_free (operation);
		g_propagate_error (error, preparation_error);
		return FALSE;
	}

	/* Encrypt and sign! */
	gpgme_error = gpgme_op_encrypt_sign_start (operation->context, gpgme_keys, 0, operation->gpgme_plain, operation->gpgme_cipher);
	gpgme_key_unref (gpgme_keys[0]);

	if (gpgme_error != GPG_ERR_NO_ERROR) {
		cipher_operation_free (operation);

		g_set_error (error, DIARY_STORAGE_MANAGER_ERROR, DIARY_STORAGE_MANAGER_ERROR_ENCRYPTING,
			     _("Error encrypting database: %s"),
			     gpgme_strerror (gpgme_error));
		return FALSE;
	}

	/* The operation will be completed in the idle function */
	g_idle_add ((GSourceFunc) database_idle_cb, operation);

	/* Delete the plain file and wait for the idle function to quit us */
	if (g_unlink (self->priv->plain_filename) != 0) {
		g_set_error (error, DIARY_STORAGE_MANAGER_ERROR, DIARY_STORAGE_MANAGER_ERROR_ENCRYPTING,
			     _("Could not delete plain database file \"%s\"."),
			     self->priv->plain_filename);
		return FALSE;
	}

	return TRUE;
}

static gchar *
get_encryption_key (void)
{
	gchar **key_parts;
	guint i;
	gchar *encryption_key;

	encryption_key = gconf_client_get_string (diary->gconf_client, ENCRYPTION_KEY_GCONF_PATH, NULL);
	if (encryption_key == NULL || encryption_key[0] == '\0')
		return NULL;

	/* Key is generally in the form openpgp:FOOBARKEY, and GPGME doesn't
	 * like the openpgp: prefix, so it must be removed. */
	key_parts = g_strsplit (encryption_key, ":", 2);
	g_free (encryption_key);

	for (i = 0; key_parts[i] != NULL; i++) {
		if (strcmp (key_parts[i], "openpgp") != 0)
			encryption_key = key_parts[i];
		else
			g_free (key_parts[i]);
	}
	g_free (key_parts);

	return encryption_key;
}
#endif /* ENABLE_ENCRYPTION */

void
diary_storage_manager_connect (DiaryStorageManager *self)
{
#ifdef ENABLE_ENCRYPTION
	/* If we're decrypting, don't bother if the cipher file doesn't exist
	 * (i.e. the database hasn't yet been created). */
	if (g_file_test (self->priv->filename, G_FILE_TEST_IS_REGULAR) == TRUE) {
		GError *error = NULL;

		/* If both files exist, throw an error. We can't be sure which is the corrupt one,
		 * and attempting to go any further may jeopardise the good one. */
		if (g_file_test (self->priv->plain_filename, G_FILE_TEST_IS_REGULAR) == TRUE) {
			gchar *error_message = g_strdup_printf (_("Both an encrypted and plaintext version of the database exist as \"%s\" and \"%s\", and one is likely corrupt. Please delete the corrupt one before continuing."),
							self->priv->filename, 
							self->priv->plain_filename);
			diary_interface_error (error_message, diary->main_window);
			g_free (error_message);
			diary_quit ();
		}

		/* Decrypt the database, or display an error if that fails (but not if it
		 * fails due to a missing encrypted DB file --- just fall through and
		 * try to open the plain DB file in that case). */
		if (decrypt_database (self, &error) != TRUE && error->code != G_FILE_ERROR_NOENT) {
			diary_interface_error (error->message, diary->main_window);
			g_error_free (error);
			diary_quit ();
		}
	}

	self->priv->decrypted = TRUE;
#else
	self->priv->decrypted = FALSE;
#endif /* ENABLE_ENCRYPTION */

	/* Open the plain database */
	if (sqlite3_open (self->priv->plain_filename, &(self->priv->connection)) != SQLITE_OK) {
		gchar *error_message = g_strdup_printf (_("Could not open database \"%s\". SQLite provided the following error message: %s"),
							self->priv->filename, 
							sqlite3_errmsg (self->priv->connection));
		diary_interface_error (error_message, diary->main_window);
		g_free (error_message);
		diary_quit ();
	}

	/* Can't hurt to create the tables now */
	create_tables (self);
}

void
diary_storage_manager_disconnect (DiaryStorageManager *self)
{
#ifdef ENABLE_ENCRYPTION
	gchar *encryption_key;
	GError *error = NULL;
#endif /* ENABLE_ENCRYPTION */

	/* Close the DB connection */
	sqlite3_close (self->priv->connection);

	if (self->priv->decrypted == FALSE) {
		if (diary->quitting == TRUE)
			diary_quit_real ();
		return;
	}

#ifdef ENABLE_ENCRYPTION
	encryption_key = get_encryption_key ();
	if (encryption_key == NULL) {
		g_warning (_("Error getting encryption key: GConf key \"%s\" invalid or empty."), ENCRYPTION_KEY_GCONF_PATH);
		if (diary->quitting == TRUE)
			diary_quit_real ();
		return;
	}

	/* Encrypt the plain DB file */
	if (encrypt_database (self, encryption_key, &error) != TRUE) {
		if (error->code == DIARY_STORAGE_MANAGER_ERROR_GETTING_KEY) {
			/* Log an error about being unable to get the key
			 * then continue without encrypting. */
			g_warning (error->message);
		} else {
			/* Display an error */
			diary_interface_error (error->message, diary->main_window);
		}

		g_error_free (error);

		if (diary->quitting == TRUE)
			diary_quit_real ();
	}

	g_free (encryption_key);
#endif /* ENABLE_ENCRYPTION */
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
		sqlite3_free (new_query);
		diary_quit ();
	}

	sqlite3_free (new_query);

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
		sqlite3_free (new_query);
		diary_quit ();
	}

	sqlite3_free (new_query);

	return TRUE;
}

gboolean
diary_storage_manager_get_statistics (DiaryStorageManager *self, guint *entry_count, guint *link_count, guint *character_count)
{
	DiaryQueryResults *results;

	/* Get the number of entries and the number of letters */
	results = diary_storage_manager_query (self, "SELECT COUNT (year), SUM (LENGTH (content)) FROM entries");
	if (results->rows != 1) {
		*entry_count = 0;
		*character_count = 0;
		*link_count = 0;

		diary_storage_manager_free_results (results);
		return FALSE;
	} else {
		*entry_count = atoi (results->data[2]);
		if (*entry_count == 0) {
			*character_count = 0;
			*link_count = 0;
			return TRUE;
		}

		*character_count = atoi (results->data[3]);
	}
	diary_storage_manager_free_results (results);

	/* Get the number of links */
	results = diary_storage_manager_query (self, "SELECT COUNT (year) FROM entry_links");
	if (results->rows != 1) {
		*link_count = 0;

		diary_storage_manager_free_results (results);
		return FALSE;
	} else {
		*link_count = atoi (results->data[1]);
	}
	diary_storage_manager_free_results (results);

	return TRUE;
}

gboolean
diary_storage_manager_entry_is_editable (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day)
{
	GDate current_date, entry_date;
	gint days_between;

	g_date_set_time_t (&current_date, time (NULL));
	g_date_set_dmy (&entry_date, day, month, year);

	/* Entries can't be edited before they've happened, or after 14 days after they've happened */
	days_between = g_date_days_between (&entry_date, &current_date);

	if (days_between < 0 || days_between > 14)
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

/**
 * diary_storage_manager_set_entry:
 * @self: a #DiaryStorageManager
 * @year: the entry's year
 * @month: the entry's month
 * @day: the entry's day
 * @content: the content for the entry
 *
 * Saves the @content for the specified entry in the database. If
 * @content is empty or %NULL, it will ask if the user wants to delete
 * the entry for that date. It will return %TRUE if the content is
 * non-empty, and %FALSE otherwise.
 *
 * Return value: %TRUE if the entry is non-empty
 **/
gboolean
diary_storage_manager_set_entry (DiaryStorageManager *self, GDateYear year, GDateMonth month, GDateDay day, const gchar *content)
{
	/* Make sure they're editable */
	if (diary_storage_manager_entry_is_editable (self, year, month, day) == FALSE)
		return TRUE;

	/* Can't nullify entries without permission */
	if (content == NULL || content[0] == '\0') {
		GDate date;
		gchar date_string[100];
		GtkWidget *dialog;

		g_date_set_dmy (&date, day, month, year);
		g_date_strftime (date_string, sizeof (date_string), "%A, %e %B %Y", &date);

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

		gtk_widget_show_all (dialog);
		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT) {
			gtk_widget_destroy (dialog);
			return FALSE;
		}

		diary_storage_manager_query_async (self, "DELETE FROM entries WHERE year = %u AND month = %u AND day = %u", NULL, NULL, year, month, day);
		gtk_widget_destroy (dialog);

		return FALSE;
	}

	diary_storage_manager_query_async (self, "REPLACE INTO entries (year, month, day, content) VALUES (%u, %u, %u, '%q')", NULL, NULL, year, month, day, content);

	return TRUE;
}

/**
 * diary_storage_manager_search_entries:
 * @self: a #DiaryStorageManager
 * @search_string: string for which to search in entry content
 * @matches: return location for the results
 *
 * Searches for @search_string in the content in entries in the
 * database, and returns a the number of results. The results
 * themselves are returned in @matches as an array of #GDates.
 *
 * If there are no results, @matches will be set to %NULL. It
 * must otherwise be freed with g_free().
 *
 * Return value: number of results
 **/
guint
diary_storage_manager_search_entries (DiaryStorageManager *self, const gchar *search_string, GDate *matches[])
{
	DiaryQueryResults *results;
	guint i;

	results = diary_storage_manager_query (self, "SELECT day, month, year FROM entries WHERE content LIKE '%%%q%%'", search_string);

	/* No results? */
	if (results->rows < 1) {
		diary_storage_manager_free_results (results);
		*matches = NULL;
		return 0;
	}

	/* Allocate and set the results */
	*matches = g_new0 (GDate, results->rows);

	for (i = 0; i < results->rows; i++) {
		g_date_set_dmy (&((*matches)[i]),
				(GDateDay) atoi (results->data[(i + 1) * results->columns]),
				(GDateMonth) atoi (results->data[(i + 1) * results->columns + 1]),
				(GDateYear) atoi (results->data[(i + 1) * results->columns + 2]));
	}

	diary_storage_manager_free_results (results);

	return i;
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
