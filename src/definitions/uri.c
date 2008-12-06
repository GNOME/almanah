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
#include <atk/atk.h>

#include "uri.h"
#include "../definition.h"
#include "../interface.h"
#include "../main.h"

static void almanah_uri_definition_init (AlmanahURIDefinition *self);
static gboolean uri_view (AlmanahDefinition *definition);
static void uri_build_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox);
static void uri_close_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox);
static void uri_parse_text (AlmanahDefinition *definition, const gchar *text);

struct _AlmanahURIDefinitionPrivate {
	GtkWidget *entry;
};

G_DEFINE_TYPE (AlmanahURIDefinition, almanah_uri_definition, ALMANAH_TYPE_DEFINITION)
#define ALMANAH_URI_DEFINITION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_URI_DEFINITION, AlmanahURIDefinitionPrivate))

static void
almanah_uri_definition_class_init (AlmanahURIDefinitionClass *klass)
{
	AlmanahDefinitionClass *definition_class = ALMANAH_DEFINITION_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahURIDefinitionPrivate));

	definition_class->type_id = ALMANAH_DEFINITION_URI;
	definition_class->name = _("URI");
	definition_class->description = _("A URI of a file or web page.");
	definition_class->icon_name = "applications-internet";

	definition_class->view = uri_view;
	definition_class->build_dialog = uri_build_dialog;
	definition_class->close_dialog = uri_close_dialog;
	definition_class->parse_text = uri_parse_text;
}

static void
almanah_uri_definition_init (AlmanahURIDefinition *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_URI_DEFINITION, AlmanahURIDefinitionPrivate);
}

static gboolean
uri_view (AlmanahDefinition *definition)
{
	GError *error = NULL;
	const gchar *value = almanah_definition_get_value (definition);

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
uri_build_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox)
{
	GtkWidget *label, *hbox;
	AtkObject *a11y_label, *a11y_entry;
	AlmanahURIDefinitionPrivate *priv = ALMANAH_URI_DEFINITION (definition)->priv;
	const gchar *value = almanah_definition_get_value (definition);

	hbox = gtk_hbox_new (FALSE, 5);

	label = gtk_label_new (_("URI"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

	priv->entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (priv->entry), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), priv->entry, TRUE, TRUE, 0);

	/* Set up the accessibility relationships */
	a11y_label = gtk_widget_get_accessible (GTK_WIDGET (label));
	a11y_entry = gtk_widget_get_accessible (GTK_WIDGET (priv->entry));
	atk_object_add_relationship (a11y_label, ATK_RELATION_LABEL_FOR, a11y_entry);
	atk_object_add_relationship (a11y_entry, ATK_RELATION_LABELLED_BY, a11y_label);

	/* Initialise the dialogue with the definition's current values */
	if (value != NULL)
		gtk_entry_set_text (GTK_ENTRY (priv->entry), value);

	gtk_box_pack_start (GTK_BOX (parent_vbox), hbox, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (parent_vbox));
}

static void
uri_close_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox)
{
	AlmanahURIDefinitionPrivate *priv = ALMANAH_URI_DEFINITION (definition)->priv;

	almanah_definition_set_value (definition, gtk_entry_get_text (GTK_ENTRY (priv->entry)));
	almanah_definition_set_value2 (definition, NULL);
}

static void
uri_parse_text (AlmanahDefinition *definition, const gchar *text)
{
	/* TODO */
}
