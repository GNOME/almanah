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

#include "main-window.h"
#include "search-dialog.h"
#include "interface.h"

void
almanah_interface_create_text_tags (GtkTextBuffer *text_buffer, gboolean connect_events)
{
	GtkTextTagTable *table;

	table = gtk_text_buffer_get_tag_table (text_buffer);
	if (gtk_text_tag_table_lookup (table, "gtkspell-misspelled") == NULL) {
		/* Create a dummy gtkspell-misspelled tag to stop errors about an unknown tag appearing
		 * when deserialising content which has misspellings highlighted, but without GtkSpell enabled */
		gtk_text_buffer_create_tag (text_buffer, "gtkspell-misspelled", NULL);
	}

	if (gtk_text_tag_table_lookup (table, "definition") == NULL) {
		/* Same for definitions (which have been removed from Almanah) */
		gtk_text_buffer_create_tag (text_buffer, "definition", NULL);
	}

	gtk_text_buffer_create_tag (text_buffer, "bold", 
				    "weight", PANGO_WEIGHT_BOLD, 
				    NULL);
	gtk_text_buffer_create_tag (text_buffer, "italic",
				    "style", PANGO_STYLE_ITALIC,
				    NULL);
	gtk_text_buffer_create_tag (text_buffer, "underline",
				    "underline", PANGO_UNDERLINE_SINGLE,
				    NULL);
}

gboolean
almanah_run_on_screen (GdkScreen *screen, const gchar *command_line, GError **error)
{
	gboolean retval;
	GAppInfo *app_info;
	GdkAppLaunchContext *context;

	app_info = g_app_info_create_from_commandline (command_line,
	                                               "Almanah Execute",
	                                               G_APP_INFO_CREATE_NONE,
	                                               error);

	if (app_info == NULL) {
		return FALSE;
	}

	context = gdk_display_get_app_launch_context (gdk_screen_get_display (screen));
	gdk_app_launch_context_set_screen (context, screen);

	retval = g_app_info_launch (app_info, NULL, G_APP_LAUNCH_CONTEXT (context), error);

	g_object_unref (context);
	g_object_unref (app_info);

	return retval;
}
