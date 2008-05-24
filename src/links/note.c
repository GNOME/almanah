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

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "../interface.h"
#include "../main.h"
#include "../link.h"

gchar *
link_note_format_value (const DiaryLink *link)
{
	return g_strdup (link->value);
}

gboolean
link_note_view (const DiaryLink *link)
{
	GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (diary->main_window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				link->value);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return TRUE;
}

void
link_note_build_dialog (const gchar *type, GtkTable *dialog_table)
{
	GtkWidget *text_view, *scrolled_window;

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
					     GTK_SHADOW_IN);
	text_view = gtk_text_view_new ();
	gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);

	gtk_table_attach_defaults (dialog_table, scrolled_window, 1, 3, 2, 3);
	gtk_widget_show_all (GTK_WIDGET (dialog_table));

	g_object_set_data (G_OBJECT (diary->add_link_dialog), "text-view", text_view);
}

void
link_note_get_values (DiaryLink *link)
{
	GtkTextView *text_view;
	GtkTextBuffer *buffer;
	GtkTextIter start_iter, end_iter;

	text_view = GTK_TEXT_VIEW (g_object_get_data (G_OBJECT (diary->add_link_dialog), "text-view"));
	buffer = gtk_text_view_get_buffer (text_view);
	gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);

	link->value = gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);
	link->value2 = NULL;
}
