/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Alvaro Peña 2013 <alvaropg@gmail.com>
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

/* CAUTION: This is just a WIP, it's ugly and in very early stage of development, and isn't completed at the moment */

/* Some thoughts:
 * - To encrypt we need a cipher operation just at this moment, then we can release it (it's right?)
 * - Where put the encryption_key? (the best place would be the EnpmFile.
 * - TODO: Ensure that the CipherOperation elements are usefull.
 */

#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <gpgme.h>
#include <sqlite3.h>

#define MAXPATHNAME 512
#define ENPMVFS_NAME "enpmvfs"

typedef struct {
	EnpmVfs *enpm_vfs; /* FIX: is this required here? Why we need it in CipherOperation? */
	gchar *path_cipher;
	GIOChannel *cipher_io_channel;
	gchar *nonpm_plain;
	gpgme_data_t gpgme_cipher;
	gpgme_data_t gpgme_plain;
	gpgme_ctx_t context;
} CipherOperation;

typedef struct {
	gchar *nonpm_plain;
        gsize read_position;
	gsize write_position;
	gsize buffer_len;
} GpgmeNpmClosure;

typedef struct {
	sqlite3_file base;
	CipherOperation *operation; /* FIX: It's required this here? Perhaps because allow to makes the read */
} EnpmFile;

typedef struct {
	sqlite3_vfs base;
	gchar *encryption_key;
} EnpmVfs;

/* Callback based data buffer functions for GPGME */
ssize_t _gpgme_read_cb (void *handle, void *buffer, size_t size);
ssize_t _gpgme_write_cb (void *handle, const void *buffer, size_t size);
off_t _gpgme_seek_cb (void *handle, off_t offset, int whence);
void _gpgme_release_cb (void *handle);

/* Private functions */
static gboolean _prepare_gpgme (CipherOperation *operation, GError **error);
static gboolean _prepare_cipher (CipherOperation *operation, GError **error);
sqlite3_io_methods* _sqlite_enpm_io (void);
sqlite3_vfs* _sqlite_enpm_vfs (void);

/* SQLite VFS functions */
static int enpm_vfs_open (sqlite3_vfs *vfs, const char *path, sqlite3_file *file, int flags, int *outflags);
static int enpm_vfs_delete (sqlite3_vfs *vfs, const char *path, int dir_sync);
static int enpm_vfs_access (sqlite3_vfs *vfs, const char *path, int flags, int *p_res_out);
static int enpm_vfs_full_pathname (sqlite3_vfs *vfs, const char *path, int n_path_out, char *z_path_out);
static void *enpm_vfs_dl_open (sqlite3_vfs *vfs, const char *path);
static void enpm_vfs_dl_error (sqlite3_vfs *vfs, int byte, char *err_msg);
static void (*enpm_vfs_dl_sym (sqlite3_vfs *vfs, void *pH, const char *z))(void);
static void enpm_vfs_dl_close (sqlite3_vfs *vfs, void *handle);
static int enpm_vfs_randomness (sqlite3_vfs *vfs, int n_byte, char *z_byte);
static int enpm_vfs_sleep (sqlite3_vfs *vfs, int micro);
static int enpm_current_time (sqlite3_vfs *vfs, double *time);

/* SQLite IO functions */
static int enpm_file_close (sqlite3_file *file);
static int enpm_file_read (sqlite3_file *file, void *buffer, int amt, sqlite_int64 offset);
static int enpm_file_write (sqlite3_file *file, const void *buffer, int amt, sqlite_int64 offset);
static int enpm_file_truncate (sqlite3_file *File, sqlite_int64 size);
static int enpm_file_sync (sqlite3_file *file, int flags);
static int enpm_file_file_size (sqlite3_file *file, sqlite_int64 *size);
static int enpm_file_lock(sqlite3_file *file, int lock);
static int enpm_file_unlock(sqlite3_file *file, int lock);
static int enpm_file_check_reserved_lock(sqlite3_file *file, int *res_out);
static int enpm_file_file_control (sqlite3_file *file, int op, void *arg);
static int enpm_file_sector_size (sqlite3_file *file);
static int enpm_file_device_characteristics (sqlite3_file *file);


ssize_t
_gpgme_read_cb (void *handle, void *buffer, size_t size)
{
        GpgmeNpmClosure *npm_closure = (GpgmeNpmClosure *) handle;
	gsize read_size = (gsize) size;

	if (npm_closure->read_position >= npm_closure->buffer_len) {
		return 0; // EOF
	}

	if (npm_closure->read_position + read_size > npm_closure->buffer_len) {
		read_size = npm_closure->buffer_len - npm_closure->read_position;
	}

	memcpy (buffer, npm_closure->npm_plain + npm_closure->read_position, read_size);
	npm_closure->read_position += read_size;

	return (ssize_t) read_size;
}

ssize_t
_gpgme_write_cb (void *handle, const void *buffer, size_t size)
{
	GpgmeNpmClosure *npm_closure = (GpgmeNpmClosure *) handle;

	memcpy (npm_closure->npm_plain + npm_closure->write_position, buffer, size);
	npm_closure->write_position += (gsize) size;

	return size;
}

off_t
_gpgme_seek_cb (void *handle, off_t offset, int whence)
{
	GpgmeNpmClosure *npm_closure = (GpgmeNpmClosure *) handle;

	switch (whence) {
	case SEEK_SET:
		npm_closure->write_position = (gsize) offset;
		npm_closure->read_position = (gsize) offset;
		break;
	case SEEK_CUR:
		npm_closure->write_position += (gsize) offset;
		npm_closure->read_position += (gsize) offset;
		break;
	default:
		g_debug ("No whence: %d", whence);
	}

	return 
}

void
_gpgme_release_cb (void *handle)
{
	/* Nothing to do right now */
}

static gboolean
_prepare_gpgme (CipherOperation *operation, GError **error)
{
	gpgme_error_t error_gpgme;

	/* Check for a minimum GPGME version (bgo#599598) */
	if (gpgme_check_version (MIN_GPGME_VERSION) == NULL) {
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_BAD_VERSION,
			     _("GPGME is not at least version %s"),
			     MIN_GPGME_VERSION);
		return FALSE;
	}

	/* Check OpenPGP's supported */
	error_gpgme = gpgme_engine_check_version (GPGME_PROTOCOL_OpenPGP);
	if (error_gpgme != GPG_ERR_NO_ERROR) {
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_UNSUPPORTED,
			     _("GPGME doesn't support OpenPGP: %s"),
			     gpgme_strerror (error_gpgme));
		return FALSE;
	}

	/* Set up for the operation */
	error_gpgme = gpgme_new (&(operation->context));
	if (error_gpgme != GPG_ERR_NO_ERROR) {
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_CREATING_CONTEXT,
			     _("Error creating cipher context: %s"),
			     gpgme_strerror (error_gpgme));
		return FALSE;
	}

	gpgme_set_protocol (operation->context, GPGME_PROTOCOL_OpenPGP);
	gpgme_set_armor (operation->context, TRUE);
	gpgme_set_textmode (operation->context, FALSE);

	return TRUE;
}

static gboolean
_prepare_cipher (CipherOperation *operation, GError **error)
{
	gpgme_error_t error_gpgme;
	gsize cipher_size;
	GpgmeNpmClosure *npm_closure;
	static gpgme_data_cbs_t cbs = {
		_gpgme_read_cb,     /* gpgme_data_read_cb_t read */
		_gpgme_write_cb,    /* gpgme_data_write_cb_t write */
		_gpgme_seek_cb,     /* gpgme_data_seek_cb_t seek */
		_gpgme_release_cb,  /* gpgme_data_release_cb_t release */
	};

	/* Open the encrypted file */
	operation->cipher_io_channel = g_io_channel_new_file (operation->path_cipher, encrypting ? "w" : "r", &io_error);
	if (operation->cipher_io_channel == NULL) {
		g_propagate_error (error, io_error);
		return FALSE;
	}

	/* Pass it to GPGME */
	error_gpgme = gpgme_data_new_from_fd (&(operation->gpgme_cipher), g_io_channel_unix_get_fd (operation->cipher_io_channel));
	if (error_gpgme != GPG_ERR_NO_ERROR) {
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_OPENING_FILE,
			     _("Error opening encrypted database file \"%s\": %s"),
			     operation->path_cipher, gpgme_strerror (error_gpgme));
		return FALSE;
	}

	/* Create a non-pageable memory */
	cipher_size = g_io_channel_get_buffer_size (operation->cipher_io_channel);
	operation->nonpm_plain = (gchar *) gnome_keyring_memory_try_alloc (cipher_size);
	if (operation->nonpm_plain == NULL) {
		g_set_error (error, 0, 0,
			     _("Error allocating non-pageable memory"));
		return FALSE;
	}

	/* Pass the non-pageable memory to GPGME as a Callback Base Data Buffer, 
	 * see: http://www.gnupg.org/documentation/manuals/gpgme/Callback-Based-Data-Buffers.html
	 */
	npm_closure = g_new0 (GpgmeNpmClosure, 1);
	npm_closure->read_position = 0;
	npm_closure->write_position = 0;
	npm_closure->nonpm_plain = operation->nonpm_plain;
	npm_closure->buffer_len = chiper_size;
	error_gpgme = gpgme_data_new_from_cbs (&(operation->gpgme_plain), cbs, (void *) npm_closure);
	if (error_gpgme != GPG_ERR_NO_ERROR) {
		g_set_error (error, 0, 0,
			     _("Error creating Callback base data buffer: %s"),
			     gpgme_strerror (error_gpgme));
		return FALSE;
	}
}

static void
_cipher_operation_free (CipherOperation *operation)
{
	gnome_keyring_memory_free (operation->nonpm_plain);
}

sqlite3_io_methods *
_sqlite_enpm_io (void)
{
	static sqlite3_io_methods enpm_io_memthods = {
		1,                               /* iVersion */
		enpm_file_close,                 /* xClose */
		enpm_file_read,                  /* xRead */
		enpm_file_write,                 /* xWrite */
		enpm_file_truncate,              /* xTruncate */
		enpm_file_sync,                  /* xSync */
		enpm_file_file_size,             /* xFileSize */
		enpm_file_lock,                  /* xLock */
		enpm_file_unlock,                /* xUnlock */
		enpm_file_check_reserved_lock,   /* xCheckReservedLock */
		enpm_file_file_control,          /* xFileControl */
		enpm_file_sector_size,           /* xSectorSize */
		enpm_file_device_characteristics /* xDeviceCharacteristics */
	};

	return &enpm_io_methods;
}

sqlite3_vfs *
_sqlite_enpm_vfs (void)
{
	static sqlite3_vfs enpm_vfs = {
		1,                      /* iVersion */
		sizeof(EnpmFile),       /* szOsFile */
		MAXPATHNAME,            /* mxPathname */
		0,                      /* pNext */
		ENPMVFS_NAME,           /* zName */
		0,                      /* pAppData */
		enpm_vfs_open,          /* xOpen */
		enpm_vfs_delete,        /* xDelete */
		enpm_vfs_access,        /* xAccess */
		enpm_vfs_full_pathname, /* xFullPathname */
		enpm_vfs_dl_open,       /* xDlOpen */
		enpm_vfs_dl_error,      /* xDlError */
		enpm_vfs_dl_sym,        /* xDlSym */
		enpm_vfs_dl_close,      /* xDlClose */
		enpm_vfs_randomness,    /* xRandomness */
		enpm_vfs_sleep,         /* xSleep */
		enpm_vfs_current_time   /* xCurrentTime */
	};

	return &enpm_vfs;
}

/*
** Open a file handle.
*/
static int
enpm_vfs_open (sqlite3_vfs *vfs, const char *path, sqlite3_file *file, int flags, int *outflags)
{
	EnpmVfs enpm_vfs = (EnpmVfs *) vfs;
	EnpmFile mem_file = (EnpmFile *) file;
	int o_flags = 0;
	CipherOperation *operation;

	if (flags & SQLITE_OPEN_EXCLUSIVE) o_flags |= O_EXCL;
	if (flags & SQLITE_OPEN_CREATE)    o_flags |= O_CREAT;
	if (flags & SQLITE_OPEN_READONLY)  o_flags |= O_RDONLY;
	if (flags & SQLITE_OPEN_READWRITE) o_flags |= O_RDWR;

	operation = g_new0 (CipherOperation, 1);
	operation->enpm_vfs = enpm_vfs;
	operation->path_cipher = path;

	/* Set up */
	if (_prepare_gpme (operation, &preparation_error) != TRUE
	    || _prepare_chiper (operation, &preparation_error) != TRUE) {
		cipher_operation_free (operation);
		return FALSE;
	}

	/* Decrypt and verify */
	error_gpgme = gpgme_op_decrypt_verify (operation->context, operation->gpgme_cipher, operation->gpgme_plain);
		if (error_gpgme != GPG_ERR_NO_ERROR) {
		cipher_operation_free (operation);
		g_set_error (error, ALMANAH_STORAGE_MANAGER_ERROR, ALMANAH_STORAGE_MANAGER_ERROR_DECRYPTING,
		             _("Error decrypting database: %s"),
		             gpgme_strerror (error_gpgme));
		return FALSE;
	}

	mem_file = g_new0 (EnpmFile, 1);
	mem_file->base.pMethods = sqlite_enpm_io ();
	mem_file->flags = flags;
	mem_file->cipher_operation = operation;

	return SQLITE_OK;
}

/*
** Delete the file identified by argument path. If the dir_sync parameter
** is non-zero, then ensure the file-system modification to delete the
** file has been synced to disk before returning.
*/
static int
enpm_vfs_delete (sqlite3_vfs *vfs, const char *path, int dir_sync)
{
	int rc;

	rc = g_unlink (path);

	/* The file dosn't exists */
	if (rc != 0 && errno == ENOENT)
		return SQLITE_OK;

	if (rc == 0 && dir_sync) {
		int dfd;
		gchar *dir_path;

		/* Figure out the directory name from the path of the file deleted. */
		dir_path = g_path_get_dirname (path);

		/* Open a file-descriptor on the directory. Sync. Close. */
		dfd = open (dir_path, O_RDONLY, 0);
		if (dfd < 0) {
			rc = -1;
		} else {
			rc = fsync (dfd);
			close (dfd);
		}
	}

	if (rc == 0)
		return SQLITE_OK;
	else
		return SQLITE_IOERR_DELETE;
}

/*
** Query the file-system to see if the named file exists, is readable or
** is both readable and writable.
*/
static int
enpm_vfs_access (sqlite3_vfs *vfs, const char *path, int flags, int *p_res_out)
{
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
enpm_vfs_full_pathname (sqlite3_vfs *vfs, const char *path, int n_path_out, char *z_path_out)
{
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
static void *
enpm_vfs_dl_open (sqlite3_vfs *vfs, const char *path)
{
	return 0;
}

static void
enpm_vfs_dl_error (sqlite3_vfs *vfs, int byte, char *err_msg)
{
	return;
}

static void
(*enpm_vfs_dl_sym (sqlite3_vfs *vfs, void *pH, const char *z))(void)
{
	return 0;
}

static void
enpm_vfs_dl_close (sqlite3_vfs *vfs, void *handle)
{
	return;
}

/*
** Parameter zByte points to a buffer nByte bytes in size. Populate this
** buffer with pseudo-random data.
*/
static int
enpm_vfs_randomness (sqlite3_vfs *vfs, int n_byte, char *z_byte)
{
	return SQLITE_OK;
}

/*
** Sleep for at least nMicro microseconds. Return the (approximate) number 
** of microseconds slept for.
*/
static int
enpm_vfs_sleep (sqlite3_vfs *vfs, int micro)
{
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
static int
enpm_current_time (sqlite3_vfs *vfs, double *time)
{
	time_t t = time(0);
	*pTime = t/86400.0 + 2440587.5; 
	return SQLITE_OK;
}

/*
** Close a file.
*/
static int
enpm_file_close (sqlite3_file *file)
{
	EnpmFile *enpm_file = (EnpmFile *) file;

	/* Encrypt the database and save to file */
	
}

/*
** Read data from a file.
*/
static int
enpm_file_read (sqlite3_file *file, void *buffer, int amt, sqlite_int64 offset)
{
}

/*
** Write data to a crash-file.
*/
static int
enpm_file_write (sqlite3_file *file, const void *buffer, int amt, sqlite_int64 offset)
{
}

/*
** Truncate a file. This is a no-op for this VFS (see header comments at
** the top of the file).
*/
static int
enpm_file_truncate (sqlite3_file *File, sqlite_int64 size)
{
}

/*
** Sync the contents of the file to the persistent media.
*/
static int
enpm_file_sync (sqlite3_file *file, int flags)
{
}

/*
** Write the size of the file in bytes to *pSize.
*/
static int
enpm_file_file_size (sqlite3_file *file, sqlite_int64 *size)
{
}

/*
** Locking functions. The xLock() and xUnlock() methods are both no-ops.
** The xCheckReservedLock() always indicates that no other process holds
** a reserved lock on the database file. This ensures that if a hot-journal
** file is found in the file-system it is rolled back.
*/
static int
enpm_file_lock(sqlite3_file *file, int lock)
{
}

static int
enpm_file_unlock(sqlite3_file *file, int lock)
{
}

static int
enpm_file_check_reserved_lock(sqlite3_file *file, int *res_out)
{
}

/*
** No xFileControl() verbs are implemented by this VFS.
*/
static int
enpm_file_file_control (sqlite3_file *file, int op, void *arg)
{
}

/*
** The xSectorSize() and xDeviceCharacteristics() methods. These two
** may return special values allowing SQLite to optimize file-system 
** access to some extent. But it is also safe to simply return 0.
*/
static int
enpm_file_sector_size (sqlite3_file *file)
{
}

static int
enpm_file_device_characteristics (sqlite3_file *file)
{
}


/*
 * This init functios is required to pass the encryption key
 * and to register this VFS plugin
 */
int
enpmvfs_init (gchar *encryption_key)
{
	EnpmVfs *enpmvfs;

	g_new0 (EnpmVfs, 1);
	enpmvfs->parent = sqlite3_vfs_find (0);
	enpmvfs->encryption_key = g_strdup (encryption_key);

	return sqlite3_vfs_register ((sqlite3_vfs *) enpmvfs, 0);
}
