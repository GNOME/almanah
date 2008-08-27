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
#include <gtkspell/gtkspell.h>

#include "main.h"
#include "storage-manager.h"
#include "link.h"
#include "add-link-dialog.h"
#include "interface.h"
#include "main-window.h"
#include "printing.h"

static void save_current_entry ();
static void add_link_to_current_entry ();
static void remove_link_from_current_entry ();

void mw_calendar_day_selected_cb (GtkCalendar *calendar, gpointer user_data);
void mw_links_selection_changed_cb (GtkTreeSelection *tree_selection, gpointer user_data);
void mw_links_value_data_cb (GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data);

static void
save_current_entry ()
{
	GtkTextIter start_iter, end_iter;
	gchar *entry_text;
	guint year, month, day;

	g_assert (diary->entry_buffer != NULL);

	/* Don't save if it hasn't been/can't be edited */
	if (gtk_text_view_get_editable (diary->entry_view) == FALSE ||
	    gtk_text_buffer_get_modified (diary->entry_buffer) == FALSE)
		return;

	/* Save the entry */
	gtk_text_buffer_get_bounds (diary->entry_buffer, &start_iter, &end_iter);
	entry_text = gtk_text_buffer_get_text (diary->entry_buffer, &start_iter, &end_iter, FALSE);

	gtk_calendar_get_date (diary->calendar, &year, &month, &day);
	month++;

	/* Mark the day on the calendar if the entry was non-empty (and deleted)
	 * and update the state of the add link button. */
	if (diary_storage_manager_set_entry (diary->storage_manager, year, month, day, entry_text) == FALSE) {
		gtk_calendar_unmark_day (diary->calendar, day);

		gtk_widget_set_sensitive (GTK_WIDGET (diary->add_button), FALSE);
		gtk_action_set_sensitive (diary->add_action, FALSE);
	} else {
		gtk_calendar_mark_day (diary->calendar, day);

		gtk_widget_set_sensitive (GTK_WIDGET (diary->add_button), TRUE);
		gtk_action_set_sensitive (diary->add_action, TRUE);
	}

	gtk_text_buffer_set_modified (diary->entry_buffer, FALSE);
	g_free (entry_text);
}

static void
add_link_to_current_entry ()
{
	guint year, month, day;
	GtkTreeIter iter;
	const DiaryLinkType *link_type;
	gchar *type;
	DiaryLink link;

	g_assert (diary->entry_buffer != NULL);
	g_assert (gtk_text_buffer_get_char_count (diary->entry_buffer) != 0);

	/* Ensure that something is selected and its widgets displayed */
	g_signal_emit_by_name (diary->ald_type_combo_box, "changed", NULL, NULL);
	gtk_widget_show_all (diary->add_link_dialog);

	if (gtk_dialog_run (GTK_DIALOG (diary->add_link_dialog)) == GTK_RESPONSE_OK) {
		if (gtk_combo_box_get_active_iter (diary->ald_type_combo_box, &iter) == FALSE)
			return;

		/* Get the link type, then the values entered in the dialogue (specific to the type) */
		gtk_tree_model_get (GTK_TREE_MODEL (diary->ald_type_store), &iter, 1, &type, -1);
		link_type = diary_link_get_type (type);
		g_assert (link_type != NULL);
		link.type = type;
		link_type->get_values_func (&link);

		/* Add to the DB */
		gtk_calendar_get_date (diary->calendar, &year, &month, &day);
		month++;
		diary_storage_manager_add_entry_link (diary->storage_manager,
						      year, month, day,
						      type,
						      link.value,
						      link.value2);

		/* Add to the treeview */
		gtk_list_store_append (diary->links_store, &iter);
		gtk_list_store_set (diary->links_store, &iter,
				    0, type,
				    1, link.value,
				    2, link.value2,
				    3, link_type->icon_name,
				    -1);

		g_free (type);
		g_free (link.value);
		g_free (link.value2);
	}
	diary_hide_ald ();
}

static void
remove_link_from_current_entry ()
{
	gchar *link_type;
	guint year, month, day;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GList *links;

	g_assert (diary->entry_buffer != NULL);
	g_assert (gtk_text_buffer_get_char_count (diary->entry_buffer) != 0);

	links = gtk_tree_selection_get_selected_rows (diary->links_selection, &model);
	gtk_calendar_get_date (diary->calendar, &year, &month, &day);
	month++;

	for (; links != NULL; links = links->next) {
		gtk_tree_model_get_iter (model, &iter, (GtkTreePath*) links->data);
		gtk_tree_model_get (model, &iter, 0, &link_type, -1);

		/* Remove it from the DB */
		diary_storage_manager_remove_entry_link (diary->storage_manager, year, month, day, link_type);

		/* Remove it from the treeview */
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

		gtk_tree_path_free (links->data);
		g_free (link_type);
	}
	g_list_free (links);
}

void
diary_main_window_setup (GtkBuilder *builder)
{
	GError *error = NULL;

	/* Select the current day and month */
	diary_calendar_month_changed_cb (diary->calendar, NULL);
	mw_calendar_day_selected_cb (diary->calendar, NULL);

	/* Set up the treeview */
	g_signal_connect (diary->links_selection, "changed", (GCallback) mw_links_selection_changed_cb, NULL);
	gtk_tree_view_column_set_cell_data_func (diary->link_value_column, GTK_CELL_RENDERER (diary->link_value_renderer), mw_links_value_data_cb, NULL, NULL);

	if (gtkspell_new_attach (diary->entry_view, NULL, &error) == FALSE) {
		gchar *error_message = g_strdup_printf (_("The spelling checker could not be initialized: %s"), error->message);
		diary_interface_error (error_message, NULL);
		g_free (error_message);
	}
}

void
mw_select_date (GDate *date)
{
	gtk_calendar_select_month (diary->calendar, g_date_get_month (date) - 1, g_date_get_year (date));
	gtk_calendar_select_day (diary->calendar, g_date_get_day (date));
}

gboolean
mw_delete_event_cb (GtkWindow *window, gpointer user_data)
{
	save_current_entry ();
	diary_quit ();

	return TRUE;
}

void
mw_print_activate_cb (GtkAction *action, gpointer user_data)
{
	diary_print_entries ();
}

void
mw_quit_activate_cb (GtkAction *action, gpointer user_data)
{
	save_current_entry ();
	diary_quit ();
}

void
mw_cut_activate_cb (GtkAction *action, gpointer user_data)
{
	GtkClipboard *clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (GTK_WIDGET (diary->main_window)), GDK_SELECTION_CLIPBOARD);
	gtk_text_buffer_cut_clipboard (diary->entry_buffer, clipboard, TRUE);
}

void
mw_copy_activate_cb (GtkAction *action, gpointer user_data)
{
	GtkClipboard *clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (GTK_WIDGET (diary->main_window)), GDK_SELECTION_CLIPBOARD);
	gtk_text_buffer_copy_clipboard (diary->entry_buffer, clipboard);
}

void
mw_paste_activate_cb (GtkAction *action, gpointer user_data)
{
	GtkClipboard *clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (GTK_WIDGET (diary->main_window)), GDK_SELECTION_CLIPBOARD);
	gtk_text_buffer_paste_clipboard (diary->entry_buffer, clipboard, NULL, TRUE);
}

void
mw_delete_activate_cb (GtkAction *action, gpointer user_data)
{
	gtk_text_buffer_delete_selection (diary->entry_buffer, TRUE, TRUE);
}

void
mw_search_activate_cb (GtkAction *action, gpointer user_data)
{
	/* Ensure everything's tidy first */
	gtk_list_store_clear (diary->sd_results_store);
	gtk_entry_set_text (diary->sd_search_entry, "");

	/* Run the dialogue */
	gtk_widget_show_all (diary->search_dialog);
	gtk_dialog_run (GTK_DIALOG (diary->search_dialog));
	gtk_widget_hide_all (diary->search_dialog);
}

void
mw_about_activate_cb (GtkAction *action, gpointer user_data)
{
	gchar *license, *description;
	guint entry_count, link_count, character_count;

	const gchar *authors[] =
	{
		"Philip Withnall <philip@tecnocode.co.uk>",
		NULL
	};
	const gchar *license_parts[] = {
		N_("Diary is free software: you can redistribute it and/or modify "
		   "it under the terms of the GNU General Public License as published by "
		   "the Free Software Foundation, either version 3 of the License, or "
		   "(at your option) any later version."),
		N_("Diary is distributed in the hope that it will be useful, "
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		   "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License "
		   "along with Diary.  If not, see <http://www.gnu.org/licenses/>."),
	};

	license = g_strjoin ("\n\n",
			  _(license_parts[0]),
			  _(license_parts[1]),
			  _(license_parts[2]),
			  NULL);

	diary_storage_manager_get_statistics (diary->storage_manager, &entry_count, &link_count, &character_count);
	description = g_strdup_printf (_("A helpful diary keeper, storing %u entries with %u links and a total of %u characters."),
				      entry_count,
				      link_count,
				      character_count);

	gtk_show_about_dialog (GTK_WINDOW (diary->main_window),
				"version", VERSION,
				"copyright", _("Copyright \xc2\xa9 2008 Philip Withnall"),
				"comments", description,
				"authors", authors,
				/* Translators: please include your names here to be credited for your hard work!
				 * Format:
				 * "Translator name 1 <translator@email.address>\n"
				 * "Translator name 2 <translator2@email.address>"
				 */
				"translator-credits", _("translator-credits"),
				"logo-icon-name", "almanah",
				"license", license,
				"wrap-license", TRUE,
				"website-label", _("Diary Website"),
				"website", "http://tecnocode.co.uk/projects/diary",
				NULL);

	g_free (license);
	g_free (description);
}

void
mw_jump_to_today_activate_cb (GtkAction *action, gpointer user_data)
{
	GDate current_date;
	g_date_set_time_t (&current_date, time (NULL));
	mw_select_date (&current_date);
}

void
mw_add_link_activate_cb (GtkAction *action, gpointer user_data)
{
	add_link_to_current_entry ();
}

void
mw_remove_link_activate_cb (GtkAction *action, gpointer user_data)
{
	remove_link_from_current_entry ();
}

void
mw_calendar_day_selected_cb (GtkCalendar *calendar, gpointer user_data)
{
	GDate calendar_date;
	gchar calendar_string[100], *entry_text;
	guint year, month, day;
	DiaryLink **links;
	guint i;
	GtkTreeIter iter;
	const DiaryLinkType *link_type;
	GtkSpell *gtkspell;

	/* Update the date label */
	gtk_calendar_get_date (calendar, &year, &month, &day);
	month++;
	g_date_set_dmy (&calendar_date, day, month, year);

	/* Translators: This is a strftime()-format string for the date displayed at the top of the main window. */
	g_date_strftime (calendar_string, sizeof (calendar_string), _("%A, %e %B %Y"), &calendar_date);
	gtk_label_set_markup (diary->date_label, calendar_string);
	diary_interface_embolden_label (diary->date_label);

	/* Update the entry */
	entry_text = diary_storage_manager_get_entry (diary->storage_manager, year, month, day);
	gtk_text_view_set_editable (diary->entry_view, diary_storage_manager_entry_is_editable (diary->storage_manager, year, month, day) != DIARY_ENTRY_FUTURE ? TRUE : FALSE);
	gtk_text_buffer_set_modified (diary->entry_buffer, FALSE);

	if (entry_text != NULL) {
		gtk_text_buffer_set_text (diary->entry_buffer, entry_text, -1);
		gtk_widget_set_sensitive (GTK_WIDGET (diary->add_button), TRUE);
		gtk_action_set_sensitive (diary->add_action, TRUE);
	} else {
		gtk_text_buffer_set_text (diary->entry_buffer, "", -1);
		gtk_widget_set_sensitive (GTK_WIDGET (diary->add_button), FALSE);
		gtk_action_set_sensitive (diary->add_action, FALSE);
	}
	gtk_widget_set_sensitive (GTK_WIDGET (diary->remove_button), FALSE); /* Only sensitive if something's selected */
	gtk_action_set_sensitive (diary->remove_action, FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (diary->view_button), FALSE);

	g_free (entry_text);

	/* Ensure the spell-checking is updated */
	gtkspell = gtkspell_get_from_text_view (diary->entry_view);
	gtkspell_recheck_all (gtkspell);

	/* List the entry's links */
	gtk_list_store_clear (diary->links_store);
	links = diary_storage_manager_get_entry_links (diary->storage_manager, year, month, day);

	i = 0;
	while (links[i] != NULL) {
		link_type = diary_link_get_type (links[i]->type);

		if (link_type != NULL) {
			gtk_list_store_append (diary->links_store, &iter);
			gtk_list_store_set (diary->links_store, &iter,
					    0, links[i]->type,
					    1, links[i]->value,
					    2, links[i]->value2,
					    3, link_type->icon_name,
					    -1);
		}

		g_free (links[i]->type);
		g_free (links[i]->value);
		g_free (links[i]->value2);
		g_slice_free (DiaryLink, links[i]);

		i++;
	}

	g_free (links);
}

void
mw_links_selection_changed_cb (GtkTreeSelection *tree_selection, gpointer user_data)
{
	if (gtk_tree_selection_count_selected_rows (tree_selection) == 0) {
		gtk_widget_set_sensitive (GTK_WIDGET (diary->remove_button), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (diary->view_button), FALSE);
		gtk_action_set_sensitive (diary->remove_action, FALSE);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (diary->remove_button), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (diary->view_button), TRUE);
		gtk_action_set_sensitive (diary->remove_action, TRUE);
	}
}

void
mw_links_value_data_cb (GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	gchar *new_value;
	DiaryLink link;

	gtk_tree_model_get (model, iter, 0, &(link.type), 1, &(link.value), 2, &(link.value2), -1);

	new_value = diary_link_format_value (&link);
	g_object_set (renderer, "text", new_value, NULL);
	g_free (new_value);

	g_free (link.type);
	g_free (link.value);
	g_free (link.value2);
}

void
mw_links_tree_view_row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	DiaryLink link;
	GtkTreeIter iter;

	gtk_tree_model_get_iter (GTK_TREE_MODEL (diary->links_store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (diary->links_store), &iter, 0, &(link.type), 1, &(link.value), 2, &(link.value2), -1);

	/* NOTE: Link types should display their own errors, so one won't be displayed here. */
	diary_link_view (&link);

	g_free (link.type);
	g_free (link.value);
	g_free (link.value2);
}

gboolean
mw_entry_view_focus_out_event_cb (GtkWidget *entry_view, GdkEventFocus *event, gpointer user_data)
{
	save_current_entry ();
	return FALSE;
}

void
mw_add_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	add_link_to_current_entry ();
}

void
mw_remove_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	remove_link_from_current_entry ();
}

void
mw_view_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	DiaryLink link;
	GtkTreeIter iter;
	GList *links;
	GtkTreeModel *model;

	links = gtk_tree_selection_get_selected_rows (diary->links_selection, &model);

	for (; links != NULL; links = links->next) {
		gtk_tree_model_get_iter (model, &iter, (GtkTreePath*) links->data);
		gtk_tree_model_get (model, &iter, 0, &(link.type), 1, &(link.value), 2, &(link.value2), -1);

		/* NOTE: Link types should display their own errors, so one won't be displayed here. */
		diary_link_view (&link);

		g_free (link.type);
		g_free (link.value);
		g_free (link.value2);

		gtk_tree_path_free (links->data);
	}
	g_list_free (links);
}
