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

#include "../link.h"

gchar *
link_email_format_value (const DiaryLink *link)
{
	/* TODO: Show the message subject, or something? */

	return g_strdup (_("E-mail"));
}

gboolean
link_email_view (const DiaryLink *link)
{
	/* TODO */
	return TRUE;
}

void
link_email_build_dialog (const gchar *type, GtkTable *dialog_table)
{
	/* TODO */
}

void
link_email_get_values (DiaryLink *link)
{
	/* TODO */
}
