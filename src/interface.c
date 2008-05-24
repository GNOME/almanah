/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Diary
 * Copyright (C) Philip Withnall 2008 <philip@tecnocode.co.uk>
 * 
 * Diary is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * Diary is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Diary.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "config.h"
#include "main.h"
#include "interface.h"

GtkWidget *
diary_create_interface (void)
{
	GError *error = NULL;
	GtkBuilder *builder;

	builder = gtk_builder_new ();

	if (gtk_builder_add_from_file (builder, "./data/diary.ui", &error) == FALSE) {
		/* Show an error */
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				_("UI file \""PACKAGE_DATA_DIR"/diary/diary.ui\" could not be loaded. Error: %s"), error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
		g_object_unref (builder);
		diary_quit ();

		return NULL;
	}

	gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	gtk_builder_connect_signals (builder, NULL);

	/* Set up the main window */
	/* TODO: This is horrible */
	diary->main_window = GTK_WIDGET (gtk_builder_get_object (builder, "dry_main_window"));
	diary->entry_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (gtk_builder_get_object (builder, "dry_mw_entry_view")));
	diary->calendar = GTK_CALENDAR (gtk_builder_get_object (builder, "dry_mw_calendar"));
	diary->date_label = GTK_LABEL (gtk_builder_get_object (builder, "dry_mw_date_label"));
	diary->add_button = GTK_BUTTON (gtk_builder_get_object (builder, "dry_mw_add_button"));
	diary->remove_button = GTK_BUTTON (gtk_builder_get_object (builder, "dry_mw_remove_button"));
	diary->add_action = GTK_ACTION (gtk_builder_get_object (builder, "dry_ui_add_link"));
	diary->remove_action = GTK_ACTION (gtk_builder_get_object (builder, "dry_ui_remove_link"));
	diary->links_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "dry_mw_links_store"));
	diary->links_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_builder_get_object (builder, "dry_mw_links_tree_view")));
	diary->link_value_column = GTK_TREE_VIEW_COLUMN (gtk_builder_get_object (builder, "dry_mw_link_value_column"));
	diary->link_value_renderer = GTK_CELL_RENDERER_TEXT (gtk_builder_get_object (builder, "dry_mw_link_value_renderer"));

	/* Set up the add link dialogue */
	diary->add_link_dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dry_add_link_dialog"));
	diary->ald_type_combo_box = GTK_COMBO_BOX (gtk_builder_get_object (builder, "dry_ald_type_combo_box"));
	diary->ald_table = GTK_TABLE (gtk_builder_get_object (builder, "dry_ald_table"));
	diary->ald_type_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "dry_ald_type_store"));
	diary_populate_link_model (diary->ald_type_store, 1, 0, 2);
	gtk_combo_box_set_active (diary->ald_type_combo_box, 0);

	g_object_unref (builder);

	return diary->main_window;
}

/**
 * diary_interface_error:
 * @message: Error message
 * @parent_window: The error dialog's parent window
 *
 * Display an error message and print the message to
 * the console.
 **/
void
diary_interface_error (const gchar *message, GtkWidget *parent_window)
{
	GtkWidget *dialog;

	g_warning (message);

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent_window),
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

