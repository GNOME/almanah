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
#include "storage-manager.h"
#include "main-window.h"

static void save_current_diary_entry ();

static void
save_current_diary_entry ()
{
	GtkTextIter start_iter, end_iter;
	gchar *entry_text;
	guint year, month, day;

	if (diary->entry_buffer == NULL)
		return;

	/* Save the entry */
	gtk_text_buffer_get_bounds (diary->entry_buffer, &start_iter, &end_iter);
	entry_text = gtk_text_buffer_get_text (diary->entry_buffer, &start_iter, &end_iter, FALSE);

	gtk_calendar_get_date (diary->calendar, &year, &month, &day);
	month++;

	diary_storage_manager_set_diary_entry (diary->storage_manager, year, month, day, entry_text);
	g_free (entry_text);

	/* Mark the day on the calendar */
	gtk_calendar_mark_day (diary->calendar, day);
}

void
mw_destroy_cb (GtkWindow *window, gpointer user_data)
{
	diary_quit ();
}

void
mw_quit_activate_cb (GtkAction *action, gpointer user_data)
{
	diary_quit ();
}

void
mw_about_activate_cb (GtkAction *action, gpointer user_data)
{
	gchar *license;
	const gchar *description = N_("A helpful diary keeper.");
	const gchar *authors[] =
	{
		"Philip Withnall <philip@tecnocode.co.uk>",
		NULL
	};
	const gchar *license_parts[] = {
		N_("Diary is free software; you can redistribute it and/or modify "
		   "it under the terms of the GNU General Public License as published by "
		   "the Free Software Foundation; either version 2 of the License, or "
		   "(at your option) any later version."),
		N_("Diary is distributed in the hope that it will be useful, "
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		   "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License "
		   "along with Diary; if not, write to the Free Software Foundation, Inc., "
		   "59 Temple Place, Suite 330, Boston, MA  02111-1307  USA"),
	};

	license = g_strjoin ("\n\n",
			  _(license_parts[0]),
			  _(license_parts[1]),
			  _(license_parts[2]),
			  NULL);

	gtk_show_about_dialog (GTK_WINDOW (diary->main_window),
				"version", VERSION,
				"copyright", _("Copyright \xc2\xa9 2008 Philip Withnall"),
				"comments", _(description),
				"authors", authors,
				"translator-credits", _("translator-credits"),
				"logo-icon-name", "diary",
				"license", license,
				"wrap-license", TRUE,
				"website-label", _("Diary Website"),
				"website", "http://tecnocode.co.uk?page=blog&action=view_item&id=60",
				NULL);

	g_free (license);
}

void
mw_calendar_day_selected_cb (GtkCalendar *calendar, gpointer user_data)
{
	GDate *calendar_date;
	gchar calendar_string[100], *entry_text;
	guint year, month, day;
	EntryLink **links;
	guint i;
	GtkTreeIter iter;

	/* Update the date label */
	gtk_calendar_get_date (calendar, &year, &month, &day);
	month++;
	calendar_date = g_date_new_dmy (day, month, year);

	/* TODO: Somewhat hacky */
	/* Translators: This is a strftime()-format string for the date displayed at the top of the main window. */
	g_date_strftime (calendar_string, sizeof (calendar_string), _("<b>%A, %e %B %Y</b>"), calendar_date);
	gtk_label_set_markup (diary->date_label, calendar_string);

	/* Update the entry */
	entry_text = diary_storage_manager_get_diary_entry (diary->storage_manager, year, month, day);

	if (entry_text != NULL) {
		gtk_text_buffer_set_text (diary->entry_buffer, entry_text, -1);
		gtk_widget_set_sensitive (GTK_WIDGET (diary->add_button), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (diary->remove_button), FALSE); /* Only sensitive if something's selected */
	} else {
		gtk_text_buffer_set_text (diary->entry_buffer, "", -1);
		gtk_widget_set_sensitive (GTK_WIDGET (diary->add_button), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (diary->remove_button), FALSE);
	}

	/* List the entry's links */
	gtk_list_store_clear (diary->links_store);
	links = diary_storage_manager_get_entry_links (diary->storage_manager, year, month, day);

	i = 0;
	while (links[i] != NULL) {
		gtk_list_store_append (diary->links_store, &iter);
		gtk_list_store_set (diary->links_store, &iter, 0, links[i]->type, 1, links[i]->value, -1);

		g_free (links[i]->type);
		g_free (links[i]->value);
		g_slice_free (EntryLink, links[i]);

		i++;
	}

	g_free (links);
}

void
mw_calendar_month_changed_cb (GtkCalendar *calendar, gpointer user_data)
{
	/* Mark the days on the calendar which have diary entries */
	guint i, year, month;
	gtk_calendar_get_date (calendar, &year, &month, NULL);
	month++;
	gboolean *days = diary_storage_manager_get_month_marked_days (diary->storage_manager, year, month);

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

void
mw_links_selection_changed_cb (GtkTreeSelection *tree_selection, gpointer user_data)
{
	if (gtk_tree_selection_count_selected_rows (tree_selection) == 0)
		gtk_widget_set_sensitive (GTK_WIDGET (diary->remove_button), FALSE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (diary->remove_button), TRUE);
}

void
mw_calendar_realize_cb (GtkWidget *widget, gpointer user_data)
{
	/* Select the current day and month */
	mw_calendar_month_changed_cb (GTK_CALENDAR (widget), user_data);
	mw_calendar_day_selected_cb (GTK_CALENDAR (widget), user_data);
}

void
mw_links_tree_view_realize_cb (GtkWidget *widget, gpointer user_data)
{
	g_signal_connect (diary->links_selection, "changed", (GCallback) mw_links_selection_changed_cb, NULL);
}

gboolean
mw_entry_view_focus_out_event_cb (GtkWidget *entry_view, GdkEventFocus *event, gpointer user_data)
{
	save_current_diary_entry ();
	return FALSE;
}

void
mw_add_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	guint year, month, day;

	gtk_widget_show_all (diary->add_link_dialog);
	if (gtk_dialog_run (GTK_DIALOG (diary->add_link_dialog)) == GTK_RESPONSE_OK) {
		/* Add the link to the DB */
		gtk_calendar_get_date (diary->calendar, &year, &month, &day);
		month++;
		diary_storage_manager_add_entry_link (diary->storage_manager,
						      year, month, day,
						      gtk_entry_get_text (diary->ald_type_entry),
						      gtk_entry_get_text (diary->ald_value_entry));
	}
	gtk_widget_hide_all (diary->add_link_dialog);
	gtk_entry_set_text (diary->ald_type_entry, "");
	gtk_entry_set_text (diary->ald_value_entry, "");
}

void
mw_remove_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	gchar *link_type;
	guint year, month, day;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GList *links = gtk_tree_selection_get_selected_rows (diary->links_selection, &model);
	gtk_calendar_get_date (diary->calendar, &year, &month, &day);
	month++;

	for (; links != NULL; links = links->next) {
		gtk_tree_model_get_iter (model, &iter, (GtkTreePath*) links->data);
		gtk_tree_model_get (model, &iter, 0, &link_type, -1);
		diary_storage_manager_remove_entry_link (diary->storage_manager, year, month, day, link_type);
		gtk_tree_path_free (links->data);
	}
	g_list_free (links);
}
