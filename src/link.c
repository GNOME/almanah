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

#include "main.h"
#include "link.h"

#define LINK_TYPE(T) gchar *link_##T##_format_value (const DiaryLink *link); \
gboolean link_##T##_view (const DiaryLink *link); \
void link_##T##_build_dialog (const gchar *type, GtkTable *dialog_table); \
void link_##T##_get_values (DiaryLink *link);

LINK_TYPE (email)
LINK_TYPE (uri)

/*
 * IMPORTANT:
 * Make sure this list is kept in alphabetical order by type.
 *
 * To add a link type, add an entry here then add a file in src/links
 * named after the link type, containing the format, view, dialogue-building
 * and value-getting functions referenced in this table.
 * Don't forget to add the function prototypes at the top of *this* file.
 */
static const DiaryLinkType link_types[] = {
	/* Type,	Name,		Description,				Icon,				Columns,	Format function,		View function,		Dialogue build function,	Get values function */
	/*{ "email", 	N_("E-mail"),	N_("An e-mail you sent or received."),	"mail-read",			2,		&link_email_format_value,	&link_email_view,	&link_email_build_dialog,	&link_email_get_values },*/
	{ "uri", 	N_("URI"),	N_("A URI of a file or web page."),	"applications-internet",	1,		&link_uri_format_value,		&link_uri_view,		&link_uri_build_dialog,		&link_uri_get_values }
};

void
diary_populate_link_model (GtkListStore *list_store, guint type_column, guint name_column, guint icon_name_column)
{
	GtkTreeIter iter;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (link_types); i++) {
		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter,
				    type_column, link_types[i].type,
				    name_column, link_types[i].name,
				    icon_name_column, link_types[i].icon_name,
				    -1);
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

	/* TODO: perhaps use GQuarks to make things less heavy on the strcmps */
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
diary_link_format_value (const DiaryLink *link)
{
	const DiaryLinkType *link_type = diary_link_get_type (link->type);
	g_assert (link_type != NULL);
	return link_type->format_value_func (link);
}

gboolean
diary_link_view (const DiaryLink *link)
{
	const DiaryLinkType *link_type = diary_link_get_type (link->type);
	g_assert (link_type != NULL);
	return link_type->view_func (link);
}

void
diary_link_build_dialog (const DiaryLinkType *link_type)
{
	g_assert (link_type != NULL);
	link_type->build_dialog_func (link_type->type, diary->ald_table);
}

void
diary_link_get_values (DiaryLink *link)
{
	const DiaryLinkType *link_type = diary_link_get_type (link->type);
	g_assert (link_type != NULL);
	link_type->get_values_func (link);
}
