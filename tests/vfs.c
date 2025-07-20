/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Alvaro Peña 2015 <alvaropg@gmail.com>
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

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gpgme.h>
#include <sqlite3.h>

#include "../src/vfs.h"

#define ALMANAH_TEST_VFS_DATABASE           "almanah_tests.db"
#define ALMANAH_TEST_VFS_DATABASE_ENCRYPTED "almanah_tests.db.encrypted"

struct AlmanahTestVfsFixture {
	sqlite3 *db;
	sqlite3_stmt *statement;
	GSettings *settings;
	gpgme_ctx_t gpgme_context;
	gchar *key_fpr;
	gchar *tmp_dir;
	gchar *db_file;
};

static GSettings*
almanah_test_setup_memory_gsettings (void)
{
	GSettingsBackend *settings_backend;
	GSettings *settings;

	settings_backend = g_settings_backend_get_default ();
	g_object_unref (settings_backend);
	settings_backend = g_memory_settings_backend_new ();
	settings = g_settings_new_with_backend ("org.gnome.almanah", settings_backend);
	g_object_unref (settings_backend);

	return settings;
}

static void
almanah_test_vfs_plain_setup (struct AlmanahTestVfsFixture *fixture, __attribute__ ((unused)) gconstpointer user_data)
{
	GError *error = NULL;

	fixture->tmp_dir = g_dir_make_tmp ("almanah_XXXXXX", &error);
	g_assert_no_error (error);
	fixture->db_file = g_build_filename (fixture->tmp_dir, ALMANAH_TEST_VFS_DATABASE, NULL);

	fixture->settings = almanah_test_setup_memory_gsettings ();
	g_settings_set_string (fixture->settings, "encryption-key" , "");

	/* Initializing the VFS */
	almanah_vfs_init (fixture->settings);

	fixture->db = NULL;
	fixture->statement = NULL;
}

static void
almanah_test_vfs_enc_setup (struct AlmanahTestVfsFixture *fixture, __attribute__ ((unused)) gconstpointer user_data)
{
	GError *error = NULL;
	gpgme_error_t gpgme_error;
	gchar *encryption_key;
	const char parms[] =
                "<GnupgKeyParms format=\"internal\">\n"
                "Key-Type: RSA\n"
                "Key-Length: 2048\n"
                "Key-Usage: sign\n"
                "Subkey-Type: RSA\n"
                "Subkey-Length: 2048\n"
                "Subkey-Usage: encrypt\n"
                "Name-Real: Almanah Test\n"
                "Name-Email: almanah@gnome.org\n"
                "Expire-Date: 0\n"
                "%no-protection\n"
                "%no-ask-passphrase\n"
                "</GnupgKeyParms>\n";
        gpgme_genkey_result_t gpgme_key_result = NULL;

	fixture->tmp_dir = g_dir_make_tmp ("almanah_XXXXXX", &error);
	g_assert_no_error (error);
	fixture->db_file = g_build_filename (fixture->tmp_dir, ALMANAH_TEST_VFS_DATABASE, NULL);

	fixture->settings = almanah_test_setup_memory_gsettings ();

	/* This is required to initialize GPGME */
	g_assert (gpgme_check_version (MIN_GPGME_VERSION));
	gpgme_error = gpgme_engine_check_version (GPGME_PROTOCOL_OpenPGP);
	g_assert_cmpint (gpgme_error, ==, GPG_ERR_NO_ERROR);
	gpgme_error = gpgme_new (&fixture->gpgme_context);
	g_assert_cmpint (gpgme_error, ==, GPG_ERR_NO_ERROR);
	gpgme_set_protocol (fixture->gpgme_context, GPGME_PROTOCOL_OpenPGP);
	gpgme_set_armor (fixture->gpgme_context, TRUE);
	gpgme_set_textmode (fixture->gpgme_context, FALSE);

	/* Creating a testing encryption key */
	gpgme_error = gpgme_op_genkey (fixture->gpgme_context, parms, NULL, NULL);
	g_assert_cmpint (gpgme_error, ==, GPG_ERR_NO_ERROR);
        gpgme_key_result = gpgme_op_genkey_result (fixture->gpgme_context);
	g_assert(gpgme_key_result);

	fixture->key_fpr = g_strdup(gpgme_key_result->fpr);
	encryption_key = g_strdup_printf ("openpgp:%s", gpgme_key_result->fpr);
	g_settings_set_string (fixture->settings, "encryption-key", encryption_key);
	g_free (encryption_key);

	gpgme_signers_clear (fixture->gpgme_context);

	/* It's required to initialize the VFS before open a database with it */
	almanah_vfs_init (fixture->settings);

	fixture->db = NULL;
	fixture->statement = NULL;
}

static void
almanah_test_vfs_enc_teardown (struct AlmanahTestVfsFixture *fixture, __attribute__ ((unused)) gconstpointer user_data)
{

	if (fixture->statement != NULL)
		sqlite3_finalize (fixture->statement);

	if (fixture->settings != NULL)
		g_object_unref (fixture->settings);

	if (fixture->db != NULL)
		sqlite3_close (fixture->db);

	/* Removing the encryption key */
	if (fixture->gpgme_context) {
		if (fixture->key_fpr) {
			gpgme_key_t key;
			gpgme_error_t gpgme_error;

			gpgme_error = gpgme_get_key (fixture->gpgme_context, fixture->key_fpr, &key, 1);
			if (gpgme_error == GPG_ERR_NO_ERROR)
				gpgme_op_delete (fixture->gpgme_context, key, 1);
			gpgme_key_release (key);
			g_free(fixture->key_fpr);
		}
		gpgme_release (fixture->gpgme_context);
	}

	almanah_vfs_finish ();

	if (fixture->db_file) {
		gchar *enc_file;

		/* Encrypted DB */
		enc_file = g_build_filename (fixture->tmp_dir, ALMANAH_TEST_VFS_DATABASE_ENCRYPTED, NULL);
		g_unlink (enc_file);
		g_free (enc_file);

		/* Backup encrypted DB */
		enc_file = g_build_filename (fixture->tmp_dir, ALMANAH_TEST_VFS_DATABASE_ENCRYPTED"~", NULL);
		g_unlink (enc_file);
		g_free (enc_file);

		g_unlink (fixture->db_file);
		g_free (fixture->db_file);
	}
	if (fixture->tmp_dir) {
		g_rmdir (fixture->tmp_dir);
		g_free (fixture->tmp_dir);
	}
}

static void
almanah_test_vfs_plain_teardown (struct AlmanahTestVfsFixture *fixture, __attribute__ ((unused)) gconstpointer user_data)
{
	if (fixture->statement != NULL)
		sqlite3_finalize (fixture->statement);

	if (fixture->settings != NULL)
		g_object_unref (fixture->settings);

	if (fixture->db != NULL)
		sqlite3_close (fixture->db);

	almanah_vfs_finish ();

	if (fixture->db_file) {
		g_unlink (fixture->db_file);
		g_free (fixture->db_file);
	}

	if (fixture->tmp_dir) {
		g_rmdir (fixture->tmp_dir);
		g_free (fixture->tmp_dir);
	}
}

static void
almanah_test_vfs_encrypted (struct AlmanahTestVfsFixture *fixture, __attribute__ ((unused)) gconstpointer user_data)
{
	gint rc;
	gchar *error_msg = 0;
	const guchar *entry_content = NULL;
	gchar *enc_file;

	/* Create the database */
	rc = sqlite3_open_v2 (fixture->db_file, &fixture->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "almanah");
	if (rc != SQLITE_OK)
		g_test_message ("Error opening database: %s", sqlite3_errmsg (fixture->db));
	g_assert_cmpint (rc, ==, SQLITE_OK);

	/* Add a table and some data */
	rc = sqlite3_exec(fixture->db, "CREATE TABLE IF NOT EXISTS entries (year INTEGER, month INTEGER, day INTEGER, content TEXT, PRIMARY KEY (year, month, day));", NULL, 0, &error_msg);
	if (rc != SQLITE_OK) {
		g_test_message ("SQL error creating table entries: %s\n", error_msg);
		sqlite3_free (error_msg);
	}
	g_assert_cmpint (rc, ==, SQLITE_OK);

	rc = sqlite3_exec(fixture->db, "INSERT INTO entries (year, month, day, content) VALUES (2015, 4, 19, 'Just a test');", NULL, 0, &error_msg);
	if (rc != SQLITE_OK) {
		g_test_message ("SQL error inserting an entry: %s\n", error_msg);
		sqlite3_free (error_msg);
	}
	g_assert_cmpint (rc, ==, SQLITE_OK);

	/* Close the DB */
	sqlite3_close(fixture->db);
	fixture->db = NULL;

	/* Ensure the encrypted file */
	enc_file = g_build_filename (fixture->tmp_dir, ALMANAH_TEST_VFS_DATABASE_ENCRYPTED, NULL);
	g_assert (g_file_test (enc_file, G_FILE_TEST_IS_REGULAR));
	g_free (enc_file);

	/* Reopen the database */
	rc = sqlite3_open_v2 (fixture->db_file, &fixture->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "almanah");
	if (rc != SQLITE_OK)
		g_test_message ("Error opening database: %s", sqlite3_errmsg (fixture->db));
	g_assert_cmpint (rc, ==, SQLITE_OK);

	/* Ensure the data */
	fixture->statement = NULL;
	rc = sqlite3_prepare_v2 (fixture->db, "SELECT content FROM entries WHERE year=2015 AND month=4 AND day=19", -1, &fixture->statement, NULL);
	if (rc != SQLITE_OK)
		g_test_message ("Error quering for content: %s", sqlite3_errmsg (fixture->db));
	g_assert_cmpint (rc, ==, SQLITE_OK);

	rc = sqlite3_step (fixture->statement);
	if (rc != SQLITE_ROW)
		g_test_message ("Error steping for content: %s", sqlite3_errmsg (fixture->db));

	entry_content = sqlite3_column_text (fixture->statement, 0);
	g_assert_cmpstr ((gchar *) entry_content, ==, "Just a test");

	sqlite3_finalize (fixture->statement);
	fixture->statement = NULL;
	sqlite3_close(fixture->db);
	fixture->db = NULL;
}

static void
almanah_test_vfs_plain_open (struct AlmanahTestVfsFixture *fixture, __attribute__ ((unused)) gconstpointer user_data)
{
	gint rc;

	rc = sqlite3_open_v2 (fixture->db_file, &fixture->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "almanah");
	if (rc != SQLITE_OK)
		g_test_message ("Error opening database: %s", sqlite3_errmsg (fixture->db));
	g_assert_cmpint (rc, ==, SQLITE_OK);

	sqlite3_close(fixture->db);
	fixture->db = NULL;
}

static void
almanah_test_vfs_plain_data (struct AlmanahTestVfsFixture *fixture, __attribute__ ((unused)) gconstpointer user_data)
{
	gint rc;
	gchar *error_msg = 0;
	const guchar *entry_content = NULL;

	rc = sqlite3_open_v2 (fixture->db_file, &fixture->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "almanah");
	if (rc != SQLITE_OK)
		g_test_message ("Error opening database: %s", sqlite3_errmsg (fixture->db));
	g_assert_cmpint (rc, ==, SQLITE_OK);

	rc = sqlite3_exec(fixture->db, "CREATE TABLE IF NOT EXISTS entries (year INTEGER, month INTEGER, day INTEGER, content TEXT, PRIMARY KEY (year, month, day));", NULL, 0, &error_msg);
	if (rc != SQLITE_OK) {
		g_test_message ("SQL error creating table entries: %s\n", error_msg);
		sqlite3_free (error_msg);
	}
	g_assert_cmpint (rc, ==, SQLITE_OK);

	rc = sqlite3_exec(fixture->db, "INSERT INTO entries (year, month, day, content) VALUES (2015, 3, 7, 'Just a test');", NULL, 0, &error_msg);
	if (rc != SQLITE_OK) {
		g_test_message ("SQL error inserting an entry: %s\n", error_msg);
		sqlite3_free (error_msg);
	}
	g_assert_cmpint (rc, ==, SQLITE_OK);

	sqlite3_close(fixture->db);
	fixture->db = NULL;

	rc = sqlite3_open_v2 (fixture->db_file, &fixture->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "almanah");
	if (rc != SQLITE_OK)
		g_test_message ("Error opening database: %s", sqlite3_errmsg (fixture->db));
	g_assert_cmpint (rc, ==, SQLITE_OK);

	fixture->statement = NULL;
	rc = sqlite3_prepare_v2 (fixture->db, "SELECT content FROM entries WHERE year=2015 AND month=3 AND day=7", -1, &fixture->statement, NULL);
	if (rc != SQLITE_OK)
		g_test_message ("Error quering for content: %s", sqlite3_errmsg (fixture->db));
	g_assert_cmpint (rc, ==, SQLITE_OK);

	rc = sqlite3_step (fixture->statement);
	if (rc != SQLITE_ROW)
		g_test_message ("Error steping for content: %s", sqlite3_errmsg (fixture->db));

	entry_content = sqlite3_column_text (fixture->statement, 0);
	g_assert_cmpstr ((gchar *) entry_content, ==, "Just a test");

	sqlite3_finalize (fixture->statement);
	fixture->statement = NULL;
	sqlite3_close(fixture->db);
	fixture->db = NULL;
}

int
main(int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	g_test_add ("/Almanah/vfs/plain",
		    struct AlmanahTestVfsFixture,
		    NULL,
		    almanah_test_vfs_plain_setup,
		    almanah_test_vfs_plain_open,
		    almanah_test_vfs_plain_teardown);
	g_test_add ("/Almanah/vfs/data",
		    struct AlmanahTestVfsFixture,
		    NULL,
		    almanah_test_vfs_plain_setup,
		    almanah_test_vfs_plain_data,
		    almanah_test_vfs_plain_teardown);
	g_test_add ("/Almanah/vfs/encrypted",
		    struct AlmanahTestVfsFixture,
		    NULL,
		    almanah_test_vfs_enc_setup,
		    almanah_test_vfs_encrypted,
		    almanah_test_vfs_enc_teardown);

	return g_test_run();
}
