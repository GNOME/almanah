/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Alvaro Peña 2014 <alvaropg@gmail.com>
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

/*
 * This code is based in the code available in the SQlite web
 * http://www.sqlite.org/vfs.html called test_demovfs.c.
 * So, many thanks to the SQLite guys!
 */

/*
 * Some important things about this VFS:
 * - Not locking allowed, so no more than one process using the same
 *   database.
 * - See more documentation in the demovfs link.
 */

#include <sqlite3.h>

#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/param.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#define GCR_API_SUBJECT_TO_CHANGE
#include <gcr/gcr.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gpgme.h>

#include <config.h>

#include "vfs.h"

/* VFS singleton and not a sqlite3_vfs static struct
   due GSettings is setup in initialization time */
static sqlite3_vfs *almanah_vfs_singleton = NULL;

#define ENCRYPTED_SUFFIX ".encrypted"

/*
** Size of the write buffer used by journal files in bytes.
*/
#ifndef SQLITE_DEMOVFS_BUFFERSZ
# define SQLITE_DEMOVFS_BUFFERSZ 8192
#endif

/*
** The maximum pathname length supported by this VFS.
*/
#define MAXPATHNAME 512

/*
** When using this VFS, the sqlite3_file* handles that SQLite uses are
** actually pointers to instances of type DemoFile.
*/

typedef struct _AlmanahSQLiteVFS AlmanahSQLiteVFS;

struct _AlmanahSQLiteVFS
{
	sqlite3_file base;              /* Base class. Must be first. */
	int fd;                         /* File descriptor */

	char *aBuffer;                  /* Pointer to malloc'd buffer */
	int nBuffer;                    /* Valid bytes of data in zBuffer */
	sqlite3_int64 iBufferOfst;      /* Offset in file of zBuffer[0] */

	gchar *plain_filename;
	gchar *encrypted_filename;

	gboolean decrypted;

	guint8  *plain_buffer;
	gsize    plain_buffer_size;     /* Reserved memory size */
	goffset  plain_offset;
	gsize    plain_size;            /* Data size (plain_size <= plain_buffer_size) */

	GSettings *settings;
};

typedef struct _CipherOperation CipherOperation;
typedef struct _GpgmeNpmClosure GpgmeNpmClosure;

struct _GpgmeNpmClosure {
	guint8  *buffer;
	gsize    buffer_size;
	goffset  offset;
	gsize    size;
};

struct _CipherOperation {
	GIOChannel *cipher_io_channel;
	GIOChannel *plain_io_channel;
	gpgme_data_t gpgme_cipher;
	gpgme_data_t gpgme_plain;
	gpgme_ctx_t context;
	AlmanahSQLiteVFS *vfs;
	struct gpgme_data_cbs *gpgme_cbs;
	GpgmeNpmClosure *npm_closure;
};

enum {
	ALMANAH_VFS_ERROR_DECRYPT = 1
};

#define ALMANAH_VFS_ERROR almanah_vfs_error_quark ()

static GQuark
almanah_vfs_error_quark (void)
{
  return g_quark_from_static_string ("almanah-vfs-error-quark");
}

/* Some wrappers around the libgcr secure memory functionality which fall back
 * to normal malloc() if secure memory fails. It will typically fail because
 * rlimit_secmem is low (64KiB by default) which prevents unprivileged
 * processes from allocating large amounts of secure memory. This typically
 * means that we can’t keep the entire decrypted database in secure memory.
 *
 * Because keeping it in normal memory is better than leaving it on disk (even
 * with the risk of normal memory being paged out at some point), do that
 * instead of failing completely.
 */
static gpointer
maybe_secure_memory_try_realloc (gpointer memory,
                                 gsize    size)
{
	gpointer buffer = NULL;

	if (gcr_secure_memory_is_secure (memory))
		buffer = gcr_secure_memory_try_realloc (memory, size);
	if (buffer == NULL)
		buffer = g_try_realloc (memory, size);

	return buffer;
}

static gpointer
maybe_secure_memory_realloc (gpointer memory,
                             gsize    size)
{
	gpointer buffer = NULL;

	if (gcr_secure_memory_is_secure (memory))
		buffer = gcr_secure_memory_try_realloc (memory, size);
	if (buffer == NULL)
		buffer = g_realloc (memory, size);

	g_assert (buffer != NULL);
	return buffer;
}

static void
maybe_secure_memory_free (gpointer memory)
{
	if (gcr_secure_memory_is_secure (memory))
		gcr_secure_memory_free (memory);
	else
		g_free (memory);
}

/* Callback based data buffer functions for GPGME */
ssize_t _gpgme_read_cb    (void *handle, void *buffer, size_t size);
ssize_t _gpgme_write_cb   (void *handle, const void *buffer, size_t size);
off_t   _gpgme_seek_cb    (void *handle, off_t offset, int whence);

ssize_t
_gpgme_read_cb (void *handle, void *buffer, size_t size)
{
	GpgmeNpmClosure *npm_closure = (GpgmeNpmClosure *) handle;
	gsize read_size;

	read_size = npm_closure->size - npm_closure->offset;
	if (!read_size)
		return 0;
	if (size < read_size)
		read_size = size;

	memcpy (buffer, npm_closure->buffer + npm_closure->offset, read_size);
	npm_closure->offset += read_size;

	return (ssize_t) read_size;
}

ssize_t
_gpgme_write_cb (void *handle, const void *buffer, size_t size)
{
	GpgmeNpmClosure *npm_closure = (GpgmeNpmClosure *) handle;
	gsize unused;

	unused = npm_closure->buffer_size - npm_closure->offset;
	if (unused < size) {
		gsize new_size;
		gsize exponential_size;
		gsize required_size;
		gpointer new_buffer;

		exponential_size = npm_closure->size ? (2 * npm_closure->size) : 512;
		required_size = npm_closure->offset + size;

		new_size = MAX (exponential_size, required_size);
		new_buffer = maybe_secure_memory_try_realloc (npm_closure->buffer, new_size);
		if (!new_buffer && (new_size > required_size)) {
			new_size = required_size;
			new_buffer = maybe_secure_memory_realloc (npm_closure->buffer, new_size);
		}

		if (!new_buffer) {
			errno = ENOMEM;
			return -1;
		}

		npm_closure->buffer = new_buffer;
		npm_closure->buffer_size = new_size;
	}

	memcpy (npm_closure->buffer + npm_closure->offset, buffer, size);
	npm_closure->offset += (gsize) size;
	npm_closure->size = MAX (npm_closure->size, (gsize) npm_closure->offset);

	return size;
}

off_t
_gpgme_seek_cb (void *handle, off_t offset, int whence)
{
	GpgmeNpmClosure *npm_closure = (GpgmeNpmClosure *) handle;

	switch (whence) {
	case SEEK_SET:
		if (offset < 0 || (gsize) offset > npm_closure->size) {
			errno = EINVAL;
			return -1;
		}
		npm_closure->offset = (goffset) offset;
		break;
	case SEEK_CUR:
		if ((offset > 0 && (npm_closure->size - npm_closure->offset) < (gsize) offset)
		    || (offset < 0 && npm_closure->offset < -offset)) {
			errno = EINVAL;
			return -1;
		}
		npm_closure->offset += offset;
		break;
	case SEEK_END:
		if (offset > 0 || (gsize) -offset > npm_closure->size) {
			errno = EINVAL;
			return -1;
		}
		npm_closure->offset = npm_closure->size + offset;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	return (off_t) npm_closure->offset;
}

static gboolean
prepare_gpgme (CipherOperation *operation)
{
	gpgme_error_t error_gpgme;

	/* Check for a minimum GPGME version (bgo#599598) */
	if (gpgme_check_version (MIN_GPGME_VERSION) == NULL) {
		g_critical (_("GPGME is not at least version %s"), MIN_GPGME_VERSION);
		return FALSE;
	}

	/* Check OpenPGP's supported */
	error_gpgme = gpgme_engine_check_version (GPGME_PROTOCOL_OpenPGP);
	if (error_gpgme != GPG_ERR_NO_ERROR) {
		g_critical (_("GPGME doesn't support OpenPGP: %s"), gpgme_strerror (error_gpgme));
		return FALSE;
	}

	/* Set up for the operation */
	error_gpgme = gpgme_new (&(operation->context));
	if (error_gpgme != GPG_ERR_NO_ERROR) {
		g_critical (_("Error creating cipher context: %s"), gpgme_strerror (error_gpgme));
		return FALSE;
	}

	gpgme_set_protocol (operation->context, GPGME_PROTOCOL_OpenPGP);
	gpgme_set_armor (operation->context, TRUE);
	gpgme_set_textmode (operation->context, FALSE);

	return TRUE;
}

static gboolean
open_db_files (AlmanahSQLiteVFS *self, gboolean encrypting, CipherOperation *operation, gboolean use_memory, GError **error)
{
	GError *io_error = NULL;
	gpgme_error_t error_gpgme;

	/* Open the encrypted file */
	operation->cipher_io_channel = g_io_channel_new_file (self->encrypted_filename, encrypting ? "w" : "r", &io_error);
	if (operation->cipher_io_channel == NULL) {
		g_critical (_("Can't create a new GIOChannel for the encrypted database: %s"), io_error->message);
		g_propagate_error (error, io_error);
		return FALSE;
	}

	/* Pass it to GPGME */
	error_gpgme = gpgme_data_new_from_fd (&(operation->gpgme_cipher), g_io_channel_unix_get_fd (operation->cipher_io_channel));
	if (error_gpgme != GPG_ERR_NO_ERROR) {
		g_critical (_("Error opening encrypted database file \"%s\": %s"), self->encrypted_filename, gpgme_strerror (error_gpgme));
		return FALSE;
	}

	if (use_memory) {
		/* Pass the non-pageable memory to GPGME as a Callback Base Data Buffer,
		 * see: http://www.gnupg.org/documentation/manuals/gpgme/Callback-Based-Data-Buffers.html
		 */
		operation->npm_closure = g_new0 (GpgmeNpmClosure, 1);
		operation->gpgme_cbs = g_new0 (struct gpgme_data_cbs, 1);
		operation->gpgme_cbs->read =_gpgme_read_cb;
		operation->gpgme_cbs->write =_gpgme_write_cb;
		operation->gpgme_cbs->seek =_gpgme_seek_cb;
		error_gpgme = gpgme_data_new_from_cbs (&(operation->gpgme_plain), operation->gpgme_cbs, operation->npm_closure);
		if (error_gpgme != GPG_ERR_NO_ERROR) {
			g_set_error (error, 0, 0,
				     _("Error creating Callback base data buffer: %s"),
				     gpgme_strerror (error_gpgme));
			return FALSE;
		}
	} else {
		/* Open the plain file */
		operation->plain_io_channel = g_io_channel_new_file (self->plain_filename, encrypting ? "r" : "w", &io_error);
		if (operation->plain_io_channel == NULL) {
			g_critical (_("Can't create a new GIOChannel for the plain database: %s"), io_error->message);
			g_propagate_error (error, io_error);
			return FALSE;
		}

		/* Pass it to GPGME */
		error_gpgme = gpgme_data_new_from_fd (&(operation->gpgme_plain), g_io_channel_unix_get_fd (operation->plain_io_channel));
		if (error_gpgme != GPG_ERR_NO_ERROR) {
			g_critical (_("Error opening plain database file \"%s\": %s"), self->plain_filename, gpgme_strerror (error_gpgme));
			return FALSE;
		}
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

	/* We could free the operation before the context is even created (bgo#599598) */
	if (operation->context != NULL) {
		gpgme_signers_clear (operation->context);
		gpgme_release (operation->context);
	}

	if (operation->npm_closure != NULL)
		g_free (operation->npm_closure);

	if (operation->gpgme_cbs != NULL)
		g_free (operation->gpgme_cbs);

	g_free (operation);
}

static gboolean
decrypt_database (AlmanahSQLiteVFS *self, GError **error)
{
	GError *preparation_error = NULL;
	CipherOperation *operation;
	gpgme_error_t error_gpgme;

	operation = g_new0 (CipherOperation, 1);

	/* Set up, decrypting to memory */
	if (prepare_gpgme (operation) != TRUE || open_db_files (self, FALSE, operation, TRUE, &preparation_error) != TRUE) {
		cipher_operation_free (operation);
		g_propagate_error (error, preparation_error);
		return FALSE;
	}

	/* Decrypt and verify! */
	error_gpgme = gpgme_op_decrypt_verify (operation->context, operation->gpgme_cipher, operation->gpgme_plain);
	if (error_gpgme != GPG_ERR_NO_ERROR) {
		cipher_operation_free (operation);
		g_set_error (error,
			     ALMANAH_VFS_ERROR,
			     ALMANAH_VFS_ERROR_DECRYPT,
			     "%s: %s", gpgme_strsource (error_gpgme), gpgme_strerror (error_gpgme));
		return FALSE;
	}

	/* Setup the database content in memory */
	self->plain_buffer = operation->npm_closure->buffer;
	self->plain_offset = 0;
	self->plain_size = operation->npm_closure->size;

	/* Do this one synchronously */
	cipher_operation_free (operation);

	return TRUE;
}

static gboolean
encrypt_database (AlmanahSQLiteVFS *self,  const gchar *encryption_key, gboolean from_memory, GError **error)
{
	GError *preparation_error = NULL;
	CipherOperation *operation;
	gpgme_error_t error_gpgme;
	gpgme_key_t gpgme_keys[2] = { NULL, };

	operation = g_new0 (CipherOperation, 1);
	operation->vfs = self;

	/* Set up */
	if (prepare_gpgme (operation) != TRUE) {
		cipher_operation_free (operation);
		g_propagate_error (error, preparation_error);
		return FALSE;
	}

	/* Set up signing and the recipient */
	error_gpgme = gpgme_get_key (operation->context, encryption_key, &gpgme_keys[0], FALSE);
	if (error_gpgme != GPG_ERR_NO_ERROR || gpgme_keys[0] == NULL) {
		cipher_operation_free (operation);
		g_critical (_("Error getting encryption key: %s"), gpgme_strerror (error_gpgme));
		return FALSE;
	}

	gpgme_signers_add (operation->context, gpgme_keys[0]);

	if (open_db_files (self, TRUE, operation, from_memory, &preparation_error) != TRUE) {
		cipher_operation_free (operation);
		g_propagate_error (error, preparation_error);
		return FALSE;
	}

	if (from_memory) {
		operation->npm_closure->buffer = self->plain_buffer;
		operation->npm_closure->offset = 0;
		operation->npm_closure->size = self->plain_size;
	}

	/* Encrypt and sign! */
	error_gpgme = gpgme_op_encrypt_sign_start (operation->context, gpgme_keys, 0, operation->gpgme_plain, operation->gpgme_cipher);
	gpgme_key_unref (gpgme_keys[0]);

	if (error_gpgme != GPG_ERR_NO_ERROR) {
		cipher_operation_free (operation);
		g_critical (_("Error encrypting database: %s"), gpgme_strerror (error_gpgme));
		return FALSE;
	}

	gpgme_wait (operation->context, &error_gpgme, TRUE);
	if (error_gpgme != GPG_ERR_NO_ERROR) {
		g_critical (_("Error encrypting database: %s"), gpgme_strerror (error_gpgme));
		cipher_operation_free (operation);
		return FALSE;
	}

	cipher_operation_free (operation);
	return TRUE;
}

static gchar *
get_encryption_key (AlmanahSQLiteVFS *self)
{
	gchar *encryption_key;
	gchar **key_parts;
	guint i;

	encryption_key = g_settings_get_string (self->settings, "encryption-key");
	if (encryption_key == NULL || encryption_key[0] == '\0') {
		g_free (encryption_key);
		return NULL;
	}

	/* Key is generally in the form openpgp:FOOBARKEY, and GPGME doesn't like the openpgp: prefix, so it must be removed. */
	key_parts = g_strsplit (encryption_key, ":", 2);
	g_free (encryption_key);

	for (i = 0; key_parts[i] != NULL; i++) {
		if (strcmp (key_parts[i], "openpgp") != 0)
			encryption_key = strdup (key_parts[i]);
	}
	g_strfreev (key_parts);

	return encryption_key;
}

static gboolean
back_up_file (const gchar *filename)
{
	GError *error = NULL;
	GFile *original_file, *backup_file;
	gchar *backup_filename;
	gboolean retval = TRUE;

	/* Make a backup of the encrypted database file */
	original_file = g_file_new_for_path (filename);
	backup_filename = g_strdup_printf ("%s~", filename);
	backup_file = g_file_new_for_path (backup_filename);

	if (g_file_copy (original_file, backup_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error) == FALSE) {
		/* Translators: The first and second params are file paths, the last param is an error message.  */
		g_warning (_("Error copying the file from %s to %s: %s"), filename, backup_filename, error->message);
		retval = FALSE;
	}

	/* Ensure the backup is only readable to the current user. */
	if (g_chmod (backup_filename, 0600) != 0 && errno != ENOENT) {
		g_warning (_("Error changing database backup file permissions: %s"), g_strerror (errno));
		retval = FALSE;
	}

	g_free (backup_filename);
	g_object_unref (original_file);
	g_object_unref (backup_file);

	return retval;
}

/*
** Write directly to the file passed as the first argument. Even if the
** file has a write-buffer (AlmanahSQLiteVFS.aBuffer), ignore it.
*/
static int
almanah_vfs_direct_write (AlmanahSQLiteVFS *self,            /* File handle */
		 const void *buffer,               /* Buffer containing data to write */
		 int len,                       /* Size of data to write in bytes */
		 sqlite_int64 offset              /* File offset to write to */
		 )
{
	off_t ofst;
	size_t nWrite;

	if (self->decrypted) {
		if ((gsize) (offset + len) > self->plain_buffer_size) {
			gsize new_size;
			gsize exponential_size;
			gsize required_size;
			gpointer new_buffer = NULL;

			exponential_size = self->plain_size ? (2 * self->plain_size) : 512;
			required_size = offset + len;

			new_size = MAX (exponential_size, required_size);
			new_buffer = maybe_secure_memory_try_realloc (self->plain_buffer, new_size);
			if (new_buffer == NULL && (new_size > required_size)) {
				new_size = required_size;
				new_buffer = maybe_secure_memory_realloc (self->plain_buffer, new_size);
			}

			if (new_buffer == NULL)
				return SQLITE_NOMEM;

			self->plain_buffer = new_buffer;
			self->plain_buffer_size = new_size;
		}

		memcpy (self->plain_buffer + offset, buffer, len);
		self->plain_size = MAX(self->plain_size, (gsize) (offset + len));

		return SQLITE_OK;
	} else {
		ofst = lseek (self->fd, offset, SEEK_SET);
		if (ofst != offset) {
			return SQLITE_IOERR_WRITE;
		}

		nWrite = write (self->fd, buffer, len);
		if (nWrite != (size_t) len){
			return SQLITE_IOERR_WRITE;
		}

		return SQLITE_OK;
	}
}

/*
** Flush the contents of the AlmanahSQLiteVFS.aBuffer buffer to disk. This is a
** no-op if this particular file does not have a buffer (i.e. it is not
** a journal file) or if the buffer is currently empty.
*/
static int
almanah_vfs_flush_buffer (AlmanahSQLiteVFS *p)
{
	int rc = SQLITE_OK;

	if (p->decrypted)
		return rc;

	if (p->nBuffer) {
		rc = almanah_vfs_direct_write(p, p->aBuffer, p->nBuffer, p->iBufferOfst);
		p->nBuffer = 0;
	}

	return rc;
}

static int
almanah_vfs_close_simple_file (AlmanahSQLiteVFS *self)
{
	int rc;
	GError *error = NULL;

	rc = almanah_vfs_flush_buffer (self);
	if (rc != SQLITE_OK)
		return rc;
	sqlite3_free (self->aBuffer);
	if (g_close (self->fd, &error) == FALSE) {
		g_critical (_("Error closing file: %s"), error->message);
		rc = SQLITE_IOERR;
	}

	return rc;
}

/*
** Close a file.
*/
static int
almanah_vfs_io_close (sqlite3_file *pFile)
{
	AlmanahSQLiteVFS *self = (AlmanahSQLiteVFS*) pFile;
	gchar *encryption_key;
	GError *child_error = NULL;
	int rc;

	encryption_key = get_encryption_key (self);
	if (encryption_key == NULL) {
		if (self->decrypted) {
			/* Save the data from memory to plain file */
			GFile *plain_file;
			GFileOutputStream *plain_output_stream;
			gsize bytes_written;

			plain_file = g_file_new_for_path (self->plain_filename);
			plain_output_stream = g_file_create (plain_file,
							     G_FILE_CREATE_PRIVATE & G_FILE_CREATE_REPLACE_DESTINATION,
							     NULL,
							     &child_error);
			if (child_error != NULL) {
				g_warning ("Error opening plain file %s: %s", self->plain_filename, child_error->message);
				g_object_unref (plain_file);
				return SQLITE_IOERR;
			}

			if (g_output_stream_write_all (G_OUTPUT_STREAM (plain_output_stream),
						       self->plain_buffer,
						       self->plain_size,
						       &bytes_written,
						       NULL,
						       &child_error) == FALSE) {
				g_warning ("Error writing data to plain file %s: %s", self->plain_filename, child_error->message);
				g_object_unref (plain_file);
				g_object_unref (plain_output_stream);
				g_unlink (self->plain_filename);
				return SQLITE_IOERR;
			}

			if (bytes_written != self->plain_size) {
				g_warning ("Error writing data to plain file %s: %s", self->plain_filename, "Not all the data has been written to the file");
				g_object_unref (plain_file);
				g_object_unref (plain_output_stream);
				g_unlink (self->plain_filename);
				return SQLITE_IOERR;
			}

			if (g_output_stream_close (G_OUTPUT_STREAM (plain_output_stream), NULL, &child_error) == FALSE) {
				g_warning ("Error closing the plain file %s: %s", self->plain_filename, child_error->message);
				g_object_unref (plain_file);
				g_object_unref (plain_output_stream);
				g_unlink (self->plain_filename);
				return SQLITE_IOERR;
			}

			g_object_unref (plain_file);
			g_object_unref (plain_output_stream);
			g_unlink (self->encrypted_filename);

			rc = SQLITE_OK;
		} else {
			rc = almanah_vfs_close_simple_file (self);
		}
	} else {
		/* SQLITE_OPEN_MAIN_JOURNAL */
		if (self->aBuffer != NULL)
			rc = almanah_vfs_close_simple_file (self);
		else {
			gboolean from_memory;

			if (self->decrypted)
				from_memory = TRUE;
			else {
				rc = almanah_vfs_close_simple_file (self);
				if (rc != SQLITE_OK) {
					g_critical ("Error closing plain file");
					return rc;
				}
				from_memory = FALSE;
			}

			if (encrypt_database (self, encryption_key, from_memory, &child_error) != TRUE) {
				if (child_error != NULL)
					g_critical ("Error encrypting the database from the plain file %s: %s", self->plain_filename, child_error->message);
				return SQLITE_IOERR;
			}

			g_unlink (self->plain_filename);

			g_free (encryption_key);
			rc = SQLITE_OK;
		}
	}

	if (self->plain_buffer)
		maybe_secure_memory_free (self->plain_buffer);

	if (self->plain_filename)
		g_free (self->plain_filename);

	if (self->encrypted_filename)
		g_free (self->encrypted_filename);

	return rc;
}

/*
** Read data from a file.
*/
static int
almanah_vfs_io_read (sqlite3_file *pFile,  void *buffer,  int len,  sqlite_int64 offset)
{
	AlmanahSQLiteVFS *self = (AlmanahSQLiteVFS*) pFile;
	off_t ofst;
	int nRead;
	int rc;

	if (self->decrypted) {
		if ((gsize) (offset + len) > self->plain_size)
			return SQLITE_IOERR_SHORT_READ;

		memcpy (buffer, self->plain_buffer + offset, len);

		return SQLITE_OK;
	}

	/* Flush any data in the write buffer to disk in case this operation
	** is trying to read data the file-region currently cached in the buffer.
	** It would be possible to detect this case and possibly save an
	** unnecessary write here, but in practice SQLite will rarely read from
	** a journal file when there is data cached in the write-buffer.
	*/
	rc = almanah_vfs_flush_buffer (self);
	if (rc != SQLITE_OK) {
		return rc;
	}

	ofst = lseek (self->fd, offset, SEEK_SET);
	if (ofst != offset) {
		return SQLITE_IOERR_READ;
	}
	nRead = read (self->fd, buffer, len);

	if (nRead == len) {
		return SQLITE_OK;
	} else if (nRead >= 0) {
		return SQLITE_IOERR_SHORT_READ;
	}

	return SQLITE_IOERR_READ;
}

/*
** Write data to a crash-file.
*/
static int
almanah_vfs_io_write (sqlite3_file *pFile,  const void *buffer, int len, sqlite_int64 offset)
{
	AlmanahSQLiteVFS *self = (AlmanahSQLiteVFS*)pFile;

	if (self->decrypted)
		return almanah_vfs_direct_write (self, buffer, len, offset);

	if (self->aBuffer) {
		char *z = (char *)buffer;       /* Pointer to remaining data to write */
		int n = len;                 /* Number of bytes at z */
		sqlite3_int64 i = offset;      /* File offset to write to */

		while (n > 0) {
			int nCopy;                  /* Number of bytes to copy into buffer */

			/* If the buffer is full, or if this data is not being written directly
			** following the data already buffered, flush the buffer. Flushing
			** the buffer is a no-op if it is empty.
			*/
			if (self->nBuffer == SQLITE_DEMOVFS_BUFFERSZ || self->iBufferOfst+self->nBuffer != i) {
				int rc = almanah_vfs_flush_buffer (self);
				if (rc != SQLITE_OK) {
					return rc;
				}
			}
			assert (self->nBuffer==0 || self->iBufferOfst+self->nBuffer==i);
			self->iBufferOfst = i - self->nBuffer;

			/* Copy as much data as possible into the buffer. */
			nCopy = SQLITE_DEMOVFS_BUFFERSZ - self->nBuffer;
			if (nCopy > n) {
				nCopy = n;
			}
			memcpy (&self->aBuffer[self->nBuffer], z, nCopy);
			self->nBuffer += nCopy;

			n -= nCopy;
			i += nCopy;
			z += nCopy;
		}

		return SQLITE_OK;
	} else {
		return almanah_vfs_direct_write (self, buffer, len, offset);
	}
}

/*
** Truncate a file. This is a no-op for this VFS (see header comments at
** the top of the file).
*/
static int
almanah_vfs_io_truncate (__attribute__ ((unused)) sqlite3_file *pFile,
			 __attribute__ ((unused)) sqlite_int64 size)
{
#if 0
	if (ftruncate ( ((AlmanahSQLiteVFS *) pFile)->fd, size))
		return SQLITE_IOERR_TRUNCATE;
#endif
	return SQLITE_OK;
}

/*
** Sync the contents of the file to the persistent media.
*/
static int
almanah_vfs_io_sync (sqlite3_file *pFile, __attribute__ ((unused)) int flags)
{
	int rc;
	AlmanahSQLiteVFS *self = (AlmanahSQLiteVFS*) pFile;

	if (self->decrypted)
		return SQLITE_OK;

	rc = almanah_vfs_flush_buffer (self);
	if (rc != SQLITE_OK) {
		return rc;
	}

	rc = fsync (self->fd);
	return (rc == 0 ? SQLITE_OK : SQLITE_IOERR_FSYNC);
}

/*
** Write the size of the file in bytes to *pSize.
*/
static int
almanah_vfs_io_file_size (sqlite3_file *pFile, sqlite_int64 *pSize)
{
	AlmanahSQLiteVFS *self = (AlmanahSQLiteVFS*)pFile;
	int rc;
	struct stat sStat;

	if (self->decrypted) {
		*pSize = self->plain_size;
		return SQLITE_OK;
	}

	/* Flush the contents of the buffer to disk. As with the flush in the
	** almanah_vfs_io_read() method, it would be possible to avoid this and save a write
	** here and there. But in practice this comes up so infrequently it is
	** not worth the trouble.
	*/
	rc = almanah_vfs_flush_buffer (self);
	if (rc != SQLITE_OK) {
		return rc;
	}

	rc = fstat (self->fd, &sStat);
	if (rc != 0)
		return SQLITE_IOERR_FSTAT;

	*pSize = sStat.st_size;

	return SQLITE_OK;
}

/*
** Locking functions. The xLock() and xUnlock() methods are both no-ops.
** The xCheckReservedLock() always indicates that no other process holds
** a reserved lock on the database file. This ensures that if a hot-journal
** file is found in the file-system it is rolled back.
*/
static int
almanah_vfs_io_lock (__attribute__ ((unused)) sqlite3_file *pFile,
		     __attribute__ ((unused)) int eLock)
{
	return SQLITE_OK;
}
static int
almanah_vfs_io_unlock (__attribute__ ((unused)) sqlite3_file *pFile,
		       __attribute__ ((unused)) int eLock)
{
	return SQLITE_OK;
}
static int
almanah_vfs_io_reserved_lock (__attribute__ ((unused)) sqlite3_file *pFile, int *pResOut)
{
	*pResOut = 0;
	return SQLITE_OK;
}

/*
** No xFileControl() verbs are implemented by this VFS.
*/
static int
almanah_vfs_io_file_control (__attribute__ ((unused)) sqlite3_file *pFile,
			     __attribute__ ((unused)) int op,
			     __attribute__ ((unused)) void *pArg)
{
	return SQLITE_OK;
}

/*
** The xSectorSize() and xDeviceCharacteristics() methods. These two
** may return special values allowing SQLite to optimize file-system
** access to some extent. But it is also safe to simply return 0.
*/
static int
almanah_vfs_io_sector_size (__attribute__ ((unused)) sqlite3_file *pFile)
{
	return 0;
}
static int
almanah_vfs_io_device_characteristis(__attribute__ ((unused)) sqlite3_file *pFile)
{
	return 0;
}

/*
** Open a file handle.
*/
static int
almanah_vfs_open (sqlite3_vfs *pVfs,
		  const char *zName,
		  sqlite3_file *pFile,
		  int flags,
		  int *pOutFlags)
{
	static const sqlite3_io_methods almanah_vfs_io = {
		1,
		almanah_vfs_io_close,
		almanah_vfs_io_read,
		almanah_vfs_io_write,
		almanah_vfs_io_truncate,
		almanah_vfs_io_sync,
		almanah_vfs_io_file_size,
		almanah_vfs_io_lock,
		almanah_vfs_io_unlock,
		almanah_vfs_io_reserved_lock,
		almanah_vfs_io_file_control,
		almanah_vfs_io_sector_size,
		almanah_vfs_io_device_characteristis
	};

	AlmanahSQLiteVFS *self = (AlmanahSQLiteVFS*) pFile;
	int oflags = 0;
	char *aBuf = NULL;
	struct stat encrypted_db_stat, plaintext_db_stat;
	GError *child_error = NULL;

	if (zName == 0) {
		return SQLITE_IOERR;
	}

	if (flags & SQLITE_OPEN_MAIN_JOURNAL) {
		aBuf = (char *) sqlite3_malloc(SQLITE_DEMOVFS_BUFFERSZ);
		if(!aBuf) {
			return SQLITE_NOMEM;
		}
	}

	memset(self, 0, sizeof(AlmanahSQLiteVFS));

	self->plain_filename = g_strdup (zName);
	self->decrypted = FALSE;

	if (flags & SQLITE_OPEN_MAIN_DB) {
		self->encrypted_filename = g_strdup_printf ("%s%s", self->plain_filename, ENCRYPTED_SUFFIX);

		if (g_chmod (self->encrypted_filename, 0600) != 0 && errno != ENOENT) {
			return SQLITE_IOERR;
		}

		g_stat (self->encrypted_filename, &encrypted_db_stat);

		/* If we're decrypting, don't bother if the cipher file doesn't exist (i.e. the database hasn't yet been created), or is empty
		 * (i.e. corrupt). */
		if (g_file_test (self->encrypted_filename, G_FILE_TEST_IS_REGULAR) == TRUE && encrypted_db_stat.st_size > 0) {
			/* Make a backup of the encrypted database file */
			if (back_up_file (self->encrypted_filename) == FALSE) {
				/* Translators: the first parameter is a filename. */
				g_warning (_("Error backing up file ‘%s’"), self->encrypted_filename);
				g_clear_error (&child_error);
			}

			g_stat (self->plain_filename, &plaintext_db_stat);

			/* Only decrypt the database if the plaintext database doesn't exist or is empty. If the plaintext database exists and is non-empty,
			 * don't decrypt — just use that database. */
			if (g_file_test (self->plain_filename, G_FILE_TEST_IS_REGULAR) != TRUE || plaintext_db_stat.st_size == 0) {
				/* Decrypt the database, or display an error if that fails (but not if it fails due to a missing encrypted DB file — just
				 * fall through and try to open the plain DB file in that case). */
				if (decrypt_database (self, &child_error) != TRUE) {
					if (child_error != NULL && child_error->code != G_FILE_ERROR_NOENT) {
						g_warning (_("Error decrypting database: %s"), child_error->message);
						g_free (self->plain_filename);
						g_free (self->encrypted_filename);
						return SQLITE_IOERR;
					}

					g_error_free (child_error);
				} else
					self->decrypted = TRUE;
			}
		} else {
			/* Make a backup of the plaintext database file */
			if (g_file_test (self->encrypted_filename, G_FILE_TEST_IS_REGULAR) == TRUE && back_up_file (self->plain_filename) != TRUE) {
				/* Translators: the first parameter is a filename. */
				g_warning (_("Error backing up file ‘%s’"), self->plain_filename);
				g_clear_error (&child_error);
			}
		}
	}

	if (self->decrypted) {
		sqlite3_free (aBuf);
		*pOutFlags = 0;
	} else {
		if (flags & SQLITE_OPEN_EXCLUSIVE) oflags |= O_EXCL;
		if (flags & SQLITE_OPEN_CREATE)    oflags |= O_CREAT;
		if (flags & SQLITE_OPEN_READONLY)  oflags |= O_RDONLY;
		if (flags & SQLITE_OPEN_READWRITE) oflags |= O_RDWR;

		self->fd = g_open (self->plain_filename, oflags, 0600);
		if (self->fd < 0) {
			sqlite3_free (aBuf);
			if (self->plain_filename)
				g_free (self->plain_filename);
			if (self->encrypted_filename)
				g_free (self->encrypted_filename);
			return SQLITE_CANTOPEN;
		}

		if (g_chmod (self->plain_filename, 0600) != 0 && errno != ENOENT) {
			g_critical (_("Error changing database file permissions: %s"), g_strerror (errno));
			sqlite3_free (aBuf);
			if (self->plain_filename)
				g_free (self->plain_filename);
			if (self->encrypted_filename)
				g_free (self->encrypted_filename);
			close (self->fd);
			return SQLITE_IOERR;
		}

		self->aBuffer = aBuf;

		if (pOutFlags) {
			*pOutFlags = flags;
		}
	}

	self->settings = (GSettings *) pVfs->pAppData;
	self->base.pMethods = &almanah_vfs_io;

	return SQLITE_OK;
}

/*
** Delete the file identified by argument zPath. If the dirSync parameter
** is non-zero, then ensure the file-system modification to delete the
** file has been synced to disk before returning.
*/
static int
almanah_vfs_delete (__attribute__ ((unused)) sqlite3_vfs *pVfs, const char *zPath, int dirSync)
{
	int rc;

	rc = unlink (zPath);
	if (rc != 0 && errno == ENOENT) return SQLITE_OK;

	if( rc==0 && dirSync) {
		int dfd;
		int i;
		char zDir[MAXPATHNAME + 1];

		/* Figure out the directory name from the path of the file deleted. */
		sqlite3_snprintf (MAXPATHNAME, zDir, "%s", zPath);
		zDir[MAXPATHNAME] = '\0';
		for (i = strlen(zDir); i > 1 && zDir[i] != '/'; i++);
		zDir[i] = '\0';

		/* Open a file-descriptor on the directory. Sync. Close. */
		dfd = g_open (zDir, O_RDONLY, 0);
		if (dfd < 0) {
			rc = -1;
		} else {
			rc = fsync (dfd);
			g_close (dfd, NULL);
		}
	}
	return (rc == 0 ? SQLITE_OK : SQLITE_IOERR_DELETE);
}

#ifndef F_OK
# define F_OK 0
#endif
#ifndef R_OK
# define R_OK 4
#endif
#ifndef W_OK
# define W_OK 2
#endif

/*
** Query the file-system to see if the named file exists, is readable or
** is both readable and writable.
*/
static int
almanah_vfs_access (__attribute__ ((unused)) sqlite3_vfs *pVfs, const char *zPath, int flags, int *pResOut)
{
	int rc;
	int eAccess = F_OK;

	assert (flags == SQLITE_ACCESS_EXISTS          /* access(zPath, F_OK) */
		|| flags == SQLITE_ACCESS_READ         /* access(zPath, R_OK) */
		|| flags == SQLITE_ACCESS_READWRITE    /* access(zPath, R_OK|W_OK) */
		);

	if (flags == SQLITE_ACCESS_READWRITE) eAccess = R_OK|W_OK;
	if (flags == SQLITE_ACCESS_READ)      eAccess = R_OK;

	rc = access (zPath, eAccess);
	*pResOut = (rc == 0);

	return SQLITE_OK;
}

/*
** Argument zPath points to a nul-terminated string containing a file path.
** If zPath is an absolute path, then it is copied as is into the output
** buffer. Otherwise, if it is a relative path, then the equivalent full
** path is written to the output buffer.
**
** This function assumes that paths are UNIX style. Specifically, that:
**
**   1. Path components are separated by a '/'. and
**   2. Full paths begin with a '/' character.
*/
static int
almanah_vfs_full_pathname (__attribute__ ((unused)) sqlite3_vfs *pVfs, const char *zPath, int nPathOut, char *zPathOut)
{
	char zDir[MAXPATHNAME+1];

	if (zPath[0] == '/') {
		zDir[0] = '\0';
	} else {
		getcwd (zDir, sizeof (zDir));
	}
	zDir[MAXPATHNAME] = '\0';

	sqlite3_snprintf (nPathOut, zPathOut, "%s/%s", zDir, zPath);
	zPathOut[nPathOut-1] = '\0';

	return SQLITE_OK;
}

/*
** The following four VFS methods:
**
**   xDlOpen
**   xDlError
**   xDlSym
**   xDlClose
**
** are supposed to implement the functionality needed by SQLite to load
** extensions compiled as shared objects. This simple VFS does not support
** this functionality, so the following functions are no-ops.
*/
static void*
almanah_vfs_dl_open (__attribute__ ((unused)) sqlite3_vfs *pVfs, __attribute__ ((unused)) const char *zPath)
{
	return 0;
}

static void
almanah_vfs_dl_error (__attribute__ ((unused)) sqlite3_vfs *pVfs, int nByte, char *zErrMsg)
{
	sqlite3_snprintf (nByte, zErrMsg, "Loadable extensions are not supported");
	zErrMsg[nByte-1] = '\0';
}

static void
(*almanah_vfs_dl_sym (__attribute__ ((unused)) sqlite3_vfs *pVfs, __attribute__ ((unused)) void *pH, __attribute__ ((unused)) const char *z)) (void)
{
	return 0;
}

static void
almanah_vfs_dl_close (__attribute__ ((unused)) sqlite3_vfs *pVfs, __attribute__ ((unused)) void *pHandle)
{
	return;
}

/*
** Parameter zByte points to a buffer nByte bytes in size. Populate this
** buffer with pseudo-random data.
*/
static int
almanah_vfs_randomness (__attribute__ ((unused)) sqlite3_vfs *pVfs, __attribute__ ((unused)) int nByte, __attribute__ ((unused)) char *zByte)
{
	return SQLITE_OK;
}

/*
** Sleep for at least nMicro microseconds. Return the (approximate) number
** of microseconds slept for.
*/
static int
almanah_vfs_sleep (__attribute__ ((unused)) sqlite3_vfs *pVfs, int nMicro)
{
	sleep (nMicro / 1000000);
	usleep (nMicro % 1000000);
	return nMicro;
}

/*
** Set *pTime to the current UTC time expressed as a Julian day. Return
** SQLITE_OK if successful, or an error code otherwise.
**
**   http://en.wikipedia.org/wiki/Julian_day
**
** This implementation is not very good. The current time is rounded to
** an integer number of seconds. Also, assuming time_t is a signed 32-bit
** value, it will stop working some time in the year 2038 AD (the so-called
** "year 2038" problem that afflicts systems that store time this way).
*/
static int
almanah_vfs_current_time (__attribute__ ((unused)) sqlite3_vfs *pVfs, double *pTime)
{
	time_t t = time (0);
	*pTime = t / 86400.0 + 2440587.5;
	return SQLITE_OK;
}

/*
** This function returns a pointer to the VFS implemented in this file.
** To make the VFS available to SQLite:
**
**   sqlite3_vfs_register(sqlite3_almanah_vfs(settings), 0);
*/
static sqlite3_vfs *
sqlite3_almanah_vfs (GSettings *settings)
{
	if (almanah_vfs_singleton == NULL) {
		almanah_vfs_singleton = (sqlite3_vfs *) g_new0(sqlite3_vfs, 1);
		almanah_vfs_singleton->iVersion = 1;
		almanah_vfs_singleton->szOsFile = sizeof(AlmanahSQLiteVFS);
		almanah_vfs_singleton->mxPathname = MAXPATHNAME;
		almanah_vfs_singleton->zName = "almanah";
		almanah_vfs_singleton->pAppData = settings;
		almanah_vfs_singleton->xOpen = almanah_vfs_open;
		almanah_vfs_singleton->xDelete = almanah_vfs_delete;
		almanah_vfs_singleton->xAccess = almanah_vfs_access;
		almanah_vfs_singleton->xFullPathname = almanah_vfs_full_pathname;
		almanah_vfs_singleton->xDlOpen = almanah_vfs_dl_open;
		almanah_vfs_singleton->xDlError = almanah_vfs_dl_error;
		almanah_vfs_singleton->xDlSym  = almanah_vfs_dl_sym;
		almanah_vfs_singleton->xDlClose = almanah_vfs_dl_close;
		almanah_vfs_singleton->xRandomness = almanah_vfs_randomness;
		almanah_vfs_singleton->xSleep = almanah_vfs_sleep;
		almanah_vfs_singleton->xCurrentTime = almanah_vfs_current_time;
	}

	return almanah_vfs_singleton;
}

int
almanah_vfs_init (GSettings *settings)
{
	return sqlite3_vfs_register (sqlite3_almanah_vfs (settings), 0);
}

void
almanah_vfs_finish (void)
{
	if (almanah_vfs_singleton)
		sqlite3_vfs_unregister (almanah_vfs_singleton);

	g_free (almanah_vfs_singleton);
	almanah_vfs_singleton = NULL;
}
