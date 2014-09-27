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

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <config.h>

#ifdef ENABLE_ENCRYPTION
#include <gpgme.h>
#define ENCRYPTED_SUFFIX ".encrypted"
#endif /* ENABLE_ENCRYPTION */

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
typedef struct AlmanahSQLiteVFS AlmanahSQLiteVFS;
struct AlmanahSQLiteVFS
{
	sqlite3_file base;              /* Base class. Must be first. */
	int fd;                         /* File descriptor */

	char *aBuffer;                  /* Pointer to malloc'd buffer */
	int nBuffer;                    /* Valid bytes of data in zBuffer */
	sqlite3_int64 iBufferOfst;      /* Offset in file of zBuffer[0] */

	gchar *plain_filename;
#ifdef ENABLE_ENCRYPTION
	gchar *encrypted_filename;
	gboolean decrypted;
#endif /* ENABLE_ENCRYPTION */
};

#ifdef ENABLE_ENCRYPTION

typedef struct {
	GIOChannel *cipher_io_channel;
	GIOChannel *plain_io_channel;
	gpgme_data_t gpgme_cipher;
	gpgme_data_t gpgme_plain;
	gpgme_ctx_t context;
	AlmanahSQLiteVFS *vfs;
} CipherOperation;

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
open_db_files (AlmanahSQLiteVFS *self, gboolean encrypting, CipherOperation *operation, GError **error)
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

	/* Open/Create the plain file */
	operation->plain_io_channel = g_io_channel_new_file (self->plain_filename, encrypting ? "r" : "w", &io_error);
	if (operation->plain_io_channel == NULL) {
		g_critical (_("Can't create a new GIOChannel for the plain database: %s"), io_error->message);
		g_propagate_error (error, io_error);
		return FALSE;
	}

	/* Ensure the permissions are restricted to only the current user */
	fchmod (g_io_channel_unix_get_fd (operation->plain_io_channel), S_IRWXU);

	/* Pass it to GPGME */
	error_gpgme = gpgme_data_new_from_fd (&(operation->gpgme_plain), g_io_channel_unix_get_fd (operation->plain_io_channel));
	if (error_gpgme != GPG_ERR_NO_ERROR) {
		g_critical (_("Error opening plain database file \"%s\": %s"), self->plain_filename, gpgme_strerror (error_gpgme));
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

	/* We could free the operation before the context is even created (bgo#599598) */
	if (operation->context != NULL) {
		gpgme_signers_clear (operation->context);
		gpgme_release (operation->context);
	}

	g_free (operation);
}

static gboolean
decrypt_database (AlmanahSQLiteVFS *self, GError **error)
{
	GError *preparation_error = NULL;
	CipherOperation *operation;
	gpgme_error_t error_gpgme;

	operation = g_new0 (CipherOperation, 1);

	/* Set up */
	if (prepare_gpgme (operation) != TRUE || open_db_files (self, FALSE, operation, &preparation_error) != TRUE) {
		cipher_operation_free (operation);
		g_propagate_error (error, preparation_error);
		return FALSE;
	}

	/* Decrypt and verify! */
	error_gpgme = gpgme_op_decrypt_verify (operation->context, operation->gpgme_cipher, operation->gpgme_plain);
	if (error_gpgme != GPG_ERR_NO_ERROR) {
		cipher_operation_free (operation);
		g_critical (_("Error decrypting database: %s"), gpgme_strerror (error_gpgme));
		return FALSE;
	}

	/* Do this one synchronously */
	cipher_operation_free (operation);

	return TRUE;
}

static gboolean
encrypt_database (AlmanahSQLiteVFS *self,  const gchar *encryption_key, GError **error)
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

	if (open_db_files (self, TRUE, operation, &preparation_error) != TRUE) {
		cipher_operation_free (operation);
		g_propagate_error (error, preparation_error);
		return FALSE;
	}

	/* Encrypt and sign! */
	error_gpgme = gpgme_op_encrypt_sign_start (operation->context, gpgme_keys, 0, operation->gpgme_plain, operation->gpgme_cipher);
	gpgme_key_unref (gpgme_keys[0]);

	if (error_gpgme != GPG_ERR_NO_ERROR) {
		cipher_operation_free (operation);
		g_critical (_("Error encrypting database: %s"), gpgme_strerror (error_gpgme));
		return FALSE;
	}

	if (gpgme_wait (operation->context, &error_gpgme, TRUE) != NULL || error_gpgme != GPG_ERR_NO_ERROR) {
		struct stat db_stat;
		gchar *warning_message = NULL;

		/* Check to see if the encrypted file is 0B in size, which isn't good. Not much we can do about it except quit without deleting the
		 * plaintext database. */
		g_stat (operation->vfs->encrypted_filename, &db_stat);
		if (g_file_test (operation->vfs->encrypted_filename, G_FILE_TEST_IS_REGULAR) == FALSE || db_stat.st_size == 0) {
			warning_message = g_strdup (_("The encrypted database is empty. The plain database file has been left undeleted as backup."));
		} else if (g_unlink (operation->vfs->plain_filename) != 0) {
			/* Delete the plain file */
			warning_message = g_strdup_printf (_("Could not delete plain database file \"%s\"."), operation->vfs->plain_filename);
		}

		if (warning_message) {
			g_critical (warning_message);
			g_free (warning_message);
		}

		/* Finished! */
		cipher_operation_free (operation);

		return FALSE;
	}

	return TRUE;

	return TRUE;
}

static gchar *
get_encryption_key (void)
{
	GSettings *settings;
	gchar *encryption_key;
	gchar **key_parts;
	guint i;

	settings = g_settings_new ("org.gnome.almanah");
	encryption_key = g_settings_get_string (settings, "encryption-key");
	if (encryption_key == NULL || encryption_key[0] == '\0') {
		g_free (encryption_key);
		return NULL;
	}

	/* Key is generally in the form openpgp:FOOBARKEY, and GPGME doesn't like the openpgp: prefix, so it must be removed. */
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
	g_free (backup_filename);

	if (g_file_copy (original_file, backup_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error) == FALSE) {
		/* Translators: The first and second params are file paths, the last param is an error message.  */
		g_warning (_("Error copying the file from %s to %s: %s"), original_file, backup_file, error->message);
		retval = FALSE;
	}

	/* Ensure the backup is only readable to the current user. */
	if (g_chmod (backup_filename, 0600) != 0 && errno != ENOENT) {
		g_warning (_("Error changing database backup file permissions: %s"), g_strerror (errno));
		retval = FALSE;
	}

	g_object_unref (original_file);
	g_object_unref (backup_file);

	return retval;
}

/*
** Write directly to the file passed as the first argument. Even if the
** file has a write-buffer (AlmanahSQLiteVFS.aBuffer), ignore it.
*/
static int
demoDirectWrite (AlmanahSQLiteVFS *p,            /* File handle */
		 const void *zBuf,               /* Buffer containing data to write */
		 int iAmt,                       /* Size of data to write in bytes */
		 sqlite_int64 iOfst              /* File offset to write to */
		 )
{
	off_t ofst;                     /* Return value from lseek() */
	size_t nWrite;                  /* Return value from write() */

	ofst = lseek (p->fd, iOfst, SEEK_SET);
	if (ofst!=iOfst) {
		return SQLITE_IOERR_WRITE;
	}

	nWrite = write (p->fd, zBuf, iAmt);
	if (nWrite != iAmt){
		return SQLITE_IOERR_WRITE;
	}

	return SQLITE_OK;
}

/*
** Flush the contents of the AlmanahSQLiteVFS.aBuffer buffer to disk. This is a
** no-op if this particular file does not have a buffer (i.e. it is not
** a journal file) or if the buffer is currently empty.
*/
static int demoFlushBuffer(AlmanahSQLiteVFS *p){
	int rc = SQLITE_OK;
	if( p->nBuffer ){
		rc = demoDirectWrite(p, p->aBuffer, p->nBuffer, p->iBufferOfst);
		p->nBuffer = 0;
	}
	return rc;
}

/*
** Close a file.
*/
static int
demoClose (sqlite3_file *pFile)
{
	int rc;
	AlmanahSQLiteVFS *self = (AlmanahSQLiteVFS*) pFile;
#ifdef ENABLE_ENCRYPTION
	gchar *encryption_key;
	GError *child_error = NULL;
#endif /* ENABLE_ENCRYPTION */

	rc = demoFlushBuffer (self);
	sqlite3_free (self->aBuffer);
	close (self->fd);

#ifdef ENABLE_ENCRYPTION
	/* If the database wasn't encrypted before we opened it, we won't encrypt it when closing. In fact, we'll go so far as to delete the old
	 * encrypted database file. */
	if (self->decrypted == FALSE)
		goto delete_encrypted_db;

	/* Get the encryption key */
	encryption_key = get_encryption_key ();
	if (encryption_key == NULL)
		goto delete_encrypted_db;

	/* Encrypt the plain DB file */
	if (encrypt_database (self, encryption_key, &child_error) != TRUE) {
		rc = SQLITE_IOERR;
	}

	g_free (encryption_key);
#endif /* ENABLE_ENCRYPTION */

	return rc;

#ifdef ENABLE_ENCRYPTION
 delete_encrypted_db:
	/* Delete the old encrypted database and return */
	g_unlink (self->encrypted_filename);
	return rc;
#endif /* ENABLE_ENCRYPTION */
}

/*
** Read data from a file.
*/
static int
demoRead (sqlite3_file *pFile,  void *zBuf,  int iAmt,  sqlite_int64 iOfst)
{
	AlmanahSQLiteVFS *p = (AlmanahSQLiteVFS*)pFile;
	off_t ofst;                     /* Return value from lseek() */
	int nRead;                      /* Return value from read() */
	int rc;                         /* Return code from demoFlushBuffer() */

	/* Flush any data in the write buffer to disk in case this operation
	** is trying to read data the file-region currently cached in the buffer.
	** It would be possible to detect this case and possibly save an 
	** unnecessary write here, but in practice SQLite will rarely read from
	** a journal file when there is data cached in the write-buffer.
	*/
	rc = demoFlushBuffer (p);
	if (rc!=SQLITE_OK) {
		return rc;
	}

	ofst = lseek (p->fd, iOfst, SEEK_SET);
	if (ofst!=iOfst) {
		return SQLITE_IOERR_READ;
	}
	nRead = read (p->fd, zBuf, iAmt);

	if (nRead==iAmt) {
		return SQLITE_OK;
	} else if (nRead>=0) {
		return SQLITE_IOERR_SHORT_READ;
	}

	return SQLITE_IOERR_READ;
}

/*
** Write data to a crash-file.
*/
static int
demoWrite (sqlite3_file *pFile,  const void *zBuf, int iAmt, sqlite_int64 iOfst)
{
	AlmanahSQLiteVFS *p = (AlmanahSQLiteVFS*)pFile;
  
	if (p->aBuffer) {
		char *z = (char *)zBuf;       /* Pointer to remaining data to write */
		int n = iAmt;                 /* Number of bytes at z */
		sqlite3_int64 i = iOfst;      /* File offset to write to */

		while (n > 0) {
			int nCopy;                  /* Number of bytes to copy into buffer */

			/* If the buffer is full, or if this data is not being written directly
			** following the data already buffered, flush the buffer. Flushing
			** the buffer is a no-op if it is empty.  
			*/
			if (p->nBuffer==SQLITE_DEMOVFS_BUFFERSZ || p->iBufferOfst+p->nBuffer!=i) {
				int rc = demoFlushBuffer (p);
				if (rc!=SQLITE_OK) {
					return rc;
				}
			}
			assert (p->nBuffer==0 || p->iBufferOfst+p->nBuffer==i);
			p->iBufferOfst = i - p->nBuffer;

			/* Copy as much data as possible into the buffer. */
			nCopy = SQLITE_DEMOVFS_BUFFERSZ - p->nBuffer;
			if (nCopy>n) {
				nCopy = n;
			}
			memcpy (&p->aBuffer[p->nBuffer], z, nCopy);
			p->nBuffer += nCopy;

			n -= nCopy;
			i += nCopy;
			z += nCopy;
		}
	} else {
		return demoDirectWrite (p, zBuf, iAmt, iOfst);
	}

	return SQLITE_OK;
}

/*
** Truncate a file. This is a no-op for this VFS (see header comments at
** the top of the file).
*/
static int
demoTruncate(sqlite3_file *pFile, sqlite_int64 size)
{
#if 0
	if (ftruncate(((AlmanahSQLiteVFS *)pFile)->fd, size))
		return SQLITE_IOERR_TRUNCATE;
#endif
	return SQLITE_OK;
}

/*
** Sync the contents of the file to the persistent media.
*/
static int
demoSync (sqlite3_file *pFile, int flags)
{
	AlmanahSQLiteVFS *p = (AlmanahSQLiteVFS*) pFile;
	int rc;

	rc = demoFlushBuffer (p);
	if (rc!=SQLITE_OK) {
		return rc;
	}

	rc = fsync (p->fd);
	return (rc==0 ? SQLITE_OK : SQLITE_IOERR_FSYNC);
}

/*
** Write the size of the file in bytes to *pSize.
*/
static int
demoFileSize (sqlite3_file *pFile, sqlite_int64 *pSize)
{
	AlmanahSQLiteVFS *p = (AlmanahSQLiteVFS*)pFile;
	int rc;                         /* Return code from fstat() call */
	struct stat sStat;              /* Output of fstat() call */

	/* Flush the contents of the buffer to disk. As with the flush in the
	** demoRead() method, it would be possible to avoid this and save a write
	** here and there. But in practice this comes up so infrequently it is
	** not worth the trouble.
	*/
	rc = demoFlushBuffer(p);
	if (rc != SQLITE_OK) {
		return rc;
	}

	rc = fstat (p->fd, &sStat);
	if (rc!=0)
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
static int demoLock(sqlite3_file *pFile, int eLock){
	return SQLITE_OK;
}
static int demoUnlock(sqlite3_file *pFile, int eLock){
	return SQLITE_OK;
}
static int demoCheckReservedLock(sqlite3_file *pFile, int *pResOut){
	*pResOut = 0;
	return SQLITE_OK;
}

/*
** No xFileControl() verbs are implemented by this VFS.
*/
static int demoFileControl(sqlite3_file *pFile, int op, void *pArg){
	return SQLITE_OK;
}

/*
** The xSectorSize() and xDeviceCharacteristics() methods. These two
** may return special values allowing SQLite to optimize file-system 
** access to some extent. But it is also safe to simply return 0.
*/
static int demoSectorSize(sqlite3_file *pFile){
	return 0;
}
static int demoDeviceCharacteristics(sqlite3_file *pFile){
	return 0;
}

/*
** Open a file handle.
*/
static int
demoOpen (__attribute__ ((unused)) sqlite3_vfs *pVfs, /* VFS */
	  const char *zName,                          /* File to open, or 0 for a temp file */
	  sqlite3_file *pFile,                        /* Pointer to AlmanahSQLiteVFS struct to populate */
	  int flags,                                  /* Input SQLITE_OPEN_XXX flags */
	  int *pOutFlags                              /* Output SQLITE_OPEN_XXX flags (or NULL) */
	 )
{
	static const sqlite3_io_methods demoio = {
		1,                            /* iVersion */
		demoClose,                    /* xClose */
		demoRead,                     /* xRead */
		demoWrite,                    /* xWrite */
		demoTruncate,                 /* xTruncate */
		demoSync,                     /* xSync */
		demoFileSize,                 /* xFileSize */
		demoLock,                     /* xLock */
		demoUnlock,                   /* xUnlock */
		demoCheckReservedLock,        /* xCheckReservedLock */
		demoFileControl,              /* xFileControl */
		demoSectorSize,               /* xSectorSize */
		demoDeviceCharacteristics     /* xDeviceCharacteristics */
	};

	AlmanahSQLiteVFS *self = (AlmanahSQLiteVFS*) pFile; /* Populate this structure */
	int oflags = 0;                 /* flags to pass to open() call */
	char *aBuf = 0;

	if (zName == 0) {
		return SQLITE_IOERR;
	}

	if (flags & SQLITE_OPEN_MAIN_JOURNAL){
		aBuf = (char *) sqlite3_malloc(SQLITE_DEMOVFS_BUFFERSZ);
		if(!aBuf) {
			return SQLITE_NOMEM;
		}
	}

	memset(self, 0, sizeof(AlmanahSQLiteVFS));

	self->plain_filename = g_strdup (zName);

#ifdef ENABLE_ENCRYPTION
	struct stat encrypted_db_stat, plaintext_db_stat;
	GError *child_error = NULL;

	self->encrypted_filename = g_strdup_printf ("%s%s", self->plain_filename, ENCRYPTED_SUFFIX);

	if (g_chmod (self->encrypted_filename, 0600) != 0 && errno != ENOENT) {
		return SQLITE_IOERR;
	}

	g_stat (self->encrypted_filename, &encrypted_db_stat);

	/* If we're decrypting, don't bother if the cipher file doesn't exist (i.e. the database hasn't yet been created), or is empty
	 * (i.e. corrupt). */
	if (g_file_test (self->encrypted_filename, G_FILE_TEST_IS_REGULAR) == TRUE && encrypted_db_stat.st_size > 0) {
		/* Make a backup of the encrypted database file */
		if (back_up_file (self->encrypted_filename) != FALSE) {
			/* Translators: the first parameter is a filename, the second is an error message. */
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
				if (child_error->code != G_FILE_ERROR_NOENT) {
					g_free (self->plain_filename);
					g_free (self->encrypted_filename);
					return SQLITE_IOERR;
				}

				g_error_free (child_error);
			}
		}
	}

	self->decrypted = TRUE;
#else
	/* Make a backup of the plaintext database file */
	if (back_up_file (self->plain_filename) != TRUE) {
		/* Translators: the first parameter is a filename. */
		g_warning (_("Error backing up file ‘%s’"), self->priv->plain_filename);
		g_clear_error (&child_error);
	}
	self->decrypted = FALSE;
#endif /* ENABLE_ENCRYPTION */

	if (flags & SQLITE_OPEN_EXCLUSIVE) oflags |= O_EXCL;
	if (flags & SQLITE_OPEN_CREATE)    oflags |= O_CREAT;
	if (flags & SQLITE_OPEN_READONLY)  oflags |= O_RDONLY;
	if (flags & SQLITE_OPEN_READWRITE) oflags |= O_RDWR;

	self->fd = open (self->plain_filename, oflags, 0600);
	if (self->fd < 0) {
		sqlite3_free (aBuf);
		return SQLITE_CANTOPEN;
	}

	if (g_chmod (self->plain_filename, 0600) != 0 && errno != ENOENT) {
		g_critical (_("Error changing database file permissions: %s"), g_strerror (errno));
		sqlite3_free (aBuf);
		g_free (self->plain_filename);
		g_free (self->encrypted_filename);
		close (self->fd);
		return SQLITE_IOERR;
	}

	self->aBuffer = aBuf;

	if (pOutFlags) {
		*pOutFlags = flags;
	}

	self->base.pMethods = &demoio;

	return SQLITE_OK;
}

/*
** Delete the file identified by argument zPath. If the dirSync parameter
** is non-zero, then ensure the file-system modification to delete the
** file has been synced to disk before returning.
*/
static int demoDelete(sqlite3_vfs *pVfs, const char *zPath, int dirSync){
	int rc;                         /* Return code */

	rc = unlink(zPath);
	if( rc!=0 && errno==ENOENT ) return SQLITE_OK;

	if( rc==0 && dirSync ){
		int dfd;                      /* File descriptor open on directory */
		int i;                        /* Iterator variable */
		char zDir[MAXPATHNAME+1];     /* Name of directory containing file zPath */

		/* Figure out the directory name from the path of the file deleted. */
		sqlite3_snprintf(MAXPATHNAME, zDir, "%s", zPath);
		zDir[MAXPATHNAME] = '\0';
		for(i=strlen(zDir); i>1 && zDir[i]!='/'; i++);
		zDir[i] = '\0';

		/* Open a file-descriptor on the directory. Sync. Close. */
		dfd = open(zDir, O_RDONLY, 0);
		if( dfd<0 ){
			rc = -1;
		}else{
			rc = fsync(dfd);
			close(dfd);
		}
	}
	return (rc==0 ? SQLITE_OK : SQLITE_IOERR_DELETE);
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
static int demoAccess(
		      sqlite3_vfs *pVfs, 
		      const char *zPath, 
		      int flags, 
		      int *pResOut
		      ){
	int rc;                         /* access() return code */
	int eAccess = F_OK;             /* Second argument to access() */

	assert( flags==SQLITE_ACCESS_EXISTS       /* access(zPath, F_OK) */
		|| flags==SQLITE_ACCESS_READ         /* access(zPath, R_OK) */
		|| flags==SQLITE_ACCESS_READWRITE    /* access(zPath, R_OK|W_OK) */
		);

	if( flags==SQLITE_ACCESS_READWRITE ) eAccess = R_OK|W_OK;
	if( flags==SQLITE_ACCESS_READ )      eAccess = R_OK;

	rc = access(zPath, eAccess);
	*pResOut = (rc==0);
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
static int demoFullPathname(
			    sqlite3_vfs *pVfs,              /* VFS */
			    const char *zPath,              /* Input path (possibly a relative path) */
			    int nPathOut,                   /* Size of output buffer in bytes */
			    char *zPathOut                  /* Pointer to output buffer */
			    ){
	char zDir[MAXPATHNAME+1];
	if( zPath[0]=='/' ){
		zDir[0] = '\0';
	}else{
		getcwd(zDir, sizeof(zDir));
	}
	zDir[MAXPATHNAME] = '\0';

	sqlite3_snprintf(nPathOut, zPathOut, "%s/%s", zDir, zPath);
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
static void *demoDlOpen(sqlite3_vfs *pVfs, const char *zPath){
	return 0;
}
static void demoDlError(sqlite3_vfs *pVfs, int nByte, char *zErrMsg){
	sqlite3_snprintf(nByte, zErrMsg, "Loadable extensions are not supported");
	zErrMsg[nByte-1] = '\0';
}
static void (*demoDlSym(sqlite3_vfs *pVfs, void *pH, const char *z))(void){
	return 0;
}
static void demoDlClose(sqlite3_vfs *pVfs, void *pHandle){
	return;
}

/*
** Parameter zByte points to a buffer nByte bytes in size. Populate this
** buffer with pseudo-random data.
*/
static int demoRandomness(sqlite3_vfs *pVfs, int nByte, char *zByte){
	return SQLITE_OK;
}

/*
** Sleep for at least nMicro microseconds. Return the (approximate) number 
** of microseconds slept for.
*/
static int demoSleep(sqlite3_vfs *pVfs, int nMicro){
	sleep(nMicro / 1000000);
	usleep(nMicro % 1000000);
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
static int demoCurrentTime(sqlite3_vfs *pVfs, double *pTime){
	time_t t = time(0);
	*pTime = t/86400.0 + 2440587.5; 
	return SQLITE_OK;
}

/*
** This function returns a pointer to the VFS implemented in this file.
** To make the VFS available to SQLite:
**
**   sqlite3_vfs_register(sqlite3_demovfs(), 0);
*/
sqlite3_vfs*
sqlite3_demovfs (void)
{
	static sqlite3_vfs demovfs = {
		1,                            /* iVersion */
		sizeof(AlmanahSQLiteVFS),     /* szOsFile */
		MAXPATHNAME,                  /* mxPathname */
		0,                            /* pNext */
		"almanah",                    /* zName */
		0,                            /* pAppData */
		demoOpen,                     /* xOpen */
		demoDelete,                   /* xDelete */
		demoAccess,                   /* xAccess */
		demoFullPathname,             /* xFullPathname */
		demoDlOpen,                   /* xDlOpen */
		demoDlError,                  /* xDlError */
		demoDlSym,                    /* xDlSym */
		demoDlClose,                  /* xDlClose */
		demoRandomness,               /* xRandomness */
		demoSleep,                    /* xSleep */
		demoCurrentTime,              /* xCurrentTime */
	};

	return &demovfs;
}

int
almanah_vfs_init (void)
{
	return sqlite3_vfs_register (sqlite3_demovfs(), 0);
}
