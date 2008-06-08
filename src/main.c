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
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gconf/gconf.h>

#include "main.h"
#include "storage-manager.h"
#include "interface.h"

void
diary_quit (void)
{
	diary->quitting = TRUE;
	diary_storage_manager_disconnect (diary->storage_manager);

	gtk_widget_destroy (diary->add_link_dialog);
	gtk_widget_destroy (diary->search_dialog);
	gtk_widget_destroy (diary->main_window);

	/* Quitting is actually done in the idle handler for disconnection
	 * which calls diary_quit_real. */
}

void
diary_quit_real (void)
{
	g_object_unref (diary->storage_manager);
	g_object_unref (diary->gconf_client);
	g_free (diary);

	if (gtk_main_level () > 0)
		gtk_main_quit ();

	exit (0);
}

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	gboolean debug = FALSE;
	gchar *db_filename;

	const GOptionEntry options[] = {
		{ "debug", 0, 0, G_OPTION_ARG_NONE, &debug, N_("Enable debug mode"), NULL },
		{ NULL }
	};

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	gtk_set_locale ();
	gtk_init (&argc, &argv);
	g_set_application_name (_("Diary"));
	gtk_window_set_default_icon_name ("diary");

	/* Options */
	context = g_option_context_new (_("- Manage your diary"));
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	if (g_option_context_parse (context, &argc, &argv, &error) == FALSE) {
		/* Show an error */
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				_("Command-line options could not be parsed. Error: %s"), error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
		exit (1);
	}

	g_option_context_free (context);

	/* Setup */
	diary = g_new (Diary, 1);
	diary->debug = debug;
	diary->quitting = FALSE;

	/* Open GConf */
	diary->gconf_client = gconf_client_get_default ();

	/* Open the DB */
	db_filename = g_build_filename (g_get_user_data_dir (), "diary.db", NULL);
	diary->storage_manager = diary_storage_manager_new (db_filename);
	g_free (db_filename);

	diary_storage_manager_connect (diary->storage_manager);

	/* Create and show the interface */
	diary_create_interface ();
	gtk_widget_show_all (diary->main_window);

	gtk_main ();
	return 0;
}
