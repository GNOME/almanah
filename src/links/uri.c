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

#include "uri.h"
#include "../link.h"
#include "../interface.h"
#include "../main.h"

static void almanah_uri_link_init (AlmanahURILink *self);
static gchar *uri_format_value (AlmanahLink *link);
static gboolean uri_view (AlmanahLink *link);
static void uri_build_dialog (AlmanahLink *link, GtkVBox *parent_vbox);
static void uri_get_values (AlmanahLink *link);

struct _AlmanahURILinkPrivate {
	GtkWidget *entry;
};

G_DEFINE_TYPE (AlmanahURILink, almanah_uri_link, ALMANAH_TYPE_LINK)
#define ALMANAH_URI_LINK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_URI_LINK, AlmanahURILinkPrivate))

static void
almanah_uri_link_class_init (AlmanahURILinkClass *klass)
{
	AlmanahLinkClass *link_class = ALMANAH_LINK_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahURILinkPrivate));

	link_class->type_id = "uri";
	link_class->name = _("URI");
	link_class->description = _("A URI of a file or web page.");
	link_class->icon_name = "applications-internet";

	link_class->format_value = uri_format_value;
	link_class->view = uri_view;
	link_class->build_dialog = uri_build_dialog;
	link_class->get_values = uri_get_values;
}

static void
almanah_uri_link_init (AlmanahURILink *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_URI_LINK, AlmanahURILinkPrivate);
}

static gchar *
uri_format_value (AlmanahLink *link)
{
	return g_strdup (almanah_link_get_value (link));
}

static gboolean
uri_view (AlmanahLink *link)
{
	GError *error = NULL;
	const gchar *value = almanah_link_get_value (link);

	if (gtk_show_uri (gtk_widget_get_screen (almanah->main_window), value, GDK_CURRENT_TIME, &error) == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (almanah->main_window),
							    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							    _("Error opening URI"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);

		return FALSE;
	}

	return TRUE;
}

static void
uri_build_dialog (AlmanahLink *link, GtkVBox *parent_vbox)
{
	GtkWidget *label, *hbox;
	AlmanahURILinkPrivate *priv = ALMANAH_URI_LINK (link)->priv;

	hbox = gtk_hbox_new (FALSE, 5);

	label = gtk_label_new (_("URI"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

	priv->entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (priv->entry), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), priv->entry, TRUE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (parent_vbox), hbox, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (parent_vbox));
}

static void
uri_get_values (AlmanahLink *link)
{
	AlmanahURILinkPrivate *priv = ALMANAH_URI_LINK (link)->priv;

	almanah_link_set_value (link, gtk_entry_get_text (GTK_ENTRY (priv->entry)));
	almanah_link_set_value2 (link, NULL);
}
