/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2009 <philip@tecnocode.co.uk>
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
#include <libedataserverui/e-name-selector.h>

#include "contact.h"
#include "../definition.h"
#include "../main.h"

static void almanah_contact_definition_dispose (GObject *object);
static gboolean contact_view (AlmanahDefinition *definition);
static void contact_build_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox);
static void contact_close_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox);
static void contact_parse_text (AlmanahDefinition *definition, const gchar *text);
static gchar *contact_get_blurb (AlmanahDefinition *definition);

struct _AlmanahContactDefinitionPrivate {
	ENameSelector *name_selector;
};

G_DEFINE_TYPE (AlmanahContactDefinition, almanah_contact_definition, ALMANAH_TYPE_DEFINITION)
#define ALMANAH_CONTACT_DEFINITION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_CONTACT_DEFINITION, AlmanahContactDefinitionPrivate))

static void
almanah_contact_definition_class_init (AlmanahContactDefinitionClass *klass)
{
	AlmanahDefinitionClass *definition_class = ALMANAH_DEFINITION_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahContactDefinitionPrivate));

	gobject_class->dispose = almanah_contact_definition_dispose;

	definition_class->type_id = ALMANAH_DEFINITION_CONTACT;
	definition_class->name = _("Contact");
	definition_class->description = _("An Evolution contact.");
	definition_class->icon_name = "stock_contact";

	definition_class->view = contact_view;
	definition_class->build_dialog = contact_build_dialog;
	definition_class->close_dialog = contact_close_dialog;
	definition_class->parse_text = contact_parse_text;
	definition_class->get_blurb = contact_get_blurb;
}

static void
almanah_contact_definition_init (AlmanahContactDefinition *self)
{
	ENameSelectorModel *name_selector_model;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_CONTACT_DEFINITION, AlmanahContactDefinitionPrivate);
	self->priv->name_selector = e_name_selector_new ();

	name_selector_model = e_name_selector_peek_model (self->priv->name_selector);
	e_name_selector_model_add_section (name_selector_model, "Select Contact", _("Select Contact"), NULL);
}

static void
almanah_contact_definition_dispose (GObject *object)
{
	AlmanahContactDefinitionPrivate *priv = ALMANAH_CONTACT_DEFINITION (object)->priv;

	if (priv->name_selector != NULL)
		g_object_unref (priv->name_selector);
	priv->name_selector = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_contact_definition_parent_class)->dispose (object);
}

static gboolean
contact_view (AlmanahDefinition *definition)
{
	const gchar *source_uid;
	gchar *command_line;
	EBook *book;
	ESource *source;
	GError *error = NULL;

	/* Get the UID of the source containing the contact from Evolution */
	if ((book = e_book_new_default_addressbook (&error)) == NULL ||
	    e_book_open (book, TRUE, &error) == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
							    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							    _("Error opening contact"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
		if (book != NULL)
			g_object_unref (book);

		return FALSE;
	}

	source = e_book_get_source (book);
	g_assert (source != NULL);
	source_uid = e_source_peek_uid (source);
	g_assert (source_uid != NULL);

	g_object_unref (book);

	/* Build the command line to launch Evolution. It should look something like this:
	 * evolution contacts:?source-uid=foo&contact-uid=bar */
	command_line = g_strdup_printf ("evolution \"contacts:?source-uid=%s&contact-uid=%s\"",
					source_uid, almanah_definition_get_value (definition));

	if (almanah->debug == TRUE) {
		g_debug ("Launching Evolution with source UID \"%s\", contact UID \"%s\" and command line: %s",
			 source_uid, almanah_definition_get_value (definition), command_line);
	}

	/* Run Evolution */
	if (g_spawn_command_line_async (command_line, &error) == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
							    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							    _("Error opening Evolution"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
		g_free (command_line);

		return FALSE;
	}

	g_free (command_line);

	return TRUE;
}

static void
contact_build_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox)
{
	AlmanahContactDefinitionPrivate *priv = ALMANAH_CONTACT_DEFINITION (definition)->priv;
	ENameSelectorEntry *name_selector_entry;
	const gchar *value = almanah_definition_get_value (definition);

	name_selector_entry = e_name_selector_peek_section_entry (priv->name_selector, "Select Contact");

	/* Initialise the dialogue with the definition's current values */
	if (value != NULL)
		gtk_entry_set_text (GTK_ENTRY (name_selector_entry), value);

	gtk_box_pack_start (GTK_BOX (parent_vbox), GTK_WIDGET (name_selector_entry), TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (parent_vbox));
}

static void
contact_close_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox)
{
	AlmanahContactDefinitionPrivate *priv = ALMANAH_CONTACT_DEFINITION (definition)->priv;
	const gchar *value;
	GList *destinations;
	EDestinationStore *destination_store;
	ENameSelectorEntry *name_selector_entry;

	/* Get the selected contact from the dialogue */
	name_selector_entry = e_name_selector_peek_section_entry (priv->name_selector, "Select Contact");
	destination_store = e_name_selector_entry_peek_destination_store (name_selector_entry);
	destinations = e_destination_store_list_destinations (destination_store);

	if (destinations == NULL) {
		almanah_definition_set_value (definition, NULL);
		almanah_definition_set_value2 (definition, NULL);
		return;
	}

	g_assert (destinations->data != NULL);

	/* At the moment, we only use the first address entered */
	value = e_destination_get_contact_uid (E_DESTINATION (destinations->data));

	almanah_definition_set_value (definition, value);
	almanah_definition_set_value2 (definition, NULL);

	g_list_free (destinations);
}

static void
contact_parse_text (AlmanahDefinition *definition, const gchar *text)
{
	AlmanahContactDefinitionPrivate *priv = ALMANAH_CONTACT_DEFINITION (definition)->priv;
	ENameSelectorEntry *name_selector_entry = e_name_selector_peek_section_entry (priv->name_selector, "Select Contact");
	gtk_entry_set_text (GTK_ENTRY (name_selector_entry), text);
}

static gchar *
contact_get_blurb (AlmanahDefinition *definition)
{
	EBook *book;
	EContact *contact = NULL;
	GError *error = NULL;
	gchar *name = NULL;

	/* Get the contact from Evolution */
	if ((book = e_book_new_default_addressbook (&error)) == NULL ||
	    e_book_open (book, TRUE, &error) == FALSE ||
	    e_book_get_contact (book, almanah_definition_get_value (definition), &contact, &error) == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
							    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							    _("Error opening contact"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
		goto error;
	}

	name = e_contact_get (contact, E_CONTACT_FULL_NAME);

error:
	if (contact != NULL)
		g_object_unref (contact);
	if (book != NULL)
		g_object_unref (book);

	return name;
}
