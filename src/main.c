/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2008, 2009, 2011 <philip@tecnocode.co.uk>
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
#include <glib.h>
#include <gio/gio.h>
#include <gtksourceview/gtksource.h>

#include "application.h"

int
main (int argc, char *argv[])
{
	AlmanahApplication *application;
	int status;

	gtk_source_init ();

	application = almanah_application_new ();
	status = g_application_run (G_APPLICATION (application), argc, argv);
	g_object_unref (application);

	gtk_source_finalize ();

	return status;
}
