/*
 * Almanah
 * Copyright (C) 2004-2006, 2008 Stefan Walter
 *               2008-2009 Philip Withnall <philip@tecnocode.co.uk>
 *               2025 Jan Tojnar <jtojnar@gmail.com>
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
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gpgme.h>
#include <gtk/gtk.h>

#include "secret-keys-store.h"

#define ENCRYPTION_NONE_ID ""
#define SEAHORSE_EXT_GPG ".gpg"

struct _AlmanahSecretKeysStore {
	GtkListStore parent_instance;
	gpgme_ctx_t gpgme_context;
	GFileMonitor *gpg_homedir_monitor;
	guint scheduled_keys_update;
};

G_DEFINE_TYPE (AlmanahSecretKeysStore, almanah_secret_keys_store, GTK_TYPE_LIST_STORE)

static void
almanah_secret_keys_store_dispose (GObject *gobject)
{
	AlmanahSecretKeysStore *self = ALMANAH_SECRET_KEYS_STORE (gobject);

	g_clear_pointer (&self->gpgme_context, gpgme_release);
	g_clear_object (&self->gpg_homedir_monitor);

	G_OBJECT_CLASS (almanah_secret_keys_store_parent_class)->dispose (gobject);
}

static void
almanah_secret_keys_store_class_init (AlmanahSecretKeysStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = almanah_secret_keys_store_dispose;
}

static void
almanah_secret_keys_store_init (AlmanahSecretKeysStore *self)
{
	GType column_types[SECRET_KEYS_STORE_N_COLUMNS];
	column_types[SECRET_KEYS_STORE_COLUMN_ID] = G_TYPE_STRING;
	column_types[SECRET_KEYS_STORE_COLUMN_LABEL] = G_TYPE_STRING;

	gtk_list_store_set_column_types (GTK_LIST_STORE (self), SECRET_KEYS_STORE_N_COLUMNS, column_types);
}

/* Stolen from libcryptui’s seahorse_pgp_uid_calc_label */
static gchar *
pgp_uid_calc_label (const gchar *name, const gchar *email, const gchar *comment)
{
	GString *string;

	g_return_val_if_fail (name, NULL);

	string = g_string_new ("");
	g_string_append (string, name);

	if (email && email[0]) {
		g_string_append (string, " <");
		g_string_append (string, email);
		g_string_append (string, ">");
	}

	if (comment && comment[0]) {
		g_string_append (string, " (");
		g_string_append (string, comment);
		g_string_append (string, ")");
	}

	return g_string_free_and_steal (string);
}

static void
append_key (GtkListStore *store, const char *id, const char *label)
{
	GtkTreeIter iter;

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
	                    SECRET_KEYS_STORE_COLUMN_ID, id,
	                    SECRET_KEYS_STORE_COLUMN_LABEL, label,
	                    -1);
}

static void
update_key_list (AlmanahSecretKeysStore *store)
{
	GtkListStore *gtk_store = GTK_LIST_STORE (store);
	/* Transient table to avoid the need to modify unchanged keys in the model */
	GHashTable *new_keys = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                              g_free, g_free);
	gpgme_key_t key;

	/* Get private keys */
	gpgme_op_keylist_start (store->gpgme_context, NULL, 1);

	while (!gpgme_op_keylist_next (store->gpgme_context, &key)) {
		if (key->uids && key->subkeys) {
			// Based on libcryptui’s seahorse_pgp_key_calc_id
			g_autofree gchar *id = g_strdup_printf ("openpgp:%s", key->subkeys->keyid);
			g_autofree gchar *label = pgp_uid_calc_label (key->uids->name,
			                                              key->uids->email,
			                                              key->uids->comment);

			if (label) {
				g_hash_table_insert (new_keys, g_steal_pointer (&id),
				                     g_steal_pointer (&label));
			}
		}

		gpgme_key_release (key);
	}

	GtkTreeIter iter;
	gboolean valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);
	while (valid) {
		g_autofree gchar *old_id = NULL;
		g_autofree gchar *old_label = NULL;
		g_autofree gchar *new_id = NULL;
		g_autofree gchar *new_label = NULL;
		gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
		                    SECRET_KEYS_STORE_COLUMN_ID, &old_id,
		                    SECRET_KEYS_STORE_COLUMN_LABEL, &old_label,
		                    -1);

		if (g_strcmp0 (old_id, ENCRYPTION_NONE_ID) != 0) {
			if (!g_hash_table_steal_extended (new_keys, old_id,
			                                  (gpointer) &new_id,
			                                  (gpointer) &new_label)) {
				/* Remove key deleted in GPG homedir */
				valid = gtk_list_store_remove (gtk_store, &iter);
				continue;
			} else {
				if (g_strcmp0 (new_label, old_label) != 0) {
					/* Update label of key because it changed in GPG homedir */
					gtk_list_store_set (gtk_store, &iter,
					                    SECRET_KEYS_STORE_COLUMN_LABEL, new_label,
					                    -1);
				}
			}
		}

		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter);
	}

	GHashTableIter hiter;
	gchar *new_id;
	gchar *new_label;
	g_hash_table_iter_init (&hiter, new_keys);
	while (g_hash_table_iter_next (&hiter, (gpointer) &new_id, (gpointer) &new_label)) {
		/* Add key that newly appeared in GPG homedir */
		append_key (gtk_store, new_id, new_label);
	}

	g_hash_table_destroy (new_keys);

	gpgme_op_keylist_end (store->gpgme_context);
}

/* Stolen from libcryptui’s cancel_scheduled_refresh */
static void
cancel_keys_update (AlmanahSecretKeysStore *store)
{
	if (store->scheduled_keys_update == 0) {
		return;
	}

	g_debug ("cancelling scheduled keys update event");
	g_source_remove (store->scheduled_keys_update);
	store->scheduled_keys_update = 0;
}

/* Stolen from libcryptui’s scheduled_refresh */
static gboolean
scheduled_keys_update (gpointer data)
{
	AlmanahSecretKeysStore *store = ALMANAH_SECRET_KEYS_STORE (data);

	g_debug ("scheduled keys update event ocurring now");
	cancel_keys_update (store);
	update_key_list (store);

	/* don't run again */
	return FALSE;
}

/* Stolen from libcryptui */
static void
monitor_gpg_homedir (GFileMonitor *handle, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data)
{
	AlmanahSecretKeysStore *store = ALMANAH_SECRET_KEYS_STORE (user_data);

	if (event_type == G_FILE_MONITOR_EVENT_CHANGED ||
	    event_type == G_FILE_MONITOR_EVENT_DELETED ||
	    event_type == G_FILE_MONITOR_EVENT_CREATED) {
		g_autofree gchar *name = g_file_get_basename (file);

		if (g_str_has_suffix (name, SEAHORSE_EXT_GPG)) {
			if (store->scheduled_keys_update == 0) {
				g_debug ("scheduling keys update event due to file changes");
				store->scheduled_keys_update = g_timeout_add (500, scheduled_keys_update, store);
			}
		}
	}
}

static void
init_gpgme (AlmanahSecretKeysStore *store)
{
	gpgme_error_t gpgme_error;

	/* This is required to initialize GPGME */
	g_assert (gpgme_check_version (MIN_GPGME_VERSION));
	gpgme_error = gpgme_engine_check_version (GPGME_PROTOCOL_OpenPGP);
	g_assert_cmpint (gpgme_error, ==, GPG_ERR_NO_ERROR);
	gpgme_error = gpgme_new (&store->gpgme_context);
	g_assert_cmpint (gpgme_error, ==, GPG_ERR_NO_ERROR);
	gpgme_set_protocol (store->gpgme_context, GPGME_PROTOCOL_OpenPGP);
}

/* Stolen from libcryptui’s seahorse_gpgme_source_init */
static void
init_gpg_homedir_monitor (AlmanahSecretKeysStore *store)
{
	const gchar *gpg_homedir = gpgme_get_dirinfo ("homedir");
	g_autoptr (GError) err = NULL;
	g_autoptr (GFile) file = g_file_new_for_path (gpg_homedir);
	g_return_if_fail (file != NULL);

	store->gpg_homedir_monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, &err);

	if (store->gpg_homedir_monitor) {
		g_signal_connect (store->gpg_homedir_monitor, "changed",
		                  G_CALLBACK (monitor_gpg_homedir), store);
	} else {
		g_warning ("couldn't monitor the GPG home directory: %s: %s",
		           gpg_homedir, err->message ?: "");
	}
}

AlmanahSecretKeysStore *
almanah_secret_keys_store_new (void)
{
	GObject *store = g_object_new (ALMANAH_TYPE_SECRET_KEYS_STORE, NULL);

	AlmanahSecretKeysStore *self = ALMANAH_SECRET_KEYS_STORE (store);

	init_gpgme (self);
	init_gpg_homedir_monitor (self);
	append_key (GTK_LIST_STORE (self), ENCRYPTION_NONE_ID, _ ("None (don't encrypt)"));
	update_key_list (self);

	return self;
}
