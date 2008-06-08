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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "main.h"
#include "storage-manager.h"
#include "search-dialog.h"
#include "main-window.h"

static void
results_selection_changed_cb (GtkTreeSelection *tree_selection, GtkWidget *button)
{
	gtk_widget_set_sensitive (button, gtk_tree_selection_count_selected_rows (tree_selection) == 0 ? FALSE : TRUE);
}

void
diary_search_dialog_setup (GtkBuilder *builder)
{
	g_signal_connect (diary->sd_results_selection, "changed", (GCallback) results_selection_changed_cb,
			  gtk_builder_get_object (builder, "dry_sd_view_button"));
}

void
sd_destroy_cb (GtkWindow *window, gpointer user_data)
{
	gtk_widget_hide_all (diary->search_dialog);
}

void
sd_search_button_clicked_cb (GtkButton *self, gpointer user_data)
{
	GDate *results;
	guint result_count, i;
	GtkTreeIter iter;

	result_count = diary_storage_manager_search_entries (diary->storage_manager,
							     gtk_entry_get_text (diary->sd_search_entry), &results);

	for (i = 0; i < result_count; i++) {
		gchar formatted_date[100];

		/* Translators: This is a strftime()-format string for the dates displayed in search results. */
		g_date_strftime (formatted_date, sizeof (formatted_date), _("%A, %e %B %Y"), &results[i]);

		gtk_list_store_append (diary->sd_results_store, &iter);
		gtk_list_store_set (diary->sd_results_store, &iter,
				    0, g_date_get_day (&results[i]),
				    1, g_date_get_month (&results[i]),
				    2, g_date_get_year (&results[i]),
				    3, &formatted_date,
				    -1);
	}

	g_free (results);
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
	mw_select_date (&date);
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
sd_view_button_clicked_cb (GtkButton *self, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (gtk_tree_selection_get_selected (diary->sd_results_selection, &model, &iter) == TRUE)
		select_date (model, &iter);
}
