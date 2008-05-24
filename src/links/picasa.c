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

/*
 * value: Album name
 * value2: Google username
 */

gchar *
link_picasa_format_value (const DiaryLink *link)
{
	/* Translators: First argument is the album name, second is the Google username. */
	return g_strdup_printf (_("Picasa: %s by %s"), link->value2, link->value);
}

gboolean
link_picasa_view (const DiaryLink *link)
{
	gboolean return_value;
	gchar *uri = g_strconcat ("http://picasaweb.google.com/", link->value2, "/", link->value, NULL);

	return_value = g_app_info_launch_default_for_uri (uri, NULL, NULL);

	if (return_value == FALSE)
		diary_interface_error (_("Due to an unknown error the URI cannot be opened."), diary->main_window);

	g_free (uri);

	return return_value;
}

void
link_picasa_build_dialog (const gchar *type, GtkTable *dialog_table)
{
	GtkWidget *label1, *entry1, *label2, *entry2;

	/* Google username (value2) */
	label1 = gtk_label_new (_("Google Username"));
	gtk_misc_set_alignment (GTK_MISC (label1), 0.0, 0.5);

	entry1 = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (entry1), TRUE);

	gtk_table_attach_defaults (dialog_table, label1, 1, 2, 2, 3);
	gtk_table_attach_defaults (dialog_table, entry1, 2, 3, 2, 3);

	/* Album name (value) */
	label2 = gtk_label_new (_("Album Name"));
	gtk_misc_set_alignment (GTK_MISC (label2), 0.0, 0.5);

	entry2 = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (entry2), TRUE);

	gtk_table_attach_defaults (dialog_table, label2, 1, 2, 3, 4);
	gtk_table_attach_defaults (dialog_table, entry2, 2, 3, 3, 4);

	gtk_widget_show_all (GTK_WIDGET (dialog_table));

	g_object_set_data (G_OBJECT (diary->add_link_dialog), "entry1", entry1);
	g_object_set_data (G_OBJECT (diary->add_link_dialog), "entry2", entry2);
}

void
link_picasa_get_values (DiaryLink *link)
{
	GtkEntry *entry1 = GTK_ENTRY (g_object_get_data (G_OBJECT (diary->add_link_dialog), "entry1"));
	GtkEntry *entry2 = GTK_ENTRY (g_object_get_data (G_OBJECT (diary->add_link_dialog), "entry2"));

	link->value = g_strdup (gtk_entry_get_text (entry1));
	link->value2 = g_strdup (gtk_entry_get_text (entry2));
}
