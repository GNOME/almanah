/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2010 <philip@tecnocode.co.uk>
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
#include <gtk/gtk.h>

#include "import-operation.h"
#include "entry.h"
#include "storage-manager.h"

typedef gboolean (*ImportFunc) (AlmanahImportOperation *self, GFile *source, AlmanahImportProgressCallback callback, gpointer user_data,
                                GCancellable *cancellable, GError **error);

typedef struct {
	const gchar *name; /* translatable */
	const gchar *description; /* translatable */
	GtkFileChooserAction action;
	ImportFunc import_func;
} ImportModeDetails;

static gboolean import_text_files (AlmanahImportOperation *self, GFile *source, AlmanahImportProgressCallback callback, gpointer user_data,
                                   GCancellable *cancellable, GError **error);
static gboolean import_database (AlmanahImportOperation *self, GFile *source, AlmanahImportProgressCallback callback, gpointer user_data,
                                 GCancellable *cancellable, GError **error);

static const ImportModeDetails import_modes[] = {
	{ N_("Text Files"),
	  N_("Select a _folder containing text files, one per entry, with names in the format 'yyyy-mm-dd', and no extension. "
	     "Any and all such files will be imported."),
	  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
	  import_text_files },
	{ N_("Database"),
	  N_("Select a database _file created by Almanah Diary to import."),
	  GTK_FILE_CHOOSER_ACTION_OPEN,
	  import_database }
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void almanah_import_operation_dispose (GObject *object);

typedef struct {
	gint current_mode; /* index into import_modes */
	GFile *source;
	AlmanahStorageManager *storage_manager;
} AlmanahImportOperationPrivate;

struct _AlmanahImportOperation {
	GObject parent;
};

enum {
	PROP_STORAGE_MANAGER = 1,
};

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahImportOperation, almanah_import_operation, G_TYPE_OBJECT)

static void
almanah_import_operation_class_init (AlmanahImportOperationClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = get_property;
	gobject_class->set_property = set_property;
	gobject_class->dispose = almanah_import_operation_dispose;

	g_object_class_install_property (gobject_class, PROP_STORAGE_MANAGER,
	                                 g_param_spec_object ("storage-manager",
	                                                      "Storage manager", "The destination storage manager for the import operation.",
	                                                      ALMANAH_TYPE_STORAGE_MANAGER,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_import_operation_init (AlmanahImportOperation *self)
{
	AlmanahImportOperationPrivate *priv = almanah_import_operation_get_instance_private (self);

	priv->current_mode = -1; /* no mode selected */
}

static void
almanah_import_operation_dispose (GObject *object)
{
	AlmanahImportOperationPrivate *priv = almanah_import_operation_get_instance_private (ALMANAH_IMPORT_OPERATION (object));

	if (priv->source != NULL)
		g_object_unref (priv->source);
	priv->source = NULL;

	if (priv->storage_manager != NULL)
		g_object_unref (priv->storage_manager);
	priv->storage_manager = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_import_operation_parent_class)->dispose (object);
}

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahImportOperationPrivate *priv = almanah_import_operation_get_instance_private (ALMANAH_IMPORT_OPERATION (object));

	switch (property_id) {
		case PROP_STORAGE_MANAGER:
			g_value_set_object (value, priv->storage_manager);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahImportOperation *self = ALMANAH_IMPORT_OPERATION (object);
	AlmanahImportOperationPrivate *priv = almanah_import_operation_get_instance_private (self);

	switch (property_id) {
		case PROP_STORAGE_MANAGER:
			priv->storage_manager = g_value_dup_object (value);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

AlmanahImportOperation *
almanah_import_operation_new (AlmanahImportOperationType type_id, GFile *source, AlmanahStorageManager *dest_storage_manager)
{
	AlmanahImportOperation *import_operation = g_object_new (ALMANAH_TYPE_IMPORT_OPERATION, "storage-manager", dest_storage_manager, NULL);
	AlmanahImportOperationPrivate *priv = almanah_import_operation_get_instance_private (import_operation);

	priv->current_mode = type_id;
	priv->source = g_object_ref (source);

	return import_operation;
}

typedef struct {
	AlmanahImportProgressCallback callback;
	gpointer user_data;
	GDate *date;
	AlmanahImportStatus status;
	gchar *message;
} ProgressData;

static gboolean
progress_idle_callback_cb (ProgressData *data)
{
	g_assert (data->callback != NULL);
	data->callback (data->date, data->status, data->message, data->user_data);

	/* Free the data */
	g_free (data->message);
	g_date_free (data->date);
	g_slice_free (ProgressData, data);

	return FALSE;
}

static void
progress_idle_callback (AlmanahImportProgressCallback callback, gpointer user_data, const GDate *date, AlmanahImportStatus status,
                        const gchar *message)
{
	GSource *source;
	ProgressData *data;

	data = g_slice_new (ProgressData);
	data->callback = callback;
	data->user_data = user_data;
	data->date = g_date_new_dmy (g_date_get_day (date), g_date_get_month (date), g_date_get_year (date));
	data->status = status;
	data->message = g_strdup (message);

	/* We can't just use g_idle_add() here, since GTask uses default priority, so the finished callback will skip any outstanding
	 * progress callbacks in the main loop's priority queue, causing Bad Things to happen. We need to guarantee that no more progress callbacks
	 * will occur after the finished callback has been called; this is one hacky way of achieving that. */
	source = g_idle_source_new ();
	g_source_set_priority (source, G_PRIORITY_DEFAULT);

	g_source_set_callback (source, (GSourceFunc) progress_idle_callback_cb, data, NULL);
	g_source_attach (source, NULL);

	g_source_unref (source);
}

/**
 * set_entry:
 * @self: an #AlmanahImportOperation
 * @imported_entry: an #AlmanahEntry created for an entry being imported
 * @import_source: a string representing the source of the imported entry
 * @message: return location for an error or informational message, or %NULL; free with g_free()
 *
 * Stores the given entry in the database, merging it with any existing one where appropriate.
 *
 * @import_source should represent where the imported entry came from, so it could be the filename of a text file which held the entry, or of
 * the entry's previous database.
 *
 * If @message is non-%NULL, error or informational messages can be allocated and returned in it. They will be returned irrespective of the return
 * value of the function, as some errors can be overcome to result in a successful import. The message should be freed with g_free() by the caller.
 *
 * Return value: %ALMANAH_IMPORT_STATUS_MERGED if the entry was merged with an existing one, %ALMANAH_IMPORT_STATUS_IMPORTED if it was imported
 * without merging, %ALMANAH_IMPORT_STATUS_FAILED otherwise
 **/
static AlmanahImportStatus
set_entry (AlmanahImportOperation *self, AlmanahEntry *imported_entry, const gchar *import_source, gchar **message)
{
	AlmanahImportOperationPrivate *priv = almanah_import_operation_get_instance_private (self);
	GDate entry_date, existing_last_edited, imported_last_edited;
	AlmanahEntry *existing_entry;
	GtkTextBuffer *existing_buffer, *imported_buffer;
	GtkTextIter existing_start, existing_end, imported_start, imported_end;
	gchar *header_string;
	GError *error = NULL;

	/* Check to see if there's a conflict first */
	almanah_entry_get_date (imported_entry, &entry_date);
	existing_entry = almanah_storage_manager_get_entry (priv->storage_manager, &entry_date);

	if (existing_entry == NULL) {
		/* Add the entry to the proper database and return, ignoring failure */
		almanah_storage_manager_set_entry (priv->storage_manager, imported_entry);

		return ALMANAH_IMPORT_STATUS_IMPORTED;
	}

	/* Load both entries into buffers */
	imported_buffer = gtk_text_buffer_new (NULL);
	if (almanah_entry_get_content (imported_entry, imported_buffer, TRUE, &error) == FALSE) {
		if (message != NULL)
			*message = g_strdup_printf (_("Error deserializing imported entry into buffer: %s"), (error != NULL) ? error->message : NULL);

		g_error_free (error);
		g_object_unref (imported_buffer);
		g_object_unref (existing_entry);

		return ALMANAH_IMPORT_STATUS_FAILED;
	}

	/* The two buffers have to use the same tag table for gtk_text_buffer_insert_range() to work */
	existing_buffer = gtk_text_buffer_new (gtk_text_buffer_get_tag_table (imported_buffer));
	if (almanah_entry_get_content (existing_entry, existing_buffer, FALSE, &error) == FALSE) {
		/* Deserialising the existing entry failed; use the imported entry instead */
		almanah_storage_manager_set_entry (priv->storage_manager, imported_entry);

		if (message != NULL) {
			*message = g_strdup_printf (_("Error deserializing existing entry into buffer; overwriting with imported entry: %s"),
			                            (error != NULL) ? error->message : NULL);
		}

		g_error_free (error);
		g_object_unref (imported_buffer);
		g_object_unref (existing_buffer);
		g_object_unref (existing_entry);

		return ALMANAH_IMPORT_STATUS_IMPORTED;
	}

	/* Get the bounds for later use */
	gtk_text_buffer_get_bounds (existing_buffer, &existing_start, &existing_end);
	gtk_text_buffer_get_bounds (imported_buffer, &imported_start, &imported_end);

	/* Compare the two buffers --- if they're identical, leave the current entry alone and mark the entries as merged.
	 * Compare the character counts first so that the comparison is less expensive in the case they aren't identical .*/
	if (gtk_text_buffer_get_char_count (existing_buffer) == gtk_text_buffer_get_char_count (imported_buffer)) {
		gchar *existing_text, *imported_text;

		existing_text = gtk_text_buffer_get_text (existing_buffer, &existing_start, &existing_end, FALSE);
		imported_text = gtk_text_buffer_get_text (imported_buffer, &imported_start, &imported_end, FALSE);

		/* If they're the same, no modifications are required */
		if (strcmp (existing_text, imported_text) == 0) {
			g_free (existing_text);
			g_free (imported_text);
			g_object_unref (imported_buffer);
			g_object_unref (existing_buffer);
			g_object_unref (existing_entry);
			return ALMANAH_IMPORT_STATUS_MERGED;
		}

		g_free (existing_text);
		g_free (imported_text);
	}

	/* Append some header text for the imported entry */
	/* Translators: This text is appended to an existing entry when an entry is being imported to the same date.
	 * The imported entry is appended to this text. */
	header_string = g_strdup_printf (_("\n\nEntry imported from \"%s\":\n\n"), import_source);
	gtk_text_buffer_insert (existing_buffer, &existing_end, header_string, -1);
	g_free (header_string);

	/* Append the imported entry to the end of the existing one */
	gtk_text_buffer_insert_range (existing_buffer, &existing_end, &imported_start, &imported_end);
	g_object_unref (imported_buffer);

	/* Store the buffer back in the existing entry and save the entry */
	almanah_entry_set_content (existing_entry, existing_buffer);
	g_object_unref (existing_buffer);

	/* Update the last-edited time for the merged entry to be the more recent of the last-edited times for the existing and imported entries */
	almanah_entry_get_last_edited (existing_entry, &existing_last_edited);
	almanah_entry_get_last_edited (imported_entry, &imported_last_edited);

	if (g_date_valid (&imported_last_edited) == TRUE &&
	    (g_date_valid (&existing_last_edited) == FALSE || g_date_compare (&existing_last_edited, &imported_last_edited) < 0)) {
		almanah_entry_set_last_edited (existing_entry, &imported_last_edited);
	}

	almanah_storage_manager_set_entry (priv->storage_manager, existing_entry);
	g_object_unref (existing_entry);

	return ALMANAH_IMPORT_STATUS_MERGED;
}

static gboolean
import_text_files (AlmanahImportOperation *self, GFile *source, AlmanahImportProgressCallback progress_callback, gpointer progress_user_data,
                   GCancellable *cancellable, GError **error)
{
	gboolean retval = FALSE;
	GFileInfo *file_info;
	GFileEnumerator *enumerator;
	GtkTextBuffer *buffer;
	GError *child_error = NULL;

	enumerator = g_file_enumerate_children (source, "standard::name,standard::display-name,standard::is-hidden,time::modified",
	                                        G_FILE_QUERY_INFO_NONE, NULL, error);
	if (enumerator == NULL)
		return FALSE;

	/* Build a text buffer to use when setting all the entries */
	buffer = gtk_text_buffer_new (NULL);

	/* Enumerate all the children of the folder */
	while ((file_info = g_file_enumerator_next_file (enumerator, NULL, &child_error)) != NULL) {
		AlmanahEntry *entry;
		GDate parsed_date, last_edited;
		g_autoptr(GDateTime) modification_date_time = NULL;
		GFile *file;
		gchar *contents, *message = NULL;
		gsize length;
		AlmanahImportStatus status;
		const gchar *file_name = g_file_info_get_name (file_info);
		const gchar *display_name = g_file_info_get_display_name (file_info);

		/* Skip the file if it's hidden */
		if (g_file_info_get_is_hidden (file_info) == TRUE || file_name[strlen (file_name) - 1] == '~') {
			g_object_unref (file_info);
			continue;
		}

		/* Heuristically parse the date, though we recommend using the format: yyyy-mm-dd */
		g_date_set_parse (&parsed_date, file_name);
		if (g_date_valid (&parsed_date) == FALSE) {
			g_object_unref (file_info);
			continue;
		}

		/* Get the file */
		file = g_file_get_child (source, file_name);
		g_assert (file != NULL);

		/* Load the content */
		if (g_file_load_contents (file, NULL, &contents, &length, NULL, &child_error) == FALSE) {
			g_object_unref (file);
			g_object_unref (file_info);
			break; /* let the error get handled by the code just after the loop */
		}
		g_object_unref (file);

		/* Create the relevant entry */
		entry = almanah_entry_new (&parsed_date);

		/* Set the content on the entry */
		gtk_text_buffer_set_text (buffer, contents, length);
		almanah_entry_set_content (entry, buffer);
		g_free (contents);

		/* Set the entry's last-edited date */
		modification_date_time = g_file_info_get_modification_date_time (file_info);
		if (modification_date_time != NULL) {
			g_date_set_time_t (&last_edited, g_date_time_to_unix (modification_date_time));
			almanah_entry_set_last_edited (entry, &last_edited);
		} else {
			almanah_entry_set_last_edited (entry, &parsed_date);
		}

		/* Store the entry */
		status = set_entry (self, entry, display_name, &message);
		progress_idle_callback (progress_callback, progress_user_data, &parsed_date, status, message);
		g_free (message);

		g_object_unref (entry);
		g_object_unref (file_info);

		/* Check for cancellation */
		if (cancellable != NULL && g_cancellable_set_error_if_cancelled (cancellable, &child_error) == TRUE)
			break;
	}

	/* Check if the loop was broken due to an error */
	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		goto finish;
	}

	/* Success! */
	retval = TRUE;

finish:
	g_object_unref (enumerator);
	g_object_unref (buffer);

	return retval;
}

static gboolean
import_database (AlmanahImportOperation *self, GFile *source, AlmanahImportProgressCallback progress_callback, gpointer progress_user_data,
                 GCancellable *cancellable, GError **error)
{
	g_autoptr (GFileInfo) file_info = NULL;
	gchar *path;
	const gchar *display_name;
	AlmanahEntry *entry;
	AlmanahStorageManager *database;
	AlmanahStorageManagerIter iter;
	gboolean success = FALSE;
	GSettings *settings;

	/* Get the display name for use with set_entry(), below */
	file_info = g_file_query_info (source, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, G_FILE_QUERY_INFO_NONE, cancellable, error);
	if (file_info == NULL)
		return FALSE;

	display_name = g_file_info_get_display_name (file_info);

	/* Open the database */
	path = g_file_get_path (source);
	settings = g_settings_new ("org.gnome.almanah");
	database = almanah_storage_manager_new (path, settings);
	g_object_unref (settings);
	g_free (path);

	/* Connect to the database */
	if (almanah_storage_manager_connect (database, error) == FALSE) {
		g_object_unref (database);
		return FALSE;
	}

	/* Iterate through every entry */
	almanah_storage_manager_iter_init (&iter);
	while ((entry = almanah_storage_manager_get_entries (database, &iter)) != NULL) {
		GDate date;
		gchar *message = NULL;
		AlmanahImportStatus status;

		almanah_entry_get_date (entry, &date);

		status = set_entry (self, entry, display_name, &message);
		progress_idle_callback (progress_callback, progress_user_data, &date, status, message);
		g_free (message);

		g_object_unref (entry);

		/* Check for cancellation */
		if (cancellable != NULL && g_cancellable_set_error_if_cancelled (cancellable, error) == TRUE)
			goto finish;
	}

	/* Success! */
	success = TRUE;

finish:
	almanah_storage_manager_disconnect (database, NULL);
	g_object_unref (database);

	return success;
}

typedef struct {
	AlmanahImportProgressCallback progress_callback;
	gpointer progress_user_data;
} ImportData;

static void
import_data_free (ImportData *data)
{
	g_slice_free (ImportData, data);
}

static void
import_thread (GTask *task, AlmanahImportOperation *operation, gpointer task_data, GCancellable *cancellable)
{
	GError *error = NULL;
	ImportData *data = (ImportData *) task_data;

	/* Check to see if the operation's been cancelled already */
	if (g_cancellable_set_error_if_cancelled (cancellable, &error) == TRUE) {
		g_task_return_error (task, error);
		return;
	}

	AlmanahImportOperationPrivate *priv = almanah_import_operation_get_instance_private (operation);

	/* Import and return */
	if (import_modes[priv->current_mode].import_func (operation, priv->source, data->progress_callback,
	    data->progress_user_data, cancellable, &error) == FALSE) {
		g_task_return_error (task, error);
	}
}

void
almanah_import_operation_run (AlmanahImportOperation *self, GCancellable *cancellable, AlmanahImportProgressCallback progress_callback,
                              gpointer progress_user_data, GAsyncReadyCallback callback, gpointer user_data)
{
	g_autoptr (GTask) task = NULL;
	ImportData *data;

	g_return_if_fail (ALMANAH_IS_IMPORT_OPERATION (self));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	data = g_slice_new (ImportData);
	data->progress_callback = progress_callback;
	data->progress_user_data = progress_user_data;

	task = g_task_new (G_OBJECT (self), cancellable, callback, user_data);
	g_task_set_source_tag (task, almanah_import_operation_run);
	g_task_set_task_data (task, data, (GDestroyNotify) import_data_free);
	g_task_run_in_thread (task, (GTaskThreadFunc) import_thread);
}

gboolean
almanah_import_operation_finish (AlmanahImportOperation *self, GAsyncResult *async_result, GError **error)
{
	GTask *task = G_TASK (async_result);

	g_return_val_if_fail (ALMANAH_IS_IMPORT_OPERATION (self), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (async_result), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	g_warn_if_fail (g_task_get_source_tag (task) == almanah_import_operation_run);

	if (g_async_result_legacy_propagate_error (G_ASYNC_RESULT (task), error) == TRUE)
		return FALSE;

	return TRUE;
}

void
almanah_import_operation_populate_model (GtkListStore *store, guint type_id_column, guint name_column, guint description_column, guint action_column)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (import_modes); i++) {
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
		                    type_id_column, i,
		                    name_column, _(import_modes[i].name),
		                    description_column, _(import_modes[i].description),
		                    action_column, import_modes[i].action,
		                    -1);
	}
}
