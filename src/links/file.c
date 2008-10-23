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

#include <glib.h>
#include <glib/gi18n.h>

#include "file.h"
#include "../link.h"
#include "../interface.h"
#include "../main.h"

static void almanah_file_link_init (AlmanahFileLink *self);
static gchar *file_format_value (AlmanahLink *link);
static gboolean file_view (AlmanahLink *link);
static void file_build_dialog (AlmanahLink *link, GtkVBox *parent_vbox);
static void file_get_values (AlmanahLink *link);

struct _AlmanahFileLinkPrivate {
	GtkWidget *chooser;
};

G_DEFINE_TYPE (AlmanahFileLink, almanah_file_link, ALMANAH_TYPE_LINK)
#define ALMANAH_FILE_LINK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_FILE_LINK, AlmanahFileLinkPrivate))

static void
almanah_file_link_class_init (AlmanahFileLinkClass *klass)
{
	AlmanahLinkClass *link_class = ALMANAH_LINK_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahFileLinkPrivate));

	link_class->type_id = "file";
	link_class->name = _("File");
	link_class->description = _("An attached file.");
	link_class->icon_name = "system-file-manager";

	link_class->format_value = file_format_value;
	link_class->view = file_view;
	link_class->build_dialog = file_build_dialog;
	link_class->get_values = file_get_values;
}

static void
almanah_file_link_init (AlmanahFileLink *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_FILE_LINK, AlmanahFileLinkPrivate);
}

static gchar *
file_format_value (AlmanahLink *link)
{
	return almanah_link_get_value (link);
}

static gboolean
file_view (AlmanahLink *link)
{
	GError *error = NULL;
	gchar *value = almanah_link_get_value (link);

	if (g_app_info_launch_default_for_uri (value, NULL, &error) == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (almanah->main_window),
							    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							    _("Error opening file"));
		gtk_message_dialog_format_secondary_text (GTK_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_free (value);
		g_error_free (error);

		return FALSE;
	}

	g_free (value);
	return TRUE;
}

static void
file_build_dialog (AlmanahLink *link, GtkVBox *parent_vbox)
{
	AlmanahFileLinkPrivate *priv = ALMANAH_FILE_LINK (link)->priv;

	priv->chooser = gtk_file_chooser_button_new (_("Select File"), GTK_FILE_CHOOSER_ACTION_OPEN);

	gtk_box_pack_start (GTK_BOX (parent_vbox), priv->chooser, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (parent_vbox));
}

static void
file_get_values (AlmanahLink *link)
{
	AlmanahFileLinkPrivate *priv = ALMANAH_FILE_LINK (link)->priv;
	gchar *value = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (priv->chooser));

	almanah_link_set_value (link, value);
	g_free (value);

	almanah_link_set_value2 (link, NULL);
}
