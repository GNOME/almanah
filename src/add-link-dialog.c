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

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

#include "main.h"
#include "link.h"
#include "add-link-dialog.h"

static void
destroy_extra_table_widgets (void)
{
	GList *children;

	/* Destroy the foreign children */
	children = gtk_container_get_children (GTK_CONTAINER (diary->ald_table));
	for (; children != NULL; children = children->next) {
		if (g_object_get_data (G_OBJECT (children->data), "native") == NULL)
			gtk_widget_destroy (GTK_WIDGET (children->data));
	}
	g_list_free (children);

	/* Resize the table */
	gtk_table_resize (diary->ald_table, 1, 2);
}

void
diary_add_link_dialog_setup (GtkBuilder *builder)
{
	g_object_set_data (gtk_builder_get_object (builder, "dry_ald_type_label"), "native", GUINT_TO_POINTER (TRUE));
	g_object_set_data (G_OBJECT (diary->ald_type_combo_box), "native", GUINT_TO_POINTER (TRUE));

	diary_populate_link_model (diary->ald_type_store, 1, 0, 2);
	gtk_combo_box_set_active (diary->ald_type_combo_box, 0);
}

void
diary_hide_ald (void)
{
	/* Resize the table so we lose all the custom widgets */
	gtk_widget_hide_all (diary->add_link_dialog);
	destroy_extra_table_widgets ();
}

void
ald_destroy_cb (GtkWindow *window, gpointer user_data)
{
	diary_hide_ald ();
}

void
ald_type_combo_box_changed_cb (GtkComboBox *self, gpointer user_data)
{
	GtkTreeIter iter;
	gchar *type;
	const DiaryLinkType *link_type;

	destroy_extra_table_widgets ();

	if (gtk_combo_box_get_active_iter (self, &iter) == FALSE)
		return;

	gtk_tree_model_get (gtk_combo_box_get_model (self), &iter, 1, &type, -1);
	link_type = diary_link_get_type (type);
	gtk_table_resize (diary->ald_table, link_type->columns + 1, 2);

	g_assert (link_type != NULL);
	link_type->build_dialog_func (type, diary->ald_table);

	g_free (type);
}
