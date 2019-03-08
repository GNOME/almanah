/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2008-2009 <philip@tecnocode.co.uk>
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

#include "search-dialog.h"
#include "interface.h"
#include "main-window.h"

static void sd_response_cb (GtkDialog *dialog, gint response_id, AlmanahSearchDialog *search_dialog);
static void sd_results_selection_changed_cb (GtkTreeSelection *tree_selection, GtkWidget *button);

/* GtkBuilder callbacks */
void sd_search_button_clicked_cb (GtkButton *self, AlmanahSearchDialog *search_dialog);
void sd_cancel_button_clicked_cb (GtkButton *self, AlmanahSearchDialog *search_dialog);
void sd_results_tree_view_row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, AlmanahSearchDialog *self);
void sd_view_button_clicked_cb (GtkButton *self, AlmanahSearchDialog *search_dialog);

static void sd_search_progress_cb (AlmanahStorageManager *storage_manager, AlmanahEntry *entry, AlmanahSearchDialog **search_dialog_weak_pointer);
static void sd_search_ready_cb (AlmanahStorageManager *storage_manager, GAsyncResult *res, AlmanahSearchDialog **search_dialog_weak_pointer);

typedef struct {
	GtkEntry *sd_search_entry;
	GtkWidget *sd_search_button;
	GtkWidget *sd_cancel_button;
	GtkSpinner *sd_search_spinner;
	GtkLabel *sd_results_label;
	GtkWidget *sd_results_alignment;
	GtkListStore *sd_results_store;
	GtkTreeSelection *sd_results_selection;
	GCancellable *sd_cancellable;
} AlmanahSearchDialogPrivate;

struct _AlmanahSearchDialog {
	GtkDialog parent;
};

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahSearchDialog, almanah_search_dialog, GTK_TYPE_DIALOG)

static void
almanah_search_dialog_class_init (AlmanahSearchDialogClass *klass)
{
}

static void
almanah_search_dialog_init (AlmanahSearchDialog *self)
{
	g_signal_connect (self, "response", G_CALLBACK (sd_response_cb), self);
	gtk_window_set_modal (GTK_WINDOW (self), FALSE);
	gtk_window_set_title (GTK_WINDOW (self), _("Search"));
}

AlmanahSearchDialog *
almanah_search_dialog_new (void)
{
	GtkBuilder *builder;
	AlmanahSearchDialog *search_dialog;
	AlmanahSearchDialogPrivate *priv;
	GError *error = NULL;
	const gchar *object_names[] = {
		"almanah_search_dialog",
		"almanah_sd_search_button_image",
		"almanah_sd_cancel_button_image",
		"almanah_sd_results_store",
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
	search_dialog = ALMANAH_SEARCH_DIALOG (gtk_builder_get_object (builder, "almanah_search_dialog"));
	gtk_builder_connect_signals (builder, search_dialog);

	if (search_dialog == NULL) {
		g_object_unref (builder);
		return NULL;
	}

	priv = almanah_search_dialog_get_instance_private (search_dialog);

	priv->sd_cancellable = NULL;

	/* Grab our child widgets */
	priv->sd_search_entry = GTK_ENTRY (gtk_builder_get_object (builder, "almanah_sd_search_entry"));
	priv->sd_results_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "almanah_sd_results_store"));
	priv->sd_results_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_builder_get_object (builder, "almanah_sd_results_tree_view")));
	priv->sd_results_alignment = GTK_WIDGET (gtk_builder_get_object (builder, "almanah_sd_results_alignment"));
	priv->sd_search_spinner = GTK_SPINNER (gtk_builder_get_object (builder, "almanah_sd_search_spinner"));
	priv->sd_results_label = GTK_LABEL (gtk_builder_get_object (builder, "almanah_sd_results_label"));
	priv->sd_search_button = GTK_WIDGET (gtk_builder_get_object (builder, "almanah_sd_search_button"));
	priv->sd_cancel_button = GTK_WIDGET (gtk_builder_get_object (builder, "almanah_sd_cancel_button"));

	g_signal_connect (priv->sd_results_selection, "changed", G_CALLBACK (sd_results_selection_changed_cb),
			  gtk_builder_get_object (builder, "almanah_sd_view_button"));

	gtk_widget_grab_default (priv->sd_search_button);

	g_object_unref (builder);

	return search_dialog;
}

static void
sd_results_selection_changed_cb (GtkTreeSelection *tree_selection, GtkWidget *button)
{
	gtk_widget_set_sensitive (button, gtk_tree_selection_count_selected_rows (tree_selection) == 0 ? FALSE : TRUE);
}

static void
sd_response_cb (GtkDialog *dialog, gint response_id, AlmanahSearchDialog *search_dialog)
{
	AlmanahSearchDialogPrivate *priv = almanah_search_dialog_get_instance_private (search_dialog);

	/* Ensure everything's tidy before we leave the room */
	if (priv->sd_cancellable != NULL) {
		g_cancellable_cancel (priv->sd_cancellable);
	}

	gtk_list_store_clear (priv->sd_results_store);
	gtk_entry_set_text (priv->sd_search_entry, "");

	gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
sd_search_progress_cb (AlmanahStorageManager *storage_manager, AlmanahEntry *entry, AlmanahSearchDialog **search_dialog_weak_pointer)
{
	AlmanahSearchDialogPrivate *priv;
	GDate date;
	gchar formatted_date[100];
	GtkTreeIter tree_iter;

	if (*search_dialog_weak_pointer == NULL) {
		/* The dialogue's been finalised already, so we'd better just return */
		return;
	}

	g_return_if_fail (ALMANAH_IS_STORAGE_MANAGER (storage_manager));
	g_return_if_fail (ALMANAH_IS_ENTRY (entry));
	g_return_if_fail (ALMANAH_IS_SEARCH_DIALOG (*search_dialog_weak_pointer));

	priv = almanah_search_dialog_get_instance_private (*search_dialog_weak_pointer);

	almanah_entry_get_date (entry, &date);
	/* Translators: This is a strftime()-format string for the dates displayed in search results. */
	g_date_strftime (formatted_date, sizeof (formatted_date), _("%A, %e %B %Y"), &date);

	gtk_list_store_append (priv->sd_results_store, &tree_iter);
	gtk_list_store_set (priv->sd_results_store, &tree_iter,
	                    0, g_date_get_day (&date),
	                    1, g_date_get_month (&date),
	                    2, g_date_get_year (&date),
	                    3, &formatted_date,
	                    4, (almanah_entry_is_important (entry) == TRUE) ? "emblem-important" : NULL,
	                    -1);
}

static void
sd_search_ready_cb (AlmanahStorageManager *storage_manager, GAsyncResult *res, AlmanahSearchDialog **search_dialog_weak_pointer)
{
	AlmanahSearchDialogPrivate *priv;
	AlmanahSearchDialog *search_dialog;
	gint counter;
	GError *error = NULL;

	/* Finish the operation */
	counter = almanah_storage_manager_search_entries_async_finish (storage_manager, res, &error);

	if (*search_dialog_weak_pointer == NULL) {
		/* The dialogue's been finalised already, so we'd better just return */
		g_clear_error (&error);
		return;
	}

	g_return_if_fail (ALMANAH_IS_SEARCH_DIALOG (*search_dialog_weak_pointer));

	search_dialog = ALMANAH_SEARCH_DIALOG (*search_dialog_weak_pointer);
	priv = almanah_search_dialog_get_instance_private (search_dialog);

	/* Return the search result count to the user */
	gtk_spinner_stop (priv->sd_search_spinner);
	gtk_widget_hide (GTK_WIDGET (priv->sd_search_spinner));
	gtk_widget_set_sensitive (priv->sd_cancel_button, FALSE);
	gtk_widget_set_sensitive (priv->sd_search_button, TRUE);

	if (error != NULL && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) == TRUE) {
		gtk_label_set_text (priv->sd_results_label, _("Search canceled."));
	} else if (error != NULL) {
		/* Translators: This is an error message wrapper for when searches encounter an error. The placeholder is for an error message. */
		gchar *error_message = g_strdup_printf (_("Error: %s"), error->message);
		gtk_label_set_text (priv->sd_results_label, error_message);
		g_free (error_message);
	} else {
		/* Success! */
		gchar *results_string = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "Found %d entry:", "Found %d entries:", counter), counter);
		gtk_label_set_text (priv->sd_results_label, results_string);
		g_free (results_string);
	}

	g_clear_error (&error);

	/* Tidy up */
	g_object_remove_weak_pointer (G_OBJECT (search_dialog), (gpointer*) search_dialog_weak_pointer);
	g_slice_free (AlmanahSearchDialog*, search_dialog_weak_pointer);

	g_object_unref (priv->sd_cancellable);
	priv->sd_cancellable = NULL;
}

void
sd_cancel_button_clicked_cb (GtkButton *self, AlmanahSearchDialog *search_dialog)
{
	AlmanahSearchDialogPrivate *priv = almanah_search_dialog_get_instance_private (search_dialog);

	if (priv->sd_cancellable != NULL) {
		g_cancellable_cancel (priv->sd_cancellable);
	}
}

void
sd_search_button_clicked_cb (GtkButton *self, AlmanahSearchDialog *search_dialog)
{
	AlmanahSearchDialogPrivate *priv = almanah_search_dialog_get_instance_private (search_dialog);
	AlmanahApplication *application;
	AlmanahStorageManager *storage_manager;
	const gchar *search_string;
	AlmanahSearchDialog **search_dialog_weak_pointer;

	if (priv->sd_cancellable != NULL) {
		// Already searching
		g_assert_not_reached ();
		return;
	}

	/* Clear the results store of previous search results first */
	gtk_list_store_clear (priv->sd_results_store);

	priv->sd_cancellable = g_cancellable_new ();

	search_string = gtk_entry_get_text (priv->sd_search_entry);

	/* Change UI to show the status */
	gtk_widget_show (GTK_WIDGET (priv->sd_results_alignment));
	gtk_label_set_text (priv->sd_results_label, _("Searching…"));
	gtk_widget_show (GTK_WIDGET (priv->sd_search_spinner));
	gtk_spinner_start (priv->sd_search_spinner);

	/* Grab the storage manager */
	application = ALMANAH_APPLICATION (gtk_window_get_application (GTK_WINDOW (search_dialog)));
	storage_manager = almanah_application_dup_storage_manager (application);

	/* Launch the search */
	search_dialog_weak_pointer = g_slice_new (AlmanahSearchDialog*);
	*search_dialog_weak_pointer = search_dialog;
	g_object_add_weak_pointer (G_OBJECT (search_dialog), (gpointer*) search_dialog_weak_pointer);

	almanah_storage_manager_search_entries_async (storage_manager, search_string, priv->sd_cancellable,
	                                              (AlmanahStorageManagerSearchCallback) sd_search_progress_cb,
	                                              (gpointer) search_dialog_weak_pointer, NULL,
	                                              (GAsyncReadyCallback) sd_search_ready_cb, (gpointer) search_dialog_weak_pointer);

	/* Allow the user to cancel the search */
	gtk_widget_set_sensitive (priv->sd_search_button, FALSE);
	gtk_widget_set_sensitive (priv->sd_cancel_button, TRUE);
	gtk_widget_grab_default (priv->sd_cancel_button);

	g_object_unref (storage_manager);
}

static void
select_date (AlmanahSearchDialog *self, GtkTreeModel *model, GtkTreeIter *iter)
{
	AlmanahMainWindow *main_window;
	guint day, month, year;
	GDate date;

	gtk_tree_model_get (model, iter,
			    0, &day,
			    1, &month,
			    2, &year,
			    -1);

	main_window = ALMANAH_MAIN_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (self)));
	g_date_set_dmy (&date, day, month, year);
	almanah_main_window_select_date (main_window, &date);
}

void
sd_results_tree_view_row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, AlmanahSearchDialog *self)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model (tree_view);
	gtk_tree_model_get_iter (model, &iter, path);
	select_date (self, model, &iter);
}

void
sd_view_button_clicked_cb (GtkButton *self, AlmanahSearchDialog *search_dialog)
{
	AlmanahSearchDialogPrivate *priv = almanah_search_dialog_get_instance_private (search_dialog);
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (gtk_tree_selection_get_selected (priv->sd_results_selection, &model, &iter) == TRUE) {
		select_date (search_dialog, model, &iter);
	}
}
