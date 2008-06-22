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
link_file_format_value (const DiaryLink *link)
{
	return g_strdup (link->value);
}

gboolean
link_file_view (const DiaryLink *link)
{
	if (g_app_info_launch_default_for_uri (link->value, NULL, NULL) == FALSE) {
		diary_interface_error (_("Due to an unknown error the file cannot be opened."), diary->main_window);
		return FALSE;
	}
	return TRUE;
}

void
link_file_build_dialog (const gchar *type, GtkTable *dialog_table)
{
	GtkWidget *chooser;

	chooser = gtk_file_chooser_button_new (_("Select File"), GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_table_attach_defaults (dialog_table, chooser, 1, 3, 2, 3);
	gtk_widget_show_all (GTK_WIDGET (dialog_table));

	g_object_set_data (G_OBJECT (diary->add_link_dialog), "chooser", chooser);
}

void
link_file_get_values (DiaryLink *link)
{
	GtkFileChooser *chooser = GTK_FILE_CHOOSER (g_object_get_data (G_OBJECT (diary->add_link_dialog), "chooser"));
	link->value = gtk_file_chooser_get_uri (chooser);
	link->value2 = NULL;
}
