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
#include <math.h>
#include <glib/gi18n.h>
#include <string.h>

#include "link.h"

/*
 * IMPORTANT:
 * Make sure this list is kept in alphabetical order by type.
 *
 * To add a link type, add an entry here then add cases for the
 * link type in diary_link_format_value_for_display and
 * diary_link_view.
 */
static const DiaryLinkType link_types[] = {
	/* Type,   Name,         Description,                           Icon,        Columns */
	{ "email", N_("E-mail"), N_("An e-mail you sent or received."), "mail-read", 2 },	/* Columns: client (e.g. "evolution"), ID (e.g. "123098" --- specific to client) */
	{ "uri", N_("URI"), N_("A URI of a file or web page."), "system-file-manager", 1 }	/* Columns: URI (e.g. "http://google.com/") */
};

void
diary_populate_link_model (GtkListStore *list_store, guint type_column, guint name_column)
{
	GtkTreeIter iter;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (link_types); i++) {
		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter, type_column, link_types[i].type, name_column, link_types[i].name, -1);
	}
}

gboolean
diary_validate_link_type (const gchar *type)
{
	return (diary_link_get_type (type) == NULL) ? FALSE : TRUE;
}

const DiaryLinkType *
diary_link_get_type (const gchar *type)
{
	guint lower_limit, upper_limit, temp;
	gint comparison;

	/* Do a binary search */
	lower_limit = 0;
	upper_limit = G_N_ELEMENTS (link_types) - 1;

	do {
		temp = ceil ((lower_limit + upper_limit) / 2);
		comparison = strcmp (type, link_types[temp].type);

		/* Exit condition */
		if (lower_limit == upper_limit && comparison != 0)
			return NULL;

		if (comparison < 0)
			upper_limit = temp - 1; /* It's in the lower half */
		else if (comparison > 0)
			lower_limit = temp + 1; /* It's in the upper half */
		else
			return &(link_types[temp]); /* Match! */
	} while (TRUE);

	return NULL;
}

gchar *
diary_link_format_value_for_display (DiaryLink *link)
{
	/* TODO: perhaps use GQuarks to make things less heavy on the strcmps */
	if (strcmp (link->type, "email") == 0) {
		/* TODO: get and display e-mail subject? */
		return g_strdup (_("E-mail"));
	} else {
		return g_strdup (link->value);
	}
}

void
diary_link_view (DiaryLink *link)
{
	/* TODO: GQuarks? */
	if (strcmp (link->type, "email") == 0) {
		/* TODO */
	} else if (strcmp (link->type, "uri") == 0) {
		g_app_info_launch_default_for_uri   (link->value, NULL, NULL);
	} else {
		/* Eck! */
		g_warning ("Tried to view unhandled link type '%s'.", link->type);
	}
}
