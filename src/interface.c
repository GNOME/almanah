/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Diary
 * Copyright (C) Philip Withnall 2008 <philip@tecnocode.co.uk>
 * 
 * Diary is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Diary is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Diary.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "main.h"
#include "main-window.h"
#include "add-link-dialog.h"
#include "search-dialog.h"
#include "interface.h"

GtkWidget *
diary_create_interface (void)
{
	GError *error = NULL;
	GtkBuilder *builder;

	builder = gtk_builder_new ();

	if (gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR"/diary2/diary.ui", &error) == FALSE &&
	    gtk_builder_add_from_file (builder, "./data2/diary.ui", NULL) == FALSE) {
		/* Show an error */
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				_("UI file \"%s/diary/diary.ui\" could not be loaded."), PACKAGE_DATA_DIR);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), error->message);
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
	diary->entry_view = GTK_TEXT_VIEW (gtk_builder_get_object (builder, "dry_mw_entry_view"));
	diary->entry_buffer = gtk_text_view_get_buffer (diary->entry_view);
	diary->calendar = GTK_CALENDAR (gtk_builder_get_object (builder, "dry_mw_calendar"));
	diary->date_label = GTK_LABEL (gtk_builder_get_object (builder, "dry_mw_date_label"));
	diary->add_button = GTK_BUTTON (gtk_builder_get_object (builder, "dry_mw_add_button"));
	diary->remove_button = GTK_BUTTON (gtk_builder_get_object (builder, "dry_mw_remove_button"));
	diary->view_button = GTK_BUTTON (gtk_builder_get_object (builder, "dry_mw_view_button"));
	diary->add_action = GTK_ACTION (gtk_builder_get_object (builder, "dry_ui_add_link"));
	diary->remove_action = GTK_ACTION (gtk_builder_get_object (builder, "dry_ui_remove_link"));
	diary->links_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "dry_mw_links_store"));
	diary->links_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_builder_get_object (builder, "dry_mw_links_tree_view")));
	diary->link_value_column = GTK_TREE_VIEW_COLUMN (gtk_builder_get_object (builder, "dry_mw_link_value_column"));
	diary->link_value_renderer = GTK_CELL_RENDERER_TEXT (gtk_builder_get_object (builder, "dry_mw_link_value_renderer"));
	diary_main_window_setup (builder);

	/* Set up the add link dialogue */
	diary->add_link_dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dry_add_link_dialog"));
	diary->ald_type_combo_box = GTK_COMBO_BOX (gtk_builder_get_object (builder, "dry_ald_type_combo_box"));
	diary->ald_table = GTK_TABLE (gtk_builder_get_object (builder, "dry_ald_table"));
	diary->ald_type_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "dry_ald_type_store"));
	diary_add_link_dialog_setup (builder);

	/* Set up the search dialogue */
	diary->search_dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dry_search_dialog"));
	diary->sd_search_entry = GTK_ENTRY (gtk_builder_get_object (builder, "dry_sd_search_entry"));
	diary->sd_results_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "dry_sd_results_store"));
	diary->sd_results_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_builder_get_object (builder, "dry_sd_results_tree_view")));
	diary_search_dialog_setup (builder);

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

void
diary_calendar_month_changed_cb (GtkCalendar *calendar, gpointer user_data)
{
	/* Mark the days on the calendar which have diary entries */
	guint i, year, month;
	gboolean *days;

	gtk_calendar_get_date (calendar, &year, &month, NULL);
	month++;
	days = diary_storage_manager_get_month_marked_days (diary->storage_manager, year, month);

	/* TODO: Don't like hard-coding the array length here */
	gtk_calendar_clear_marks (calendar);
	for (i = 1; i < 32; i++) {
		if (days[i] == TRUE)
			gtk_calendar_mark_day (calendar, i);
		else
			gtk_calendar_unmark_day (calendar, i);
	}

	g_slice_free (gboolean, days);
}
