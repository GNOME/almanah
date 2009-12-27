/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2009 <philip@tecnocode.co.uk>
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

#include "import-dialog.h"
#include "interface.h"
#include "main.h"
#include "main-window.h"

typedef enum {
	MODE_TEXT_FILES = 0,
	MODE_DATABASE
} ImportMode;

static const struct { ImportMode mode; const gchar *name; } import_modes[] = {
	{ MODE_TEXT_FILES, N_("Text Files") },
	{ MODE_DATABASE, N_("Database") }
};

static void response_cb (GtkDialog *dialog, gint response_id, AlmanahImportDialog *self);

/* GtkBuilder callbacks */
void id_mode_combo_box_changed_cb (GtkComboBox *combo_box, AlmanahImportDialog *self);
void id_file_chooser_selection_changed_cb (GtkFileChooser *file_chooser, AlmanahImportDialog *self);
void id_file_chooser_file_activated_cb (GtkFileChooser *file_chooser, AlmanahImportDialog *self);

struct _AlmanahImportDialogPrivate {
	GtkComboBox *mode_combo_box;
	GtkListStore *mode_store;
	ImportMode current_mode;
	GtkFileChooser *file_chooser;
	GtkWidget *import_button;
	GtkLabel *description_label;
};

G_DEFINE_TYPE (AlmanahImportDialog, almanah_import_dialog, GTK_TYPE_DIALOG)
#define ALMANAH_IMPORT_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_IMPORT_DIALOG, AlmanahImportDialogPrivate))

static void
almanah_import_dialog_class_init (AlmanahImportDialogClass *klass)
{
	g_type_class_add_private (klass, sizeof (AlmanahImportDialogPrivate));
}

static void
almanah_import_dialog_init (AlmanahImportDialog *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_IMPORT_DIALOG, AlmanahImportDialogPrivate);
	self->priv->current_mode = -1; /* no mode selected */

	g_signal_connect (self, "response", G_CALLBACK (response_cb), self);
	g_signal_connect (self, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), self);
	gtk_dialog_set_has_separator (GTK_DIALOG (self), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (self), TRUE);
	gtk_window_set_title (GTK_WINDOW (self), _("Import Entries"));
	gtk_window_set_transient_for (GTK_WINDOW (self), GTK_WINDOW (almanah->main_window));
	gtk_window_set_default_size (GTK_WINDOW (self), 600, 400);
}

static void
populate_mode_store (GtkListStore *store)
{
	guint i;

	/* Add all the available import modes to the combo box */
	for (i = 0; i < G_N_ELEMENTS (import_modes); i++) {
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, _(import_modes[i].name), 1, import_modes[i].mode, -1);
	}
}

AlmanahImportDialog *
almanah_import_dialog_new (void)
{
	GtkBuilder *builder;
	AlmanahImportDialog *import_dialog;
	AlmanahImportDialogPrivate *priv;
	GError *error = NULL;
	const gchar *interface_filename = almanah_get_interface_filename ();
	const gchar *object_names[] = {
		"almanah_id_mode_store",
		"almanah_import_dialog",
		NULL
	};

	builder = gtk_builder_new ();

	if (gtk_builder_add_objects_from_file (builder, interface_filename, (gchar**) object_names, &error) == FALSE) {
		/* Show an error */
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				_("UI file \"%s\" could not be loaded"), interface_filename);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
		g_object_unref (builder);

		return NULL;
	}

	gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	import_dialog = ALMANAH_IMPORT_DIALOG (gtk_builder_get_object (builder, "almanah_import_dialog"));
	gtk_builder_connect_signals (builder, import_dialog);

	if (import_dialog == NULL) {
		g_object_unref (builder);
		return NULL;
	}

	priv = import_dialog->priv;

	/* Grab our child widgets */
	priv->mode_combo_box = GTK_COMBO_BOX (gtk_builder_get_object (builder, "almanah_id_mode_combo_box"));
	priv->file_chooser = GTK_FILE_CHOOSER (gtk_builder_get_object (builder, "almanah_id_file_chooser"));
	priv->import_button = GTK_WIDGET (gtk_builder_get_object (builder, "almanah_id_import_button"));
	priv->mode_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "almanah_id_mode_store"));
	priv->description_label = GTK_LABEL (gtk_builder_get_object (builder, "almanah_id_description_label"));

	/* Populate the mode combo box */
	populate_mode_store (priv->mode_store);
	gtk_combo_box_set_active (priv->mode_combo_box, 0);

	g_object_unref (builder);

	return import_dialog;
}

/**
 * set_entry:
 * @self: an #AlmanahImportDialog
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
set_entry (AlmanahImportDialog *self, AlmanahEntry *imported_entry, const gchar *import_source, gchar **message)
{
	GDate entry_date;
	AlmanahEntry *existing_entry;
	GtkTextBuffer *existing_buffer, *imported_buffer;
	GtkTextIter existing_start, existing_end, imported_start, imported_end;
	gchar *header_string;
	GError *error = NULL;

	/* Check to see if there's a conflict first */
	almanah_entry_get_date (imported_entry, &entry_date);
	existing_entry = almanah_storage_manager_get_entry (almanah->storage_manager, &entry_date);

	if (existing_entry == NULL) {
		/* Add the entry to the proper database and return, ignoring failure */
		almanah_storage_manager_set_entry (almanah->storage_manager, imported_entry);
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
	if (almanah_entry_get_content (existing_entry, existing_buffer, FALSE, NULL) == FALSE) {
		/* Deserialising the existing entry failed; use the imported entry instead */
		almanah_storage_manager_set_entry (almanah->storage_manager, imported_entry);

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

	almanah_storage_manager_set_entry (almanah->storage_manager, existing_entry);
	g_object_unref (existing_entry);

	return ALMANAH_IMPORT_STATUS_MERGED;
}

static gboolean
import_text_files (AlmanahImportDialog *self, AlmanahImportResultsDialog *results_dialog, GError **error)
{
	gboolean retval = FALSE;
	GFile *folder;
	GFileInfo *file_info;
	GFileEnumerator *enumerator;
	GtkTextBuffer *buffer;
	GError *child_error = NULL;
	AlmanahImportDialogPrivate *priv = self->priv;

	/* Get the folder containing all the files to import */
	folder = gtk_file_chooser_get_file (priv->file_chooser);
	g_assert (folder != NULL);

	enumerator = g_file_enumerate_children (folder, "standard::name,standard::display-name,standard::is-hidden", G_FILE_QUERY_INFO_NONE,
						NULL, error);
	g_object_unref (folder);
	if (enumerator == NULL)
		return FALSE;

	/* Build a text buffer to use when setting all the entries */
	buffer = gtk_text_buffer_new (NULL);

	/* Enumerate all the children of the folder */
	while ((file_info = g_file_enumerator_next_file (enumerator, NULL, &child_error)) != NULL) {
		AlmanahEntry *entry;
		GDate parsed_date;
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
		file = g_file_get_child (folder, file_name);
		g_assert (file != NULL);

		/* Load the content */
		if (g_file_load_contents (file, NULL, &contents, &length, NULL, error) == FALSE) {
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

		/* Store the entry */
		status = set_entry (self, entry, display_name, &message);
		almanah_import_results_dialog_add_result (results_dialog, &parsed_date, status, message);
		g_free (message);

		g_object_unref (entry);
		g_object_unref (file_info);
	}

	/* Check if the loop was broken due to an error */
	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		goto finish;
	}

	/* Success! */
	retval = TRUE;

finish:
	g_object_unref (folder);
	g_object_unref (enumerator);
	g_object_unref (buffer);

	return retval;
}

static gboolean
import_database (AlmanahImportDialog *self, AlmanahImportResultsDialog *results_dialog, GError **error)
{
	GFile *file;
	GFileInfo *file_info;
	gchar *path;
	const gchar *display_name;
	GSList *entries, *i, *definitions;
	AlmanahStorageManager *database;
	AlmanahImportDialogPrivate *priv = self->priv;

	/* Get the database file to import */
	file = gtk_file_chooser_get_file (priv->file_chooser);
	g_assert (file != NULL);

	/* Get the display name for use with set_entry(), below */
	file_info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, G_FILE_QUERY_INFO_NONE, NULL, NULL);
	display_name = g_file_info_get_display_name (file_info);

	/* Open the database */
	path = g_file_get_path (file);
	database = almanah_storage_manager_new (path);
	g_free (path);
	g_object_unref (file);

	/* Connect to the database */
	if (almanah_storage_manager_connect (database, error) == FALSE) {
		g_object_unref (database);
		return FALSE;
	}

	/* Query for every entry */
	entries = almanah_storage_manager_get_entries (database);
	for (i = entries; i != NULL; i = i->next) {
		GDate date;
		gchar *message = NULL;
		AlmanahImportStatus status;
		AlmanahEntry *entry = ALMANAH_ENTRY (i->data);

		almanah_entry_get_date (entry, &date);

		status = set_entry (self, entry, display_name, &message);
		almanah_import_results_dialog_add_result (results_dialog, &date, status, message);
		g_free (message);

		g_object_unref (entry);
	}
	g_slist_free (entries);

	/* Query for every definition */
	definitions = almanah_storage_manager_get_definitions (database);
	for (i = definitions; i != NULL; i = i->next) {
		/* Add the definition to the proper database, ignoring failure */
		almanah_storage_manager_add_definition (almanah->storage_manager, ALMANAH_DEFINITION (i->data));
		g_object_unref (i->data);
	}
	g_slist_free (definitions);

	almanah_storage_manager_disconnect (database, NULL);
	g_object_unref (database);
	g_object_unref (file_info);

	return TRUE;
}

static void
response_cb (GtkDialog *dialog, gint response_id, AlmanahImportDialog *self)
{
	GError *error = NULL;
	AlmanahImportDialogPrivate *priv = self->priv;
	AlmanahImportResultsDialog *results_dialog;

	/* Just return if the user pressed Cancel */
	if (response_id != GTK_RESPONSE_OK) {
		gtk_widget_hide (GTK_WIDGET (self));
		return;
	}

	/* Import the entries according to the selected method. It's OK if we block here, since the dialogue should
	 * be running in its own main loop. */
	results_dialog = almanah_import_results_dialog_new ();
	switch (priv->current_mode) {
		case MODE_TEXT_FILES:
			import_text_files (self, results_dialog, &error);
			break;
		case MODE_DATABASE:
			import_database (self, results_dialog, &error);
			break;
		default:
			g_assert_not_reached ();
	}

	/* Check for errors (e.g. errors opening databases or files; not errors importing individual entries once we have the content to import) */
	if (error != NULL) {
		/* Show an error */
		GtkWidget *error_dialog = gtk_message_dialog_new (GTK_WINDOW (self), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
								  GTK_BUTTONS_OK, _("Import failed"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (error_dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (error_dialog));
		gtk_widget_destroy (error_dialog);

		g_error_free (error);

		gtk_widget_hide (GTK_WIDGET (self));
	} else {
		/* Show the results dialogue */
		gtk_widget_hide (GTK_WIDGET (self));
		gtk_widget_show_all (GTK_WIDGET (results_dialog));
		gtk_dialog_run (GTK_DIALOG (results_dialog));
	}

	gtk_widget_destroy (GTK_WIDGET (results_dialog));
}

void
id_mode_combo_box_changed_cb (GtkComboBox *combo_box, AlmanahImportDialog *self)
{
	gint new_mode;
	AlmanahImportDialogPrivate *priv = self->priv;

	new_mode = gtk_combo_box_get_active (combo_box);
	if (new_mode == -1 || new_mode == (gint) priv->current_mode)
		return;

	priv->current_mode = new_mode;

	/* Create the new widgets */
	switch (priv->current_mode) {
		case MODE_TEXT_FILES:
			gtk_file_chooser_set_action (priv->file_chooser, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
			gtk_label_set_text (priv->description_label, _("Select a folder containing text files, one per entry, with names in the "
								       "format 'yyyy-mm-dd', and no extension. Any and all such files will be "
								       "imported."));
			break;
		case MODE_DATABASE:
			gtk_file_chooser_set_action (priv->file_chooser, GTK_FILE_CHOOSER_ACTION_OPEN);
			gtk_label_set_text (priv->description_label, _("Select a database file created by Almanah Diary to import."));
			break;
		default:
			g_assert_not_reached ();
	}
}

void
id_file_chooser_selection_changed_cb (GtkFileChooser *file_chooser, AlmanahImportDialog *self)
{
	GFile *current_file;

	/* Change the sensitivity of the dialogue's buttons based on whether a file is selected */
	current_file = gtk_file_chooser_get_file (file_chooser);
	if (current_file == NULL) {
		gtk_widget_set_sensitive (self->priv->import_button, FALSE);
	} else {
		gtk_widget_set_sensitive (self->priv->import_button, TRUE);
		g_object_unref (current_file);
	}
}

void
id_file_chooser_file_activated_cb (GtkFileChooser *file_chooser, AlmanahImportDialog *self)
{
	/* Activate the dialogue's default button */
	gtk_window_activate_default (GTK_WINDOW (self));
}

static gboolean filter_results_cb (GtkTreeModel *model, GtkTreeIter *iter, AlmanahImportResultsDialog *self);
static void results_selection_changed_cb (GtkTreeSelection *tree_selection, GtkWidget *button);

/* GtkBuilder callbacks */
void ird_results_tree_view_row_activated_cb (GtkTreeView *self, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data);
void ird_view_button_clicked_cb (GtkButton *button, AlmanahImportResultsDialog *self);
void ird_view_combo_box_changed_cb (GtkComboBox *combo_box, AlmanahImportResultsDialog *self);

struct _AlmanahImportResultsDialogPrivate {
	GtkListStore *results_store;
	GtkTreeSelection *results_selection;
	GtkTreeModelFilter *filtered_results_store;
	GtkComboBox *view_combo_box;
	AlmanahImportStatus current_mode;
};

G_DEFINE_TYPE (AlmanahImportResultsDialog, almanah_import_results_dialog, GTK_TYPE_DIALOG)
#define ALMANAH_IMPORT_RESULTS_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_IMPORT_RESULTS_DIALOG,\
							AlmanahImportResultsDialogPrivate))

static void
almanah_import_results_dialog_class_init (AlmanahImportResultsDialogClass *klass)
{
	g_type_class_add_private (klass, sizeof (AlmanahImportResultsDialogPrivate));
}

static void
almanah_import_results_dialog_init (AlmanahImportResultsDialog *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_IMPORT_RESULTS_DIALOG, AlmanahImportResultsDialogPrivate);

	g_signal_connect (self, "response", G_CALLBACK (response_cb), self);
	g_signal_connect (self, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), self);
	gtk_dialog_set_has_separator (GTK_DIALOG (self), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (self), TRUE);
	gtk_window_set_title (GTK_WINDOW (self), _("Import Results"));
	gtk_window_set_default_size (GTK_WINDOW (self), 600, 400);
	gtk_window_set_modal (GTK_WINDOW (self), FALSE);
}

AlmanahImportResultsDialog *
almanah_import_results_dialog_new (void)
{
	GtkBuilder *builder;
	AlmanahImportResultsDialog *results_dialog;
	AlmanahImportResultsDialogPrivate *priv;
	GError *error = NULL;
	const gchar *interface_filename = almanah_get_interface_filename ();
	const gchar *object_names[] = {
		"almanah_ird_view_store",
		"almanah_ird_results_store",
		"almanah_ird_filtered_results_store",
		"almanah_import_results_dialog",
		NULL
	};

	builder = gtk_builder_new ();

	if (gtk_builder_add_objects_from_file (builder, interface_filename, (gchar**) object_names, &error) == FALSE) {
		/* Show an error */
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				_("UI file \"%s\" could not be loaded"), interface_filename);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
		g_object_unref (builder);

		return NULL;
	}

	gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	results_dialog = ALMANAH_IMPORT_RESULTS_DIALOG (gtk_builder_get_object (builder, "almanah_import_results_dialog"));
	gtk_builder_connect_signals (builder, results_dialog);

	if (results_dialog == NULL) {
		g_object_unref (builder);
		return NULL;
	}

	priv = results_dialog->priv;

	/* Grab our child widgets */
	priv->results_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "almanah_ird_results_store"));
	priv->results_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_builder_get_object (builder, "almanah_ird_results_tree_view")));
	priv->filtered_results_store = GTK_TREE_MODEL_FILTER (gtk_builder_get_object (builder, "almanah_ird_filtered_results_store"));
	priv->view_combo_box = GTK_COMBO_BOX (gtk_builder_get_object (builder, "almanah_ird_view_combo_box"));

	g_signal_connect (priv->results_selection, "changed", G_CALLBACK (results_selection_changed_cb),
			  gtk_builder_get_object (builder, "almanah_ird_view_button"));

	/* Set up the tree filter */
	gtk_tree_model_filter_set_visible_func (priv->filtered_results_store, (GtkTreeModelFilterVisibleFunc) filter_results_cb, results_dialog, NULL);

	/* Set up the combo box */
	gtk_combo_box_set_active (priv->view_combo_box, ALMANAH_IMPORT_STATUS_MERGED);

	g_object_unref (builder);

	return results_dialog;
}

static gboolean
filter_results_cb (GtkTreeModel *model, GtkTreeIter *iter, AlmanahImportResultsDialog *self)
{
	guint status;

	/* Compare the current mode to the row's status column */
	gtk_tree_model_get (model, iter, 4, &status, -1);

	return (self->priv->current_mode == status) ? TRUE : FALSE;
}

static void
results_selection_changed_cb (GtkTreeSelection *tree_selection, GtkWidget *button)
{
	gtk_widget_set_sensitive (button, gtk_tree_selection_count_selected_rows (tree_selection) == 0 ? FALSE : TRUE);
}

void
almanah_import_results_dialog_add_result (AlmanahImportResultsDialog *self, GDate *date, AlmanahImportStatus status, const gchar *message)
{
	GtkTreeIter iter;
	gchar formatted_date[100];

	/* Translators: This is a strftime()-format string for the dates displayed in import results. */
	g_date_strftime (formatted_date, sizeof (formatted_date), _("%A, %e %B %Y"), date);

	gtk_list_store_append (self->priv->results_store, &iter);
	gtk_list_store_set (self->priv->results_store, &iter,
			    0, g_date_get_day (date),
			    1, g_date_get_month (date),
			    2, g_date_get_year (date),
			    3, &formatted_date,
			    4, status,
			    5, message,
			    -1);
}

static void
select_date (GtkTreeModel *model, GtkTreeIter *iter)
{
	guint day, month, year;
	GDate date;

	gtk_tree_model_get (model, iter,
			    0, &day,
			    1, &month,
			    2, &year,
			    -1);

	g_date_set_dmy (&date, day, month, year);
	almanah_main_window_select_date (ALMANAH_MAIN_WINDOW (almanah->main_window), &date);
}

void
ird_results_tree_view_row_activated_cb (GtkTreeView *self, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model (self);
	gtk_tree_model_get_iter (model, &iter, path);
	select_date (model, &iter);
}

void
ird_view_button_clicked_cb (GtkButton *button, AlmanahImportResultsDialog *self)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (gtk_tree_selection_get_selected (self->priv->results_selection, &model, &iter) == TRUE)
		select_date (model, &iter);
}

void
ird_view_combo_box_changed_cb (GtkComboBox *combo_box, AlmanahImportResultsDialog *self)
{
	gint new_mode;
	AlmanahImportResultsDialogPrivate *priv = self->priv;

	new_mode = gtk_combo_box_get_active (combo_box);
	if (new_mode == -1 || new_mode == (gint) priv->current_mode)
		return;

	priv->current_mode = new_mode;
	gtk_tree_model_filter_refilter (self->priv->filtered_results_store);
}
