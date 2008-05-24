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
#include <gio/gio.h>

#include "../interface.h"
#include "../main.h"
#include "../link.h"

gchar *
link_uri_format_value (const DiaryLink *link)
{
	return g_strdup (link->value);
}

gboolean
link_uri_view (const DiaryLink *link)
{
	if (g_app_info_launch_default_for_uri (link->value, NULL, NULL) == FALSE) {
		diary_interface_error (_("Due to an unknown error the URI cannot be opened."), diary->main_window);
		return FALSE;
	}
	return TRUE;
}

void
link_uri_build_dialog (const gchar *type, GtkTable *dialog_table)
{
	GtkWidget *label, *entry;

	label = gtk_label_new (_("URI"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

	entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

	gtk_table_attach_defaults (dialog_table, label, 1, 2, 2, 3);
	gtk_table_attach_defaults (dialog_table, entry, 2, 3, 2, 3);

	gtk_widget_show_all (GTK_WIDGET (dialog_table));

	g_object_set_data (G_OBJECT (diary->add_link_dialog), "entry", entry);
}

void
link_uri_get_values (DiaryLink *link)
{
	GtkEntry *entry = GTK_ENTRY (g_object_get_data (G_OBJECT (diary->add_link_dialog), "entry"));
	link->value = g_strdup (gtk_entry_get_text (entry));
	link->value2 = NULL;
}
