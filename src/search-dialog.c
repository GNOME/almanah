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
#include "main.h"
#include "main-window.h"

static void sd_response_cb (GtkDialog *dialog, gint response_id, AlmanahSearchDialog *search_dialog);
static void sd_results_selection_changed_cb (GtkTreeSelection *tree_selection, GtkWidget *button);

/* GtkBuilder callbacks */
void sd_search_button_clicked_cb (GtkButton *self, AlmanahSearchDialog *search_dialog);
void sd_results_tree_view_row_activated_cb (GtkTreeView *self, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data);
void sd_view_button_clicked_cb (GtkButton *self, AlmanahSearchDialog *search_dialog);

struct _AlmanahSearchDialogPrivate {
	GtkEntry *sd_search_entry;
	GtkListStore *sd_results_store;
	GtkTreeSelection *sd_results_selection;
};

G_DEFINE_TYPE (AlmanahSearchDialog, almanah_search_dialog, GTK_TYPE_DIALOG)
#define ALMANAH_SEARCH_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_SEARCH_DIALOG, AlmanahSearchDialogPrivate))

static void
almanah_search_dialog_class_init (AlmanahSearchDialogClass *klass)
{
	g_type_class_add_private (klass, sizeof (AlmanahSearchDialogPrivate));
}

static void
almanah_search_dialog_init (AlmanahSearchDialog *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_SEARCH_DIALOG, AlmanahSearchDialogPrivate);

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
	const gchar *interface_filename = almanah_get_interface_filename ();
	const gchar *object_names[] = {
		"almanah_search_dialog",
		"almanah_sd_search_button_image",
		"almanah_sd_results_store",
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
	search_dialog = ALMANAH_SEARCH_DIALOG (gtk_builder_get_object (builder, "almanah_search_dialog"));
	gtk_builder_connect_signals (builder, search_dialog);

	if (search_dialog == NULL) {
		g_object_unref (builder);
		return NULL;
	}

	priv = ALMANAH_SEARCH_DIALOG (search_dialog)->priv;

	/* Grab our child widgets */
	priv->sd_search_entry = GTK_ENTRY (gtk_builder_get_object (builder, "almanah_sd_search_entry"));
	priv->sd_results_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "almanah_sd_results_store"));
	priv->sd_results_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_builder_get_object (builder, "almanah_sd_results_tree_view")));

	g_signal_connect (priv->sd_results_selection, "changed", G_CALLBACK (sd_results_selection_changed_cb),
			  gtk_builder_get_object (builder, "almanah_sd_view_button"));

	gtk_widget_grab_default (GTK_WIDGET (gtk_builder_get_object (builder, "almanah_sd_search_button")));

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
	/* Ensure everything's tidy before we leave the room */
	gtk_list_store_clear (search_dialog->priv->sd_results_store);
	gtk_entry_set_text (search_dialog->priv->sd_search_entry, "");

	gtk_widget_hide (GTK_WIDGET (dialog));
}

void
sd_search_button_clicked_cb (GtkButton *self, AlmanahSearchDialog *search_dialog)
{
	AlmanahEntry *entry;
	AlmanahStorageManagerIter iter;
	AlmanahSearchDialogPrivate *priv = search_dialog->priv;
	const gchar *search_string = gtk_entry_get_text (priv->sd_search_entry);

	/* Clear the results store of previous search results first */
	gtk_list_store_clear (search_dialog->priv->sd_results_store);

	/* Search over all entries */
	almanah_storage_manager_iter_init (&iter);
	while ((entry = almanah_storage_manager_search_entries (almanah->storage_manager, search_string, &iter)) != NULL) {
		GDate date;
		gchar formatted_date[100];
		GtkTreeIter tree_iter;

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

		g_object_unref (entry);
	}
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
sd_results_tree_view_row_activated_cb (GtkTreeView *self, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model (self);
	gtk_tree_model_get_iter (model, &iter, path);
	select_date (model, &iter);
}

void
sd_view_button_clicked_cb (GtkButton *self, AlmanahSearchDialog *search_dialog)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (gtk_tree_selection_get_selected (search_dialog->priv->sd_results_selection, &model, &iter) == TRUE)
		select_date (model, &iter);
}
