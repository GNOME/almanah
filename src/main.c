/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2008-2009 <philip@tecnocode.co.uk>
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

#include "main.h"
#include "application.h"

gboolean
almanah_run_on_screen (GdkScreen *screen, const gchar *commandline, GError **error)
{
	gboolean retval;
	GAppInfo *appinfo;
	GdkAppLaunchContext *context;

	appinfo = g_app_info_create_from_commandline (commandline,
                                                "Almanah Execute",
                                                G_APP_INFO_CREATE_NONE,
                                                error);

	if (!appinfo)
		return FALSE;

	context = gdk_display_get_app_launch_context (gdk_screen_get_display (screen));
	gdk_app_launch_context_set_screen (context, screen);

	retval = g_app_info_launch (appinfo, NULL, G_APP_LAUNCH_CONTEXT (context), error);

	g_object_unref (context);
	g_object_unref (appinfo);

	return retval;
}

int
main (int argc, char *argv[])
{
	AlmanahApplication *application;
	int status;

	g_thread_init (NULL);
	g_type_init ();

	application = almanah_application_new ();
	status = g_application_run (G_APPLICATION (application), argc, argv);
	g_object_unref (application);

	return status;
}
