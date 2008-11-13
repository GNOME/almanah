/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2008 <philip@tecnocode.co.uk>
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
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#ifdef ENABLE_ENCRYPTION
#include <gpgme.h>
#endif /* ENABLE_ENCRYPTION */

#include "main.h"
#include "entry.h"
#include "link.h"
#include "storage-manager.h"

static void almanah_storage_manager_init (AlmanahStorageManager *self);
static void almanah_storage_manager_finalize (GObject *object);
static void almanah_storage_manager_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_storage_manager_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

struct _AlmanahStorageManagerPrivate {
	gchar *filename, *plain_filename;
	sqlite3 *connection;
	gboolean decrypted;
};

enum {
	PROP_FILENAME = 1
};

enum {
	SIGNAL_DISCONNECTED,
	LAST_SIGNAL
};

static guint storage_manager_signals[LAST_SIGNAL] = { 0, };


G_DEFINE_TYPE (AlmanahStorageManager, almanah_storage_manager, G_TYPE_OBJECT)
#define ALMANAH_STORAGE_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_STORAGE_MANAGER, AlmanahStorageManagerPrivate))

GQuark
almanah_storage_manager_error_quark (void)
{
	return g_quark_from_static_string ("almanah-storage-manager-error-quark");
}

static void
almanah_storage_manager_class_init (AlmanahStorageManagerClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahStorageManagerPrivate));

	gobject_class->set_property = almanah_storage_manager_set_property;
	gobject_class->get_property = almanah_storage_manager_get_property;
	gobject_class->finalize = almanah_storage_manager_finalize;

	g_object_class_install_property (gobject_class, PROP_FILENAME,
				g_param_spec_string ("filename",
					"Database filename", "The path and filename for the unencrypted SQLite database.",
					NULL,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	storage_manager_signals[SIGNAL_DISCONNECTED] = g_signal_new ("disconnected",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				0, NULL, NULL,
				g_cclosure_marshal_VOID__OBJECT,
				G_TYPE_NONE, 1, ALMANAH_TYPE_STORAGE_MANAGER);
}

static void
almanah_storage_manager_init (AlmanahStorageManager *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_STORAGE_MANAGER, AlmanahStorageManagerPrivate);
	self->priv->filename = NULL;
	self->priv->plain_filename = NULL;
	self->priv->decrypted = FALSE;
}

/**
 * almanah_storage_manager_new:
 * @filename: database filename to open
 *
 * Creates a new #AlmanahStorageManager, connected to the given database @filename.
 *
 * Return value: the new #AlmanahStorageManager
 **/
AlmanahStorageManager *
almanah_storage_manager_new (const gchar *filename)
{
	return g_object_new (ALMANAH_TYPE_STORAGE_MANAGER, "filename", filename, NULL);
}

static void
almanah_storage_manager_finalize (GObject *object)
{
	AlmanahStorageManagerPrivate *priv = ALMANAH_STORAGE_MANAGER (object)->priv;

	g_free (priv->filename);
	g_free (priv->plain_filename);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_storage_manager_parent_class)->finalize (object);
}

static void
almanah_storage_manager_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahStorageManagerPrivate *priv = ALMANAH_STORAGE_MANAGER (object)->priv;

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
almanah_storage_manager_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahStorageManagerPrivate *priv = ALMANAH_STORAGE_MANAGER (object)->priv;

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
create_tables (AlmanahStorageManager *self)
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
		almanah_storage_manager_query_async (self, queries[i++], NULL, NULL, NULL);
}

#ifdef ENABLE_ENCRYPTION
typedef struct {
	AlmanahStorageManager *storage_manager;
	GIOChannel *cipher_io_channel;
	GIOChannel *plain_io_channel;
	gpgme_data_t gpgme_cipher;
	gpgme_data_t gpgme_plain;
	gpgme_ctx_t context;
} CipherOperation;

static gboolean
prepare_gpgme (AlmanahStorageManager *self, gboolean encrypting, CipherOperation *operation, GError **error)
{
	gpgme_error_t gpgme_error;

	/* Check OpenPGP's supported */
	gpgme_error = gpgme_engine_check_version (GPGME_PROTOCOL_OpenPGP);
	if (gpgme_error != GPG_ERR_NO_ERROR) {
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_UNSUPPORTED,
			     _("GPGME doesn't support OpenPGP: %s"),
			     gpgme_strerror (gpgme_error));
		return FALSE;
	}

	/* Set up for the operation */
	gpgme_error = gpgme_new (&(operation->context));
	if (gpgme_error != GPG_ERR_NO_ERROR) {
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_CREATING_CONTEXT,
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
open_db_files (AlmanahStorageManager *self, gboolean encrypting, CipherOperation *operation, GError **error)
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
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_OPENING_FILE,
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
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_OPENING_FILE,
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

	g_object_unref (operation->storage_manager);
	g_free (operation);
}

static gboolean
database_idle_cb (CipherOperation *operation)
{
	gpgme_error_t gpgme_error;

	if (!(gpgme_wait (operation->context, &gpgme_error, FALSE) == NULL && gpgme_error == 0)) {
		/* A slight assumption that we're disconnecting at this point (we're technically
		 * only encrypting), but a valid one. */
		g_signal_emit (operation->storage_manager, storage_manager_signals[SIGNAL_DISCONNECTED], 0, operation->storage_manager);

		/* Finished! */
		cipher_operation_free (operation);

		return FALSE;
	}

	return TRUE;
}

static gboolean
decrypt_database (AlmanahStorageManager *self, GError **error)
{
	GError *preparation_error = NULL;
	CipherOperation *operation;
	gpgme_error_t gpgme_error;

	operation = g_new0 (CipherOperation, 1);
	operation->storage_manager = g_object_ref (self);

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
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_DECRYPTING,
			     _("Error decrypting database: %s"),
			     gpgme_strerror (gpgme_error));
		return FALSE;
	}

	/* Do this one synchronously */
	cipher_operation_free (operation);

	return TRUE;
}

static gboolean
encrypt_database (AlmanahStorageManager *self, const gchar *encryption_key, GError **error)
{
	GError *preparation_error = NULL;
	CipherOperation *operation;
	gpgme_error_t gpgme_error;
	gpgme_key_t gpgme_keys[2] = { NULL, };

	operation = g_new0 (CipherOperation, 1);
	operation->storage_manager = g_object_ref (self);

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
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_GETTING_KEY,
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

		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_ENCRYPTING,
			     _("Error encrypting database: %s"),
			     gpgme_strerror (gpgme_error));
		return FALSE;
	}

	/* The operation will be completed in the idle function */
	g_idle_add ((GSourceFunc) database_idle_cb, operation);

	/* Delete the plain file and wait for the idle function to quit us */
	if (g_unlink (self->priv->plain_filename) != 0) {
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_ENCRYPTING,
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

	encryption_key = gconf_client_get_string (almanah->gconf_client, ENCRYPTION_KEY_GCONF_PATH, NULL);
	if (encryption_key == NULL || encryption_key[0] == '\0') {
		g_free (encryption_key);
		return NULL;
	}

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

gboolean
almanah_storage_manager_connect (AlmanahStorageManager *self, GError **error)
{
#ifdef ENABLE_ENCRYPTION
	struct stat encrypted_db_stat, plaintext_db_stat;

	g_stat (self->priv->filename, &encrypted_db_stat);

	/* If we're decrypting, don't bother if the cipher file doesn't exist
	 * (i.e. the database hasn't yet been created), or is empty (i.e. corrupt). */
	if (g_file_test (self->priv->filename, G_FILE_TEST_IS_REGULAR) == TRUE && encrypted_db_stat.st_size > 0) {
		GError *child_error = NULL;

		g_stat (self->priv->plain_filename, &plaintext_db_stat);

		/* Only decrypt the database if the plaintext database doesn't exist or is empty. If the plaintext
		 * database exists and is non-empty, don't decrypt --- just use that database. */
		if (g_file_test (self->priv->plain_filename, G_FILE_TEST_IS_REGULAR) != TRUE || plaintext_db_stat.st_size == 0) {
			/* Decrypt the database, or display an error if that fails (but not if it
			 * fails due to a missing encrypted DB file --- just fall through and
			 * try to open the plain DB file in that case). */
			if (decrypt_database (self, &child_error) != TRUE) {
				if (child_error->code != G_FILE_ERROR_NOENT) {
					g_propagate_error (error, child_error);
					return FALSE;
				}

				g_error_free (child_error);
			}
		}

		
	}

	self->priv->decrypted = TRUE;
#else
	self->priv->decrypted = FALSE;
#endif /* ENABLE_ENCRYPTION */

	/* Open the plain database */
	if (sqlite3_open (self->priv->plain_filename, &(self->priv->connection)) != SQLITE_OK) {
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_OPENING_FILE,
			     _("Could not open database \"%s\". SQLite provided the following error message: %s"),
			     self->priv->filename, 
			     sqlite3_errmsg (self->priv->connection));
		return FALSE;
	}

	/* Can't hurt to create the tables now */
	create_tables (self);

	return TRUE;
}

gboolean
almanah_storage_manager_disconnect (AlmanahStorageManager *self, GError **error)
{
#ifdef ENABLE_ENCRYPTION
	gchar *encryption_key;
	GError *child_error = NULL;
#endif /* ENABLE_ENCRYPTION */

	/* Close the DB connection */
	sqlite3_close (self->priv->connection);

	if (self->priv->decrypted == FALSE) {
		g_signal_emit (self, storage_manager_signals[SIGNAL_DISCONNECTED], 0, self);
		return TRUE;
	}

#ifdef ENABLE_ENCRYPTION
	encryption_key = get_encryption_key ();
	if (encryption_key == NULL) {
		g_message (_("Error getting encryption key: GConf key \"%s\" invalid or empty. Your diary will not be encrypted; please install Seahorse and set up a default key, or ignore this message."), ENCRYPTION_KEY_GCONF_PATH);
		g_signal_emit (self, storage_manager_signals[SIGNAL_DISCONNECTED], 0, self);
		return TRUE;
	}

	/* Encrypt the plain DB file */
	if (encrypt_database (self, encryption_key, &child_error) != TRUE) {
		if (child_error->code != ALMANAH_STORAGE_MANAGER_ERROR_GETTING_KEY) {
			/* Propagate the error */
			g_propagate_error (error, child_error);
			return FALSE;
		}

		/* Log an error about being unable to get the key
		 * then continue without encrypting. */
		g_warning ("%s", child_error->message);
		g_error_free (child_error);

		g_signal_emit (self, storage_manager_signals[SIGNAL_DISCONNECTED], 0, self);
	}

	g_free (encryption_key);
#endif /* ENABLE_ENCRYPTION */

	return TRUE;
}

AlmanahQueryResults *
almanah_storage_manager_query (AlmanahStorageManager *self, const gchar *query, GError **error, ...)
{
	AlmanahStorageManagerPrivate *priv = self->priv;
	gchar *new_query;
	va_list params;
	AlmanahQueryResults *results;

	va_start (params, error);
	new_query = sqlite3_vmprintf (query, params);
	va_end (params);

	results = g_new (AlmanahQueryResults, 1);

	if (almanah->debug)
		g_debug ("Database query: %s", new_query);
	if (sqlite3_get_table (priv->connection, new_query, &(results->data), &(results->rows), &(results->columns), NULL) != SQLITE_OK) {
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_RUNNING_QUERY,
			     _("Could not run query \"%s\". SQLite provided the following error message: %s"),
			     new_query,
			     sqlite3_errmsg (priv->connection));
		sqlite3_free (new_query);
		return NULL;
	}

	sqlite3_free (new_query);

	return results;
}

void
almanah_storage_manager_free_results (AlmanahQueryResults *results)
{
	sqlite3_free_table (results->data);
	g_free (results);
}

gboolean
almanah_storage_manager_query_async (AlmanahStorageManager *self, const gchar *query, const AlmanahQueryCallback callback, gpointer user_data, GError **error, ...)
{
	AlmanahStorageManagerPrivate *priv = self->priv;
	gchar *new_query;
	va_list params;

	va_start (params, error);
	new_query = sqlite3_vmprintf (query, params);
	va_end (params);

	if (almanah->debug)
		g_debug ("Database query: %s", new_query);
	if (sqlite3_exec (priv->connection, new_query, callback, user_data, NULL) != SQLITE_OK) {
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_RUNNING_QUERY,
			     _("Could not run query \"%s\". SQLite provided the following error message: %s"),
			     new_query,
			     sqlite3_errmsg (priv->connection));
		sqlite3_free (new_query);
		return FALSE;
	}

	sqlite3_free (new_query);

	return TRUE;
}

gboolean
almanah_storage_manager_get_statistics (AlmanahStorageManager *self, guint *entry_count, guint *link_count)
{
	AlmanahQueryResults *results;

	*entry_count = 0;
	*link_count = 0;

	/* Get the number of entries and the number of letters */
	results = almanah_storage_manager_query (self, "SELECT COUNT (year) FROM entries", NULL);
	if (results == NULL) {
		return FALSE;
	} else if (results->rows != 1) {
		almanah_storage_manager_free_results (results);
		return FALSE;
	} else {
		*entry_count = atoi (results->data[1]);
		if (*entry_count == 0) {
			*link_count = 0;
			almanah_storage_manager_free_results (results);
			return TRUE;
		}
	}
	almanah_storage_manager_free_results (results);

	/* Get the number of links */
	results = almanah_storage_manager_query (self, "SELECT COUNT (year) FROM entry_links", NULL);
	if (results == NULL) {
		return FALSE;
	} else if (results->rows != 1) {
		*link_count = 0;
		almanah_storage_manager_free_results (results);
		return FALSE;
	} else {
		*link_count = atoi (results->data[1]);
	}
	almanah_storage_manager_free_results (results);

	return TRUE;
}

gboolean
almanah_storage_manager_entry_exists (AlmanahStorageManager *self, GDate *date)
{
	AlmanahQueryResults *results;
	gboolean exists = FALSE;

	results = almanah_storage_manager_query (self, "SELECT day FROM entries WHERE year = %u AND month = %u AND day = %u LIMIT 1", NULL,
						 g_date_get_year (date),
						 g_date_get_month (date),
						 g_date_get_day (date));

	if (results == NULL)
		return FALSE;
	if (results->rows == 1)
		exists = TRUE;

	almanah_storage_manager_free_results (results);

	return exists;
}

/**
 * almanah_storage_manager_get_entry:
 * @self: a #AlmanahStorageManager
 * @date: the date of the entry
 *
 * Gets the entry for the specified day from the database.
 * If an entry can't be found it will return %NULL.
 *
 * Return value: an #AlmanahEntry or %NULL
 **/
AlmanahEntry *
almanah_storage_manager_get_entry (AlmanahStorageManager *self, GDate *date)
{
	AlmanahEntry *entry;
	sqlite3_stmt *statement;

	/* It's necessary to avoid our nice SQLite interface and use the sqlite3 API directly here
	 * as we can't otherwise reliably bind the data blob to the query --- if we pass it in as
	 * a string, it gets cut off at the first nul character, which could occur anywhere in
	 * the blob. */

	/* Prepare the statement */
	if (sqlite3_prepare_v2 (self->priv->connection,
				"SELECT content FROM entries WHERE year = ? AND month = ? AND day = ?", -1,
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
	entry = almanah_entry_new (date);
	almanah_entry_set_data (entry, sqlite3_column_blob (statement, 0), sqlite3_column_bytes (statement, 0));

	sqlite3_finalize (statement);

	return entry;
}

/**
 * almanah_storage_manager_set_entry:
 * @self: a #AlmanahStorageManager
 * @entry: an #AlmanahEntry
 *
 * Saves the specified @entry in the database synchronously.
 * If the @entry's content is empty, it will delete @entry's rows
 * in the database (as well as its links' rows).
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
		almanah_storage_manager_query_async (self, "DELETE FROM entries WHERE year = %u AND month = %u AND day = %u", NULL, NULL, NULL,
						     g_date_get_year (&date),
						     g_date_get_month (&date),
						     g_date_get_day (&date));
		almanah_storage_manager_query_async (self, "DELETE FROM entry_links WHERE year = %u AND month = %u AND day = %u", NULL, NULL, NULL,
						     g_date_get_year (&date),
						     g_date_get_month (&date),
						     g_date_get_day (&date));

		return TRUE;
	} else {
		const guint8 *data;
		gsize length;
		sqlite3_stmt *statement;

		/* It's necessary to avoid our nice SQLite interface and use the sqlite3 API directly here
		 * as we can't otherwise reliably bind the data blob to the query --- if we pass it in as
		 * a string, it gets cut off at the first nul character, which could occur anywhere in
		 * the blob. */

		/* Prepare the statement */
		if (sqlite3_prepare_v2 (self->priv->connection,
					"REPLACE INTO entries (year, month, day, content) VALUES (?, ?, ?, ?)", -1,
					&statement, NULL) != SQLITE_OK) {
			return FALSE;
		}

		/* Bind parameters */
		sqlite3_bind_int (statement, 1, g_date_get_year (&date));
		sqlite3_bind_int (statement, 2, g_date_get_month (&date));
		sqlite3_bind_int (statement, 3, g_date_get_day (&date));

		data = almanah_entry_get_data (entry, &length);
		sqlite3_bind_blob (statement, 4, data, length, SQLITE_TRANSIENT);

		/* Execute the statement */
		if (sqlite3_step (statement) != SQLITE_DONE) {
			sqlite3_finalize (statement);
			return FALSE;
		}

		sqlite3_finalize (statement);

		return TRUE;
	}
}

/**
 * almanah_storage_manager_search_entries:
 * @self: a #AlmanahStorageManager
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
 * Return value: number of results, or -1 on failure
 **/
gint
almanah_storage_manager_search_entries (AlmanahStorageManager *self, const gchar *search_string, GDate *matches[])
{
	sqlite3_stmt *statement;
	GtkTextBuffer *text_buffer;
	guint result_count = 1; /* initialise to 1 to account for the working array element */

	/* Prepare the statement */
	if (sqlite3_prepare_v2 (self->priv->connection,
				"SELECT content, day, month, year FROM entries", -1,
				&statement, NULL) != SQLITE_OK) {
		return -1;
	}

	text_buffer = gtk_text_buffer_new (NULL);
	*matches = g_malloc (sizeof (GDate));

	/* Execute the statement */
	while (sqlite3_step (statement) == SQLITE_ROW) {
		AlmanahEntry *entry;
		GDate *date = &((*matches)[result_count - 1]);
		GtkTextIter iter;

		g_date_set_dmy (date, sqlite3_column_int (statement, 1), sqlite3_column_int (statement, 2), sqlite3_column_int (statement, 3));
		entry = almanah_entry_new (date);
		almanah_entry_set_data (entry, sqlite3_column_blob (statement, 0), sqlite3_column_bytes (statement, 0));

		/* Deserialise the entry into our buffer */
		gtk_text_buffer_set_text (text_buffer, "", 0);
		if (almanah_entry_get_content (entry, text_buffer, NULL) == FALSE) {
			g_object_unref (entry);
			continue;
		}

		/* Perform the search */
		gtk_text_buffer_get_start_iter (text_buffer, &iter);
		if (gtk_text_iter_forward_search (&iter, search_string, GTK_TEXT_SEARCH_VISIBLE_ONLY | GTK_TEXT_SEARCH_TEXT_ONLY, NULL, NULL, NULL) == TRUE) {
			/* A match was found, so move to the next working array element
			 * (effectively add the date to the results list, by preventing it being overwritten). */
			*matches = g_realloc (*matches, ++result_count * sizeof (GDate));
		}

		/* Free stuff up and continue */
		g_object_unref (entry);
	}

	sqlite3_finalize (statement);
	g_object_unref (text_buffer);

	return result_count - 1;
}

/* NOTE: Free results with g_free */
gboolean *
almanah_storage_manager_get_month_marked_days (AlmanahStorageManager *self, GDateYear year, GDateMonth month)
{
	AlmanahQueryResults *results;
	guint i;
	gboolean *days = g_malloc0 (sizeof (gboolean) * 32);

	results = almanah_storage_manager_query (self, "SELECT day FROM entries WHERE year = %u AND month = %u", NULL,
						 year,
						 month);

	if (results == NULL)
		return days;

	for (i = 1; i <= results->rows; i++)
		days[atoi (results->data[i])] = TRUE;

	almanah_storage_manager_free_results (results);

	return days;
}

/* NOTE: Free array with g_free and each element with g_object_unref */
AlmanahLink **
almanah_storage_manager_get_entry_links (AlmanahStorageManager *self, GDate *date)
{
	AlmanahQueryResults *results;
	AlmanahLink **links;
	guint i;

	results = almanah_storage_manager_query (self, "SELECT link_type, link_value, link_value2 FROM entry_links WHERE year = %u AND month = %u AND day = %u", NULL,
						 g_date_get_year (date),
						 g_date_get_month (date),
						 g_date_get_day (date));

	if (results == NULL || results->rows == 0) {
		if (results != NULL)
			almanah_storage_manager_free_results (results);

		/* Return empty array */
		links = (AlmanahLink**) g_new (AlmanahLink*, 1);
		links[0] = NULL;
		return links;
	}

	links = (AlmanahLink**) g_new (AlmanahLink*, results->rows + 1);
	for (i = 0; i < results->rows; i++) {
		links[i] = almanah_link_new (results->data[(i + 1) * results->columns]);
		almanah_link_set_value (links[i], results->data[(i + 1) * results->columns + 1]);
		almanah_link_set_value2 (links[i], results->data[(i + 1) * results->columns + 2]);
	}
	links[i] = NULL;

	almanah_storage_manager_free_results (results);

	return links;
}

gboolean
almanah_storage_manager_add_entry_link (AlmanahStorageManager *self, GDate *date, AlmanahLink *link)
{
	gboolean return_value;
	gchar *value, *value2;
	const gchar *type_id;

	type_id = almanah_link_get_type_id (link);
	value = almanah_link_get_value (link);
	value2 = almanah_link_get_value2 (link);

	if (value2 == NULL) {
		return_value = almanah_storage_manager_query_async (self, "REPLACE INTO entry_links (year, month, day, link_type, link_value) VALUES (%u, %u, %u, '%q', '%q')", NULL, NULL, NULL,
								    g_date_get_year (date),
								    g_date_get_month (date),
								    g_date_get_day (date),
								    type_id,
								    value);
	} else {
		return_value = almanah_storage_manager_query_async (self, "REPLACE INTO entry_links (year, month, day, link_type, link_value, link_value2) VALUES (%u, %u, %u, '%q', '%q', '%q')", NULL, NULL, NULL,
								    g_date_get_year (date),
								    g_date_get_month (date),
								    g_date_get_day (date),
								    type_id,
								    value,
								    value2);
	}

	g_free (value);
	g_free (value2);

	return return_value;
}

gboolean
almanah_storage_manager_remove_entry_link (AlmanahStorageManager *self, GDate *date, const gchar *link_type_id)
{
	return almanah_storage_manager_query_async (self, "DELETE FROM entry_links WHERE year = %u AND month = %u AND day = %u AND link_type = '%q'", NULL, NULL, NULL,
						    g_date_get_year (date),
						    g_date_get_month (date),
						    g_date_get_day (date),
						    link_type_id);
}
