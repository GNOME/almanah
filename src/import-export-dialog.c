/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2009–2010 <philip@tecnocode.co.uk>
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

#include "import-export-dialog.h"
#include "import-operation.h"
#include "export-operation.h"
#include "interface.h"
#include "main-window.h"

static void response_cb (GtkDialog *dialog, gint response_id, AlmanahImportExportDialog *self);

/* GtkBuilder callbacks */
void ied_mode_combo_box_changed_cb (GtkComboBox *combo_box, AlmanahImportExportDialog *self);
void ied_file_chooser_selection_changed_cb (GtkFileChooser *file_chooser, AlmanahImportExportDialog *self);
void ied_file_chooser_file_activated_cb (GtkFileChooser *file_chooser, AlmanahImportExportDialog *self);

typedef struct {
	AlmanahStorageManager *storage_manager;
	gboolean import; /* TRUE if we're in import mode, FALSE otherwise */
	GtkComboBox *mode_combo_box;
	GtkListStore *mode_store;
	gint current_mode;
	GtkFileChooser *file_chooser;
	GtkWidget *import_export_button;
	GtkLabel *description_label;
	GtkProgressBar *progress_bar;
	GCancellable *cancellable; /* non-NULL iff an operation is underway */
} AlmanahImportExportDialogPrivate;

struct _AlmanahImportExportDialog {
	GtkDialog parent;
};

enum {
	PROP_STORAGE_MANAGER = 1,
};

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahImportExportDialog, almanah_import_export_dialog, GTK_TYPE_DIALOG)

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void almanah_import_export_dialog_dispose (GObject *object);

static void
almanah_import_export_dialog_class_init (AlmanahImportExportDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = get_property;
	gobject_class->set_property = set_property;
	gobject_class->dispose = almanah_import_export_dialog_dispose;

	g_object_class_install_property (gobject_class, PROP_STORAGE_MANAGER,
	                                 g_param_spec_object ("storage-manager",
	                                                      "Storage manager", "The local storage manager: source for export operations and "
	                                                      "destination for import operations.",
	                                                      ALMANAH_TYPE_STORAGE_MANAGER,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_import_export_dialog_init (AlmanahImportExportDialog *self)
{
	AlmanahImportExportDialogPrivate *priv = almanah_import_export_dialog_get_instance_private (self);

	priv->current_mode = -1; /* no mode selected */

	g_signal_connect (self, "response", G_CALLBACK (response_cb), self);
	gtk_window_set_default_size (GTK_WINDOW (self), 500, 400);
}

static void
almanah_import_export_dialog_dispose (GObject *object)
{
	AlmanahImportExportDialogPrivate *priv = almanah_import_export_dialog_get_instance_private (ALMANAH_IMPORT_EXPORT_DIALOG (object));

	if (priv->storage_manager != NULL)
		g_object_unref (priv->storage_manager);
	priv->storage_manager = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_import_export_dialog_parent_class)->dispose (object);
}

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahImportExportDialogPrivate *priv = almanah_import_export_dialog_get_instance_private (ALMANAH_IMPORT_EXPORT_DIALOG (object));

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
	AlmanahImportExportDialog *self = ALMANAH_IMPORT_EXPORT_DIALOG (object);
	AlmanahImportExportDialogPrivate *priv = almanah_import_export_dialog_get_instance_private (self);

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

/**
 * almanah_import_export_dialog_new:
 * @import: %TRUE to set the dialog up for importing, %FALSE to set it up for exporting
 * @storage_manager: the storage manager which will be the source for export operations and the destination for import operations
 *
 * Returns a new #AlmanahImportExportDialog, configured for importing if @import is %TRUE, and exporting otherwise.
 *
 * Return value: a new #AlmanahImportExportDialog; destroy with gtk_widget_destroy()
 **/
AlmanahImportExportDialog *
almanah_import_export_dialog_new (AlmanahStorageManager *storage_manager, gboolean import)
{
	GtkBuilder *builder;
	AlmanahImportExportDialog *import_export_dialog;
	AlmanahImportExportDialogPrivate *priv;
	GError *error = NULL;
	const gchar *object_names[] = {
		"almanah_ied_mode_store",
		"almanah_import_export_dialog",
		NULL
	};

	g_return_val_if_fail (ALMANAH_IS_STORAGE_MANAGER (storage_manager), NULL);

	builder = gtk_builder_new ();

	if (gtk_builder_add_objects_from_resource (builder, "/org/gnome/Almanah/ui/almanah.ui", (gchar**) object_names, &error) == 0) {
		/* Show an error */
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
							    GTK_DIALOG_MODAL,
							    GTK_MESSAGE_ERROR,
							    GTK_BUTTONS_OK,
		                                            _("UI data could not be loaded"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
		g_object_unref (builder);

		return NULL;
	}

	gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	import_export_dialog = ALMANAH_IMPORT_EXPORT_DIALOG (gtk_builder_get_object (builder, "almanah_import_export_dialog"));
	gtk_builder_connect_signals (builder, import_export_dialog);

	if (import_export_dialog == NULL) {
		g_object_unref (builder);
		return NULL;
	}

	priv = almanah_import_export_dialog_get_instance_private (import_export_dialog);
	priv->storage_manager = g_object_ref (storage_manager);
	priv->import = import;

	/* Grab our child widgets */
	priv->mode_combo_box = GTK_COMBO_BOX (gtk_builder_get_object (builder, "almanah_ied_mode_combo_box"));
	priv->file_chooser = GTK_FILE_CHOOSER (gtk_builder_get_object (builder, "almanah_ied_file_chooser"));
	priv->import_export_button = GTK_WIDGET (gtk_builder_get_object (builder, "almanah_ied_import_export_button"));
	priv->mode_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "almanah_ied_mode_store"));
	priv->description_label = GTK_LABEL (gtk_builder_get_object (builder, "almanah_ied_description_label"));
	priv->progress_bar = GTK_PROGRESS_BAR (gtk_builder_get_object (builder, "almanah_ied_progress_bar"));

	/* Set the mode label */
	gtk_label_set_text_with_mnemonic (GTK_LABEL (gtk_builder_get_object (builder, "almanah_ied_mode_label")),
	                                  (import == TRUE) ? _("Import _mode: ") : _("Export _mode: "));

	/* Set the window title */
	gtk_window_set_title (GTK_WINDOW (import_export_dialog), (import == TRUE) ? _("Import Entries") : _("Export Entries"));

	/* Set the button label. */
	gtk_button_set_label (GTK_BUTTON (priv->import_export_button),
	                      /* Translators: These are verbs. */
	                      (import == TRUE) ? C_("Dialog button", "_Import") : C_("Dialog button", "_Export"));

	/* Populate the mode combo box */
	if (import == TRUE)
		almanah_import_operation_populate_model (priv->mode_store, 0, 1, 2, 3);
	else
		almanah_export_operation_populate_model (priv->mode_store, 0, 1, 2, 3);
	gtk_combo_box_set_active (priv->mode_combo_box, 0);

	g_object_unref (builder);

	return import_export_dialog;
}

static void
import_progress_cb (const GDate *date, AlmanahImportStatus status, const gchar *message, AlmanahImportResultsDialog *results_dialog)
{
	AlmanahImportExportDialog *self = ALMANAH_IMPORT_EXPORT_DIALOG (gtk_window_get_transient_for (GTK_WINDOW (results_dialog))); /* set in response_cb() */
	AlmanahImportExportDialogPrivate *priv = almanah_import_export_dialog_get_instance_private (self);

	almanah_import_results_dialog_add_result (results_dialog, date, status, message);
	gtk_progress_bar_pulse (priv->progress_bar);
}

static void
import_cb (AlmanahImportOperation *operation, GAsyncResult *async_result, AlmanahImportResultsDialog *results_dialog)
{
	AlmanahImportExportDialog *self = ALMANAH_IMPORT_EXPORT_DIALOG (gtk_window_get_transient_for (GTK_WINDOW (results_dialog))); /* set in response_cb() */
	AlmanahImportExportDialogPrivate *priv = almanah_import_export_dialog_get_instance_private (self);
	GError *error = NULL;

	/* Check for errors (e.g. errors opening databases or files; not errors importing individual entries once we have the content to import) */
	if (almanah_import_operation_finish (operation, async_result, &error) == FALSE) {
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) == FALSE) {
			/* Show an error if the operation wasn't cancelled */
			GtkWidget *error_dialog = gtk_message_dialog_new (GTK_WINDOW (self), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
			                                                  GTK_BUTTONS_OK, _("Import failed"));
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (error_dialog), "%s", error->message);
			gtk_dialog_run (GTK_DIALOG (error_dialog));
			gtk_widget_destroy (error_dialog);
		}

		g_error_free (error);
	} else {
		/* Show the results dialogue */
		gtk_widget_hide (GTK_WIDGET (self));
		gtk_widget_show_all (GTK_WIDGET (results_dialog));
		gtk_dialog_run (GTK_DIALOG (results_dialog));
	}

	gtk_widget_destroy (GTK_WIDGET (results_dialog));

	g_object_unref (priv->cancellable);
	priv->cancellable = NULL;

	gtk_widget_destroy (GTK_WIDGET (self));
}

static void
export_progress_cb (const GDate *date, AlmanahImportExportDialog *self)
{
	AlmanahImportExportDialogPrivate *priv = almanah_import_export_dialog_get_instance_private (self);

	gtk_progress_bar_pulse (priv->progress_bar);
}

static void
export_cb (AlmanahExportOperation *operation, GAsyncResult *async_result, AlmanahImportExportDialog *self)
{
	GError *error = NULL;

	/* Check for errors (e.g. errors opening databases or files; not errors importing individual entries once we have the content to import) */
	if (almanah_export_operation_finish (operation, async_result, &error) == FALSE) {
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) == FALSE) {
			/* Show an error if the operation wasn't cancelled */
			GtkWidget *error_dialog = gtk_message_dialog_new (GTK_WINDOW (self), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
				                                          GTK_BUTTONS_OK, _("Export failed"));
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (error_dialog), "%s", error->message);
			gtk_dialog_run (GTK_DIALOG (error_dialog));
			gtk_widget_destroy (error_dialog);
		}

		g_error_free (error);
	} else {
		/* Show a success message */
		GtkWidget *message_dialog;

		gtk_widget_hide (GTK_WIDGET (self));
		message_dialog = gtk_message_dialog_new (GTK_WINDOW (self), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
		                                         GTK_BUTTONS_OK, _("Export successful"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message_dialog), _("The diary was successfully exported."));
		gtk_dialog_run (GTK_DIALOG (message_dialog));
		gtk_widget_destroy (message_dialog);
	}

	AlmanahImportExportDialogPrivate *priv = almanah_import_export_dialog_get_instance_private (self);

	g_object_unref (priv->cancellable);
	priv->cancellable = NULL;

	gtk_widget_destroy (GTK_WIDGET (self));
}

static void
response_cb (GtkDialog *dialog, gint response_id, AlmanahImportExportDialog *self)
{
	AlmanahImportExportDialogPrivate *priv = almanah_import_export_dialog_get_instance_private (self);
	GFile *file;

	/* If the user pressed Cancel, cancel the operation if we've started, and return otherwise */
	if (response_id != GTK_RESPONSE_OK) {
		if (priv->cancellable == NULL)
			gtk_widget_destroy (GTK_WIDGET (self));
		else
			g_cancellable_cancel (priv->cancellable);
		return;
	}

	/* Disable the widgets */
	gtk_widget_set_sensitive (priv->import_export_button, FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->file_chooser), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->mode_combo_box), FALSE);

	/* Get the input/output file or folder */
	file = gtk_file_chooser_get_file (priv->file_chooser);
	g_assert (file != NULL);

	/* Set up for cancellation */
	g_assert (priv->cancellable == NULL);
	priv->cancellable = g_cancellable_new ();

	if (priv->import == TRUE) {
		/* Import the entries according to the selected method.*/
		AlmanahImportOperation *operation;
		AlmanahImportResultsDialog *results_dialog = almanah_import_results_dialog_new (); /* destroyed in import_cb() */
		gtk_window_set_transient_for (GTK_WINDOW (results_dialog), GTK_WINDOW (self)); /* this is required in import_cb() */

		operation = almanah_import_operation_new (priv->current_mode, file, priv->storage_manager);
		almanah_import_operation_run (operation, priv->cancellable, (AlmanahImportProgressCallback) import_progress_cb, results_dialog,
		                              (GAsyncReadyCallback) import_cb, results_dialog);
		g_object_unref (operation);
	} else {
		/* Export the entries according to the selected method. */
		AlmanahExportOperation *operation;

		operation = almanah_export_operation_new (priv->current_mode, priv->storage_manager, file);
		almanah_export_operation_run (operation, priv->cancellable, (AlmanahExportProgressCallback) export_progress_cb, self,
		                              (GAsyncReadyCallback) export_cb, self);
		g_object_unref (operation);
	}

	g_object_unref (file);
}

void
ied_mode_combo_box_changed_cb (GtkComboBox *combo_box, AlmanahImportExportDialog *self)
{
	AlmanahImportExportDialogPrivate *priv = almanah_import_export_dialog_get_instance_private (self);
	gint new_mode;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *description;
	GtkFileChooserAction action;

	new_mode = gtk_combo_box_get_active (combo_box);
	if (new_mode == -1 || new_mode == priv->current_mode)
		return;

	priv->current_mode = new_mode;

	/* Change the dialogue */
	gtk_combo_box_get_active_iter (combo_box, &iter);
	model = gtk_combo_box_get_model (combo_box);
	gtk_tree_model_get (model, &iter,
	                    2, &description,
	                    3, &action,
	                    -1);

	gtk_file_chooser_set_action (priv->file_chooser, action);
	gtk_label_set_text_with_mnemonic (priv->description_label, description);

	g_free (description);
}

void
ied_file_chooser_selection_changed_cb (GtkFileChooser *file_chooser, AlmanahImportExportDialog *self)
{
	AlmanahImportExportDialogPrivate *priv = almanah_import_export_dialog_get_instance_private (self);
	GFile *current_file;

	/* Change the sensitivity of the dialogue's buttons based on whether a file is selected */
	current_file = gtk_file_chooser_get_file (file_chooser);
	if (current_file == NULL) {
		gtk_widget_set_sensitive (priv->import_export_button, FALSE);
	} else {
		gtk_widget_set_sensitive (priv->import_export_button, TRUE);
		g_object_unref (current_file);
	}
}

void
ied_file_chooser_file_activated_cb (GtkFileChooser *file_chooser, AlmanahImportExportDialog *self)
{
	/* Activate the dialogue's default button */
	gtk_window_activate_default (GTK_WINDOW (self));
}

static gboolean filter_results_cb (GtkTreeModel *model, GtkTreeIter *iter, AlmanahImportResultsDialog *self);
static void results_selection_changed_cb (GtkTreeSelection *tree_selection, GtkWidget *button);

/* GtkBuilder callbacks */
void ird_results_tree_view_row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, AlmanahImportResultsDialog *self);
void ird_view_button_clicked_cb (GtkButton *button, AlmanahImportResultsDialog *self);
void ird_view_combo_box_changed_cb (GtkComboBox *combo_box, AlmanahImportResultsDialog *self);

typedef struct {
	GtkListStore *results_store;
	GtkTreeSelection *results_selection;
	GtkTreeModelFilter *filtered_results_store;
	GtkComboBox *view_combo_box;
	AlmanahImportStatus current_mode;
} AlmanahImportResultsDialogPrivate;

struct _AlmanahImportResultsDialog {
	GtkDialog parent;
	AlmanahImportResultsDialogPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahImportResultsDialog, almanah_import_results_dialog, GTK_TYPE_DIALOG)

static void
almanah_import_results_dialog_class_init (AlmanahImportResultsDialogClass *klass)
{
}

static void
almanah_import_results_dialog_init (AlmanahImportResultsDialog *self)
{
	g_signal_connect (self, "response", G_CALLBACK (response_cb), self);
	g_signal_connect (self, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), self);
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
	const gchar *object_names[] = {
		"almanah_ird_view_store",
		"almanah_ird_results_store",
		"almanah_ird_filtered_results_store",
		"almanah_import_results_dialog",
		NULL
	};

	builder = gtk_builder_new ();

	if (gtk_builder_add_objects_from_resource (builder, "/org/gnome/Almanah/ui/almanah.ui", (gchar**) object_names, &error) == 0) {
		/* Show an error */
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
							    GTK_DIALOG_MODAL,
							    GTK_MESSAGE_ERROR,
							    GTK_BUTTONS_OK,
		                                            _("UI data could not be loaded"));
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

	priv = almanah_import_results_dialog_get_instance_private (results_dialog);

	/* Grab our child widgets */
	priv->results_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "almanah_ird_results_store"));
	priv->results_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_builder_get_object (builder, "almanah_ird_results_tree_view")));
	priv->filtered_results_store = GTK_TREE_MODEL_FILTER (gtk_builder_get_object (builder, "almanah_ird_filtered_results_store"));
	priv->view_combo_box = GTK_COMBO_BOX (gtk_builder_get_object (builder, "almanah_ird_view_combo_box"));

	g_signal_connect (priv->results_selection, "changed", G_CALLBACK (results_selection_changed_cb),
	                  gtk_builder_get_object (builder, "almanah_ird_view_button"));

	/* Set up the tree filter */
	gtk_tree_model_filter_set_visible_func (priv->filtered_results_store, (GtkTreeModelFilterVisibleFunc) filter_results_cb,
	                                        results_dialog, NULL);

	/* Set up the combo box */
	gtk_combo_box_set_active (priv->view_combo_box, ALMANAH_IMPORT_STATUS_MERGED);

	g_object_unref (builder);

	return results_dialog;
}

static gboolean
filter_results_cb (GtkTreeModel *model, GtkTreeIter *iter, AlmanahImportResultsDialog *self)
{
	AlmanahImportResultsDialogPrivate *priv = almanah_import_results_dialog_get_instance_private (self);
	guint status;

	/* Compare the current mode to the row's status column */
	gtk_tree_model_get (model, iter, 4, &status, -1);

	return (priv->current_mode == status) ? TRUE : FALSE;
}

static void
results_selection_changed_cb (GtkTreeSelection *tree_selection, GtkWidget *button)
{
	gtk_widget_set_sensitive (button, gtk_tree_selection_count_selected_rows (tree_selection) == 0 ? FALSE : TRUE);
}

void
almanah_import_results_dialog_add_result (AlmanahImportResultsDialog *self, const GDate *date, AlmanahImportStatus status, const gchar *message)
{
	AlmanahImportResultsDialogPrivate *priv = almanah_import_results_dialog_get_instance_private (self);
	GtkTreeIter iter;
	gchar formatted_date[100];

	/* Translators: This is a strftime()-format string for the dates displayed in import results. */
	g_date_strftime (formatted_date, sizeof (formatted_date), _("%A, %e %B %Y"), date);

	gtk_list_store_append (priv->results_store, &iter);
	gtk_list_store_set (priv->results_store, &iter,
	                    0, g_date_get_day (date),
	                    1, g_date_get_month (date),
	                    2, g_date_get_year (date),
	                    3, &formatted_date,
	                    4, status,
	                    5, message,
	                    -1);
}

static void
select_date (AlmanahImportResultsDialog *self, GtkTreeModel *model, GtkTreeIter *iter)
{
	AlmanahMainWindow *main_window;
	guint day, month, year;
	GDate date;

	gtk_tree_model_get (model, iter,
	                    0, &day,
	                    1, &month,
	                    2, &year,
	                    -1);

	main_window = ALMANAH_MAIN_WINDOW (gtk_window_get_transient_for (gtk_window_get_transient_for (GTK_WINDOW (self))));
	g_date_set_dmy (&date, day, month, year);
	almanah_main_window_select_date (main_window, &date);
}

void
ird_results_tree_view_row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, AlmanahImportResultsDialog *self)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model (tree_view);
	gtk_tree_model_get_iter (model, &iter, path);
	select_date (self, model, &iter);
}

void
ird_view_button_clicked_cb (GtkButton *button, AlmanahImportResultsDialog *self)
{
	AlmanahImportResultsDialogPrivate *priv = almanah_import_results_dialog_get_instance_private (self);
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (gtk_tree_selection_get_selected (priv->results_selection, &model, &iter) == TRUE) {
		select_date (self, model, &iter);
	}
}

void
ird_view_combo_box_changed_cb (GtkComboBox *combo_box, AlmanahImportResultsDialog *self)
{
	AlmanahImportResultsDialogPrivate *priv = almanah_import_results_dialog_get_instance_private (self);
	gint new_mode;

	new_mode = gtk_combo_box_get_active (combo_box);
	if (new_mode == -1 || new_mode == (gint) priv->current_mode)
		return;

	priv->current_mode = new_mode;
	gtk_tree_model_filter_refilter (priv->filtered_results_store);
}
