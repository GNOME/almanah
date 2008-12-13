/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2008 <philip@tecnocode.co.uk>
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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include "definition-manager-window.h"
#include "main.h"
#include "interface.h"
#include "definition.h"
#include "storage-manager.h"

static void almanah_definition_manager_window_init (AlmanahDefinitionManagerWindow *self);
static void almanah_definition_manager_window_dispose (GObject *object);
static void definition_selection_changed_cb (GtkTreeSelection *tree_selection, AlmanahDefinitionManagerWindow *self);
static void definition_added_cb (AlmanahStorageManager *storage_manager, AlmanahDefinition *definition, AlmanahDefinitionManagerWindow *self);
static void definition_removed_cb (AlmanahStorageManager *storage_manager, const gchar *definition_text, AlmanahDefinitionManagerWindow *self);
static void populate_definition_store (AlmanahDefinitionManagerWindow *self);

struct _AlmanahDefinitionManagerWindowPrivate {
	GtkLabel *name_label;
	GtkLabel *description_label;
	GtkListStore *definition_store;
	GtkTreeSelection *definition_selection;
	GtkButton *view_button;
	GtkButton *remove_button;

	AlmanahDefinition *current_definition;
};

G_DEFINE_TYPE (AlmanahDefinitionManagerWindow, almanah_definition_manager_window, GTK_TYPE_WINDOW)
#define ALMANAH_DEFINITION_MANAGER_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_DEFINITION_MANAGER_WINDOW, AlmanahDefinitionManagerWindowPrivate))

static void
almanah_definition_manager_window_class_init (AlmanahDefinitionManagerWindowClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	g_type_class_add_private (klass, sizeof (AlmanahDefinitionManagerWindowPrivate));
	gobject_class->dispose = almanah_definition_manager_window_dispose;
}

static void
almanah_definition_manager_window_init (AlmanahDefinitionManagerWindow *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_DEFINITION_MANAGER_WINDOW, AlmanahDefinitionManagerWindowPrivate);

	gtk_window_set_title (GTK_WINDOW (self), _("Definition Manager"));
	gtk_window_set_default_size (GTK_WINDOW (self), 500, 400);
	/*gtk_window_set_resizable (GTK_WINDOW (self), FALSE);*/
}

static gboolean
definitions_dispose_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	AlmanahDefinition *definition;

	gtk_tree_model_get (model, iter, 0, &definition, -1);
	g_object_unref (definition);

	return TRUE;
}

static void
almanah_definition_manager_window_dispose (GObject *object)
{
	AlmanahDefinitionManagerWindowPrivate *priv = ALMANAH_DEFINITION_MANAGER_WINDOW (object)->priv;

	/* Unref the current definition */
	if (priv->current_definition != NULL)
		g_object_unref (priv->current_definition);
	priv->current_definition = NULL;

	/* Unref all the definitions in the store */
	if (priv->definition_store != NULL) {
		gtk_tree_model_foreach (GTK_TREE_MODEL (priv->definition_store), (GtkTreeModelForeachFunc) definitions_dispose_cb, NULL);
	}
	priv->definition_store = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_definition_manager_window_parent_class)->dispose (object);
}

AlmanahDefinitionManagerWindow *
almanah_definition_manager_window_new (void)
{
	GtkBuilder *builder;
	AlmanahDefinitionManagerWindow *definition_manager_window;
	AlmanahDefinitionManagerWindowPrivate *priv;
	GError *error = NULL;
	const gchar *interface_filename = almanah_get_interface_filename ();
	const gchar *object_names[] = {
		"almanah_definition_manager_window",
		"almanah_dmw_definition_store",
		"almanah_mw_view_button_image",
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
	definition_manager_window = ALMANAH_DEFINITION_MANAGER_WINDOW (gtk_builder_get_object (builder, "almanah_definition_manager_window"));
	gtk_builder_connect_signals (builder, definition_manager_window);

	if (definition_manager_window == NULL) {
		g_object_unref (builder);
		return NULL;
	}

	priv = ALMANAH_DEFINITION_MANAGER_WINDOW (definition_manager_window)->priv;

	/* Grab our child widgets */
	priv->name_label = GTK_LABEL (gtk_builder_get_object (builder, "almanah_dmw_name_label"));
	priv->description_label = GTK_LABEL (gtk_builder_get_object (builder, "almanah_dmw_description_label"));
	priv->definition_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "almanah_dmw_definition_store"));
	priv->definition_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_builder_get_object (builder, "almanah_dmw_definition_tree_view")));
	priv->view_button = GTK_BUTTON (gtk_builder_get_object (builder, "almanah_dmw_view_button"));
	priv->remove_button = GTK_BUTTON (gtk_builder_get_object (builder, "almanah_dmw_remove_button"));

	/* Prettify some labels */
	almanah_interface_embolden_label (GTK_LABEL (gtk_builder_get_object (builder, "almanah_dmw_name_label_label")));
	almanah_interface_embolden_label (GTK_LABEL (gtk_builder_get_object (builder, "almanah_dmw_description_label_label")));

	/* Get notifications about added/removed definitions from the storage manager */
	g_signal_connect (almanah->storage_manager, "definition-added", G_CALLBACK (definition_added_cb), definition_manager_window);
	g_signal_connect (almanah->storage_manager, "definition-removed", G_CALLBACK (definition_removed_cb), definition_manager_window);

	/* Set up the treeview */
	g_signal_connect (priv->definition_selection, "changed", G_CALLBACK (definition_selection_changed_cb), definition_manager_window);

	/* Populate the definition list store */
	populate_definition_store (definition_manager_window);

	g_object_unref (builder);

	return definition_manager_window;
}

void
dmw_definition_tree_view_row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, AlmanahDefinitionManagerWindow *self)
{
	g_assert (self->priv->current_definition != NULL);
	almanah_definition_view (self->priv->current_definition);
}

void
dmw_view_button_clicked_cb (GtkButton *button, AlmanahDefinitionManagerWindow *self)
{
	g_assert (self->priv->current_definition != NULL);
	almanah_definition_view (self->priv->current_definition);
}

void
dmw_remove_button_clicked_cb (GtkButton *button, AlmanahDefinitionManagerWindow *self)
{
	AlmanahDefinitionManagerWindowPrivate *priv = self->priv;
	GtkWidget *dialog;

	g_assert (priv->current_definition != NULL);

	/* Check to see if it's really OK */
	dialog = gtk_message_dialog_new (GTK_WINDOW (self),
					 GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 _("Are you sure you want to delete the definition for \"%s\"?"),
					 almanah_definition_get_text (priv->current_definition));
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
				GTK_STOCK_REMOVE, GTK_RESPONSE_ACCEPT,
				NULL);

	gtk_widget_show_all (dialog);
	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT) {
		/* Cancelled the edit */
		gtk_widget_destroy (dialog);
		return;
	}

	gtk_widget_destroy (dialog);

	/* Delete the definition */
	almanah_storage_manager_remove_definition (almanah->storage_manager, almanah_definition_get_text (priv->current_definition));
}

static void
definition_selection_changed_cb (GtkTreeSelection *tree_selection, AlmanahDefinitionManagerWindow *self)
{
	AlmanahDefinitionManagerWindowPrivate *priv = self->priv;
	gboolean row_selected;
	GtkTreeModel *model;
	GtkTreeIter iter;
	AlmanahDefinition *definition;
	gchar *blurb;

	row_selected = gtk_tree_selection_get_selected (tree_selection, &model, &iter);

	/* Change the sensitivity of the "View" and "Remove" buttons */
	gtk_widget_set_sensitive (GTK_WIDGET (priv->view_button), row_selected);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->remove_button), row_selected);

	/* Unset priv->current_definition */
	if (priv->current_definition != NULL)
		g_object_unref (priv->current_definition);
	priv->current_definition = NULL;

	/* If nothing's selected, reset the window and return */
	if (row_selected == FALSE) {
		gtk_label_set_text (priv->name_label, _("Nothing selected"));
		gtk_label_set_text (priv->description_label, "");
		return;
	}

	/* Set the new priv->current_definition */
	gtk_tree_model_get (model, &iter, 0, &definition, -1);
	priv->current_definition = g_object_ref (definition);

	/* Display information about the selected definition */
	gtk_label_set_text (priv->name_label, almanah_definition_get_name (definition));
	blurb = almanah_definition_get_blurb (definition);
	gtk_label_set_text (priv->description_label, blurb);
	g_free (blurb);
}

static void
definition_added_cb (AlmanahStorageManager *storage_manager, AlmanahDefinition *definition, AlmanahDefinitionManagerWindow *self)
{
	AlmanahDefinitionManagerWindowPrivate *priv = self->priv;
	GtkTreeIter iter;

	/* Make sure we get a reference to the definition */
	g_object_ref (definition);

	gtk_list_store_append (priv->definition_store, &iter);
	gtk_list_store_set (priv->definition_store, &iter,
			    0, definition,
			    1, almanah_definition_get_text (definition),
			    -1);
}

static void
definition_removed_cb (AlmanahStorageManager *storage_manager, const gchar *definition_text, AlmanahDefinitionManagerWindow *self)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean valid_iter;

	model = GTK_TREE_MODEL (self->priv->definition_store);
	if (gtk_tree_model_get_iter_first (model, &iter) == FALSE)
		return;

	do {
		gchar *model_definition_text;
		GtkTreeIter next_iter;

		/* Get the next row ready */
		next_iter = iter;
		valid_iter = gtk_tree_model_iter_next (model, &next_iter);

		/* Remove the current row if it has the correct definition text, then break */
		gtk_tree_model_get (model, &iter, 1, &model_definition_text, -1);
		if (strcmp (model_definition_text, definition_text) == 0) {
			gtk_list_store_remove (self->priv->definition_store, &iter);
			valid_iter = FALSE;
		}
		g_free (model_definition_text);

		iter = next_iter;
	} while (valid_iter == TRUE);
}

static void
populate_definition_store (AlmanahDefinitionManagerWindow *self)
{
	AlmanahDefinitionManagerWindowPrivate *priv = self->priv;
	AlmanahDefinition **definitions;
	guint i = 0;

	definitions = almanah_storage_manager_get_definitions (almanah->storage_manager);
	while (definitions[i] != NULL) {
		GtkTreeIter iter;

		gtk_list_store_append (priv->definition_store, &iter);
		gtk_list_store_set (priv->definition_store, &iter,
				    0, definitions[i],
				    1, almanah_definition_get_text (definitions[i]),
				    -1);

		i++;
	}
	g_free (definitions);
}
