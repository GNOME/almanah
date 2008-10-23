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
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "main.h"
#include "main-window.h"
#include "add-link-dialog.h"
#include "search-dialog.h"
#include "preferences-dialog.h"
#include "interface.h"

const gchar *
almanah_get_interface_filename (void)
{
	if (g_file_test ("./data/almanah.ui", G_FILE_TEST_EXISTS) == TRUE)
		return "./data/almanah.ui";
	else
		return PACKAGE_DATA_DIR"/almanah/almanah.ui";
}

GtkWidget *
almanah_create_interface (void)
{
	almanah->main_window = GTK_WIDGET (almanah_main_window_new ());
	almanah->add_link_dialog = GTK_WIDGET (almanah_add_link_dialog_new ());
	almanah->search_dialog = GTK_WIDGET (almanah_search_dialog_new ());
	almanah->preferences_dialog = GTK_WIDGET (almanah_preferences_dialog_new ());

	return almanah->main_window;
}

void
almanah_interface_embolden_label (GtkLabel *label)
{
	gchar *markup;

	markup = g_strdup_printf ("<b>%s</b>", gtk_label_get_label (label));
	gtk_label_set_markup_with_mnemonic (label, markup);
	g_free (markup);
}

/* TODO: This exists so that different calendars can be highlighted according to which days have entries
 * (i.e. the ones on the print dialogue). This should eventually be replaced by a custom calendar widget. */
void
almanah_calendar_month_changed_cb (GtkCalendar *calendar, gpointer user_data)
{
	/* Mark the days on the calendar which have diary entries */
	guint i, year, month;
	gboolean *days;

	gtk_calendar_get_date (calendar, &year, &month, NULL);
	month++;
	days = almanah_storage_manager_get_month_marked_days (almanah->storage_manager, year, month);

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
