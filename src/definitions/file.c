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
#include "../definition.h"
#include "../main.h"

static void almanah_file_definition_init (AlmanahFileDefinition *self);
static gboolean file_view (AlmanahDefinition *definition);
static void file_build_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox);
static void file_close_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox);
static void file_parse_text (AlmanahDefinition *definition, const gchar *text);
static gchar *file_get_blurb (AlmanahDefinition *definition);

struct _AlmanahFileDefinitionPrivate {
	GtkWidget *chooser;
};

G_DEFINE_TYPE (AlmanahFileDefinition, almanah_file_definition, ALMANAH_TYPE_DEFINITION)
#define ALMANAH_FILE_DEFINITION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_FILE_DEFINITION, AlmanahFileDefinitionPrivate))

static void
almanah_file_definition_class_init (AlmanahFileDefinitionClass *klass)
{
	AlmanahDefinitionClass *definition_class = ALMANAH_DEFINITION_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahFileDefinitionPrivate));

	definition_class->type_id = ALMANAH_DEFINITION_FILE;
	definition_class->name = _("File");
	definition_class->description = _("An attached file.");
	definition_class->icon_name = "system-file-manager";

	definition_class->view = file_view;
	definition_class->build_dialog = file_build_dialog;
	definition_class->close_dialog = file_close_dialog;
	definition_class->parse_text = file_parse_text;
	definition_class->get_blurb = file_get_blurb;
}

static void
almanah_file_definition_init (AlmanahFileDefinition *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_FILE_DEFINITION, AlmanahFileDefinitionPrivate);
}

static gboolean
file_view (AlmanahDefinition *definition)
{
	GError *error = NULL;
	const gchar *value = almanah_definition_get_value (definition);

	if (gtk_show_uri (gtk_widget_get_screen (almanah->main_window), value, GDK_CURRENT_TIME, &error) == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (almanah->main_window),
							    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							    _("Error opening file"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);

		return FALSE;
	}

	return TRUE;
}

static void
file_build_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox)
{
	AlmanahFileDefinitionPrivate *priv = ALMANAH_FILE_DEFINITION (definition)->priv;
	const gchar *value = almanah_definition_get_value (definition);

	priv->chooser = gtk_file_chooser_button_new (_("Select File"), GTK_FILE_CHOOSER_ACTION_OPEN);

	/* Initialise the dialogue with the definition's current values */
	if (value != NULL)
		gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (priv->chooser), value);

	gtk_box_pack_start (GTK_BOX (parent_vbox), priv->chooser, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (parent_vbox));
}

static void
file_close_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox)
{
	AlmanahFileDefinitionPrivate *priv = ALMANAH_FILE_DEFINITION (definition)->priv;
	gchar *value = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (priv->chooser));

	almanah_definition_set_value (definition, value);
	g_free (value);

	almanah_definition_set_value2 (definition, NULL);
}

static void
file_parse_text (AlmanahDefinition *definition, const gchar *text)
{
	/* TODO */
}

static gchar *
file_get_blurb (AlmanahDefinition *definition)
{
	return g_strdup (almanah_definition_get_value (definition));
}
