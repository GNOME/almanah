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
#include <errno.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "export-operation.h"
#include "entry.h"
#include "storage-manager.h"

typedef gboolean (*ExportFunc) (AlmanahExportOperation *self, GFile *destination, AlmanahExportProgressCallback progress_callback,
                                gpointer progress_user_data, GCancellable *cancellable, GError **error);

typedef struct {
	const gchar *name; /* translatable */
	const gchar *description; /* translatable */
	GtkFileChooserAction action;
	ExportFunc export_func;
} ExportModeDetails;

static gboolean export_text_files (AlmanahExportOperation *self, GFile *destination, AlmanahExportProgressCallback progress_callback,
                                   gpointer progress_user_data, GCancellable *cancellable, GError **error);
static gboolean export_database (AlmanahExportOperation *self, GFile *destination, AlmanahExportProgressCallback progress_callback,
                                 gpointer progress_user_data, GCancellable *cancellable, GError **error);

static const ExportModeDetails export_modes[] = {
	{ N_("Text Files"),
	  N_("Select a _folder to export the entries to as text files, one per entry, with names in the format 'yyyy-mm-dd', and no extension. "
	     "All entries will be exported, unencrypted in plain text format."),
	  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
	  export_text_files },
	{ N_("Database"),
	  N_("Select a _filename for a complete copy of the unencrypted Almanah Diary database to be given."),
	  GTK_FILE_CHOOSER_ACTION_SAVE,
	  export_database }
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void almanah_export_operation_dispose (GObject *object);

typedef struct {
	gint current_mode; /* index into export_modes */
	AlmanahStorageManager *storage_manager;
	GFile *destination;
} AlmanahExportOperationPrivate;

struct _AlmanahExportOperation {
	GObject parent;
};

enum {
	PROP_STORAGE_MANAGER = 1,
};

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahExportOperation, almanah_export_operation, G_TYPE_OBJECT)

static void
almanah_export_operation_class_init (AlmanahExportOperationClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = get_property;
	gobject_class->set_property = set_property;
	gobject_class->dispose = almanah_export_operation_dispose;

	g_object_class_install_property (gobject_class, PROP_STORAGE_MANAGER,
	                                 g_param_spec_object ("storage-manager",
	                                                      "Storage manager", "The source storage manager for the export operation.",
	                                                      ALMANAH_TYPE_STORAGE_MANAGER,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_export_operation_init (AlmanahExportOperation *self)
{
	AlmanahExportOperationPrivate *priv = almanah_export_operation_get_instance_private (self);

	priv->current_mode = -1; /* no mode selected */
}

static void
almanah_export_operation_dispose (GObject *object)
{
	AlmanahExportOperationPrivate *priv = almanah_export_operation_get_instance_private (ALMANAH_EXPORT_OPERATION (object));

	if (priv->storage_manager != NULL)
		g_object_unref (priv->storage_manager);
	priv->storage_manager = NULL;

	if (priv->destination != NULL)
		g_object_unref (priv->destination);
	priv->destination = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_export_operation_parent_class)->dispose (object);
}

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahExportOperationPrivate *priv = almanah_export_operation_get_instance_private (ALMANAH_EXPORT_OPERATION (object));

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
	AlmanahExportOperation *self = ALMANAH_EXPORT_OPERATION (object);
	AlmanahExportOperationPrivate *priv = almanah_export_operation_get_instance_private (self);

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

AlmanahExportOperation *
almanah_export_operation_new (AlmanahExportOperationType type_id, AlmanahStorageManager *source_storage_manager, GFile *destination)
{
	AlmanahExportOperation *export_operation = g_object_new (ALMANAH_TYPE_EXPORT_OPERATION, "storage-manager", source_storage_manager, NULL);
	AlmanahExportOperationPrivate *priv = almanah_export_operation_get_instance_private (export_operation);

	priv->current_mode = type_id;
	priv->destination = g_object_ref (destination);

	return export_operation;
}

typedef struct {
	AlmanahExportProgressCallback callback;
	gpointer user_data;
	GDate *date;
} ProgressData;

static gboolean
progress_idle_callback_cb (ProgressData *data)
{
	g_assert (data->callback != NULL);
	data->callback (data->date, data->user_data);

	/* Free the data */
	g_date_free (data->date);
	g_slice_free (ProgressData, data);

	return FALSE;
}

static void
progress_idle_callback (AlmanahExportProgressCallback callback, gpointer user_data, const GDate *date)
{
	GSource *source;
	ProgressData *data;

	data = g_slice_new (ProgressData);
	data->callback = callback;
	data->user_data = user_data;
	data->date = g_date_new_dmy (g_date_get_day (date), g_date_get_month (date), g_date_get_year (date));

	/* We can't just use g_idle_add() here, since GTask uses default priority, so the finished callback will skip any outstanding
	 * progress callbacks in the main loop's priority queue, causing Bad Things to happen. We need to guarantee that no more progress callbacks
	 * will occur after the finished callback has been called; this is one hacky way of achieving that. */
	source = g_idle_source_new ();
	g_source_set_priority (source, G_PRIORITY_DEFAULT);

	g_source_set_callback (source, (GSourceFunc) progress_idle_callback_cb, data, NULL);
	g_source_attach (source, NULL);

	g_source_unref (source);
}

static gboolean
export_text_files (AlmanahExportOperation *self, GFile *destination, AlmanahExportProgressCallback progress_callback, gpointer progress_user_data,
                   GCancellable *cancellable, GError **error)
{
	AlmanahStorageManagerIter iter;
	AlmanahEntry *entry;
	GtkTextBuffer *buffer;
	gboolean success = FALSE;
	GError *child_error = NULL;
	AlmanahExportOperationPrivate *priv = almanah_export_operation_get_instance_private (self);

	/* Build a text buffer to use when getting all the entries */
	buffer = gtk_text_buffer_new (NULL);

	/* Iterate through the entries */
	almanah_storage_manager_iter_init (&iter);
	while ((entry = almanah_storage_manager_get_entries (priv->storage_manager, &iter)) != NULL) {
		GDate date;
		gchar *filename, *content, *path;
		GFile *file;
		GtkTextIter start_iter, end_iter;

		/* Get the filename */
		almanah_entry_get_date (entry, &date);
		filename = g_strdup_printf ("%04u-%02u-%02u", g_date_get_year (&date), g_date_get_month (&date), g_date_get_day (&date));
		file = g_file_get_child (destination, filename);
		g_free (filename);

		/* Get the entry contents */
		if (almanah_entry_get_content (entry, buffer, TRUE, &child_error) == FALSE) {
			/* Error */
			g_object_unref (file);
			g_object_unref (entry);
			break;
		}

		g_object_unref (entry);

		gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);
		content = gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);

		/* Create the file */
		if (g_file_replace_contents (file, content, strlen (content), NULL, FALSE,
		                             G_FILE_CREATE_PRIVATE | G_FILE_CREATE_REPLACE_DESTINATION, NULL, NULL, &child_error) == FALSE) {
			/* Error */
			g_object_unref (file);
			g_free (content);
			break;
		}

		g_free (content);

		/* Ensure the file is only readable to the current user. */
		path = g_file_get_path (file);
		if (g_chmod (path, 0600) != 0) {
			g_set_error (&child_error, G_IO_ERROR, G_IO_ERROR_FAILED,
			             _("Error changing exported file permissions: %s"),
			             g_strerror (errno));

			g_object_unref (file);
			g_free (path);

			break;
		}

		g_object_unref (file);
		g_free (path);

		/* Progress callback */
		progress_idle_callback (progress_callback, progress_user_data, &date);

		/* Clear the buffer. */
		gtk_text_buffer_delete (buffer, &start_iter, &end_iter);

		/* Check for cancellation */
		if (cancellable != NULL && g_cancellable_set_error_if_cancelled (cancellable, &child_error) == TRUE)
			break;
	}

	/* Check if the loop was broken due to an error */
	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		goto finish;
	}

	success = TRUE;

finish:
	g_object_unref (buffer);

	return success;
}

static gboolean
export_database (AlmanahExportOperation *self, GFile *destination, AlmanahExportProgressCallback progress_callback, gpointer progress_user_data,
                 GCancellable *cancellable, GError **error)
{
	GFile *source;
	gboolean success;
	gchar *destination_path;
	AlmanahExportOperationPrivate *priv = almanah_export_operation_get_instance_private (self);

	/* We ignore the progress callbacks, since this is a fairly fast operation, and it exports all the entries at once. */

	/* Get the input file (current unencrypted database) */
	source = g_file_new_for_path (almanah_storage_manager_get_filename (priv->storage_manager));

	/* Copy the current database to that location */
	success = g_file_copy (source, destination, G_FILE_COPY_OVERWRITE, cancellable, NULL, NULL, error);

	/* Ensure the backup is only readable to the current user. */
	destination_path = g_file_get_path (destination);
	if (success == TRUE && g_chmod (destination_path, 0600) != 0) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
		             _("Error changing exported file permissions: %s"),
		             g_strerror (errno));
		success = FALSE;
	}

	g_free (destination_path);
	g_object_unref (source);

	return success;
}

typedef struct {
	AlmanahExportProgressCallback progress_callback;
	gpointer progress_user_data;
} ExportData;

static void
export_data_free (ExportData *data)
{
	g_slice_free (ExportData, data);
}

static void
export_thread (GTask *task, AlmanahExportOperation *operation, gpointer task_data, GCancellable *cancellable)
{
	AlmanahExportOperationPrivate *priv = almanah_export_operation_get_instance_private (operation);
	GError *error = NULL;
	ExportData *data = (ExportData *) task_data;

	/* Check to see if the operation's been cancelled already */
	if (g_cancellable_set_error_if_cancelled (cancellable, &error) == TRUE) {
		g_task_return_error (task, error);
		return;
	}

	/* Export and return */
	if (export_modes[priv->current_mode].export_func (operation, priv->destination, data->progress_callback,
	    data->progress_user_data, cancellable, &error) == FALSE) {
		g_task_return_error (task, error);
	}
}

void
almanah_export_operation_run (AlmanahExportOperation *self, GCancellable *cancellable, AlmanahExportProgressCallback progress_callback,
                              gpointer progress_user_data,GAsyncReadyCallback callback, gpointer user_data)
{
	g_autoptr (GTask) task = NULL;
	ExportData *data;

	g_return_if_fail (ALMANAH_IS_EXPORT_OPERATION (self));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	data = g_slice_new (ExportData);
	data->progress_callback = progress_callback;
	data->progress_user_data = progress_user_data;

	task = g_task_new (G_OBJECT (self), cancellable, callback, user_data);
	g_task_set_source_tag (task, almanah_export_operation_run);
	g_task_set_task_data (task, data, (GDestroyNotify) export_data_free);
	g_task_run_in_thread (task, (GTaskThreadFunc) export_thread);
}

gboolean
almanah_export_operation_finish (AlmanahExportOperation *self, GAsyncResult *async_result, GError **error)
{
	GTask *task = G_TASK (async_result);

	g_return_val_if_fail (ALMANAH_IS_EXPORT_OPERATION (self), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (async_result), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	g_warn_if_fail (g_task_get_source_tag (task) == almanah_export_operation_run);

	if (g_async_result_legacy_propagate_error (G_ASYNC_RESULT (task), error) == TRUE)
		return FALSE;

	return TRUE;
}

void
almanah_export_operation_populate_model (GtkListStore *store, guint type_id_column, guint name_column, guint description_column, guint action_column)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (export_modes); i++) {
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
		                    type_id_column, i,
		                    name_column, _(export_modes[i].name),
		                    description_column, _(export_modes[i].description),
		                    action_column, export_modes[i].action,
		                    -1);
	}
}
