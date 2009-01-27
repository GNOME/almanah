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
#include "add-definition-dialog.h"
#include "search-dialog.h"
#ifdef ENABLE_ENCRYPTION
#include "preferences-dialog.h"
#endif /* ENABLE_ENCRYPTION */
#include "interface.h"

const gchar *
almanah_get_interface_filename (void)
{
	if (g_file_test ("./data/almanah.ui", G_FILE_TEST_EXISTS) == TRUE)
		return "./data/almanah.ui";
	else
		return PACKAGE_DATA_DIR"/almanah/almanah.ui";
}

void
almanah_interface_embolden_label (GtkLabel *label)
{
	gchar *markup;

	markup = g_strdup_printf ("<b>%s</b>", gtk_label_get_label (label));
	gtk_label_set_markup_with_mnemonic (label, markup);
	g_free (markup);
}

static gboolean
definition_tag_event_cb (GtkTextTag *tag, GObject *object, GdkEvent *event, GtkTextIter *iter, gpointer user_data)
{
	AlmanahDefinition *definition;
	gchar *text;
	GtkTextIter start_iter, end_iter;

	/* TODO: Display a popup menu on right-clicking? Display a list of definitions, or allow this one to be edited, when Ctrl clicking? */
	/* Handle only double- or control-click events on any definition tags, so they can act like hyperlinks */
	if ((event->type != GDK_BUTTON_RELEASE && event->type != GDK_2BUTTON_PRESS) ||
	    (event->type == GDK_BUTTON_RELEASE && !(event->button.state & GDK_CONTROL_MASK))) {
		return FALSE;
	}

	/* Get the start and end iters for this tag instance */
	start_iter = *iter;
	if (gtk_text_iter_backward_to_tag_toggle (&start_iter, tag) == FALSE)
		start_iter = *iter;

	end_iter = start_iter;
	if (gtk_text_iter_forward_to_tag_toggle (&end_iter, tag) == FALSE)
		end_iter = *iter;

	/* Get the tag's text */
	text = gtk_text_iter_get_text (&start_iter, &end_iter);
	definition = almanah_storage_manager_get_definition (almanah->storage_manager, text);
	g_free (text);

	if (definition == NULL) {
		/* If the definition no longer exists, remove the tag */
		gtk_text_buffer_remove_tag (gtk_text_iter_get_buffer (iter), tag, &start_iter, &end_iter);
		return FALSE;
	}

	return almanah_definition_view (definition);
}

void
almanah_interface_create_text_tags (GtkTextBuffer *text_buffer, gboolean connect_events)
{
	GtkTextTag *tag;

	gtk_text_buffer_create_tag (text_buffer, "bold", 
				    "weight", PANGO_WEIGHT_BOLD, 
				    NULL);
	gtk_text_buffer_create_tag (text_buffer, "italic",
				    "style", PANGO_STYLE_ITALIC,
				    NULL);
	gtk_text_buffer_create_tag (text_buffer, "underline",
				    "underline", PANGO_UNDERLINE_SINGLE,
				    NULL);
	tag = gtk_text_buffer_create_tag (text_buffer, "definition",
					  "foreground", "blue",
					  "underline", PANGO_UNDERLINE_SINGLE,
					  NULL);

	if (connect_events == TRUE)
		g_signal_connect (tag, "event", G_CALLBACK (definition_tag_event_cb), NULL);
}

/* TODO: This exists so that different calendars can be highlighted according to which days have entries
 * (i.e. the ones on the print dialogue). This should eventually be replaced by a custom calendar widget. */
void
almanah_calendar_month_changed_cb (GtkCalendar *calendar, gpointer user_data)
{
	/* Mark the days on the calendar which have diary entries */
	guint i, year, month, num_days;
	gboolean *days;

	gtk_calendar_get_date (calendar, &year, &month, NULL);
	month++;
	days = almanah_storage_manager_get_month_marked_days (almanah->storage_manager, year, month, &num_days);

	gtk_calendar_clear_marks (calendar);
	for (i = 0; i < num_days; i++) {
		if (days[i] == TRUE)
			gtk_calendar_mark_day (calendar, i + 1);
		else
			gtk_calendar_unmark_day (calendar, i + 1);
	}

	g_free (days);
}
