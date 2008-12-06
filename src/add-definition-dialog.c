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

#include <config.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "add-definition-dialog.h"
#include "definition.h"
#include "interface.h"
#include "main.h"
#include "storage-manager.h"

static void almanah_add_definition_dialog_init (AlmanahAddDefinitionDialog *self);
static void almanah_add_definition_dialog_dispose (GObject *object);
static void almanah_add_definition_dialog_finalize (GObject *object);
static void almanah_add_definition_dialog_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_add_definition_dialog_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void response_cb (GtkDialog *dialog, gint response_id, AlmanahAddDefinitionDialog *self);

struct _AlmanahAddDefinitionDialogPrivate {
	GtkComboBox *type_combo_box;
	GtkVBox *vbox;
	GtkListStore *type_store;
	AlmanahDefinition *definition;
	gchar *text;
};

enum {
	PROP_TEXT = 1
};

G_DEFINE_TYPE (AlmanahAddDefinitionDialog, almanah_add_definition_dialog, GTK_TYPE_DIALOG)
#define ALMANAH_ADD_DEFINITION_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_ADD_DEFINITION_DIALOG, AlmanahAddDefinitionDialogPrivate))

static void
almanah_add_definition_dialog_class_init (AlmanahAddDefinitionDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahAddDefinitionDialogPrivate));

	gobject_class->set_property = almanah_add_definition_dialog_set_property;
	gobject_class->get_property = almanah_add_definition_dialog_get_property;
	gobject_class->dispose = almanah_add_definition_dialog_dispose;
	gobject_class->finalize = almanah_add_definition_dialog_finalize;

	g_object_class_install_property (gobject_class, PROP_TEXT,
				g_param_spec_string ("text",
					"Text", "The text for which this dialog is getting a definition.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_add_definition_dialog_init (AlmanahAddDefinitionDialog *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_ADD_DEFINITION_DIALOG, AlmanahAddDefinitionDialogPrivate);

	g_signal_connect (self, "response", G_CALLBACK (response_cb), self);
	gtk_dialog_set_has_separator (GTK_DIALOG (self), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
	gtk_window_set_title (GTK_WINDOW (self), _("Add Definition"));
	gtk_window_set_transient_for (GTK_WINDOW (self), GTK_WINDOW (almanah->main_window));
}

static void
almanah_add_definition_dialog_dispose (GObject *object)
{
	AlmanahAddDefinitionDialogPrivate *priv = ALMANAH_ADD_DEFINITION_DIALOG (object)->priv;

	if (priv->definition != NULL)
		g_object_unref (priv->definition);
	priv->definition = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_add_definition_dialog_parent_class)->dispose (object);
}

static void
almanah_add_definition_dialog_finalize (GObject *object)
{
	AlmanahAddDefinitionDialogPrivate *priv = ALMANAH_ADD_DEFINITION_DIALOG (object)->priv;

	g_free (priv->text);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_add_definition_dialog_parent_class)->finalize (object);
}

static void
almanah_add_definition_dialog_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahAddDefinitionDialogPrivate *priv = ALMANAH_ADD_DEFINITION_DIALOG (object)->priv;

	switch (property_id) {
		case PROP_TEXT:
			g_value_set_string (value, priv->text);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_add_definition_dialog_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahAddDefinitionDialog *self = ALMANAH_ADD_DEFINITION_DIALOG (object);

	switch (property_id) {
		case PROP_TEXT:
			almanah_add_definition_dialog_set_text (self, g_value_get_string (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

AlmanahAddDefinitionDialog *
almanah_add_definition_dialog_new (void)
{
	GtkBuilder *builder;
	AlmanahAddDefinitionDialog *add_definition_dialog;
	AlmanahAddDefinitionDialogPrivate *priv;
	GError *error = NULL;
	const gchar *interface_filename = almanah_get_interface_filename ();
	const gchar *object_names[] = {
		"dry_add_definition_dialog",
		"dry_add_type_store",
		NULL
	};

	builder = gtk_builder_new ();

	if (gtk_builder_add_objects_from_file (builder, interface_filename, (gchar**) object_names, &error) == FALSE) {
		/* Show an error */
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				_("UI file \"%s\" could not be loaded"), interface_filename);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
		g_object_unref (builder);

		return NULL;
	}

	gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	add_definition_dialog = ALMANAH_ADD_DEFINITION_DIALOG (gtk_builder_get_object (builder, "dry_add_definition_dialog"));
	gtk_builder_connect_signals (builder, add_definition_dialog);

	if (add_definition_dialog == NULL) {
		g_object_unref (builder);
		return NULL;
	}

	priv = add_definition_dialog->priv;

	/* Grab our child widgets */
	priv->type_combo_box = GTK_COMBO_BOX (gtk_builder_get_object (builder, "dry_add_type_combo_box"));
	priv->vbox = GTK_VBOX (gtk_builder_get_object (builder, "dry_add_vbox"));
	priv->type_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "dry_add_type_store"));

	almanah_definition_populate_model (priv->type_store, 1, 0, 2);
	gtk_combo_box_set_active (priv->type_combo_box, 0);

	g_object_unref (builder);

	return add_definition_dialog;
}

static void
destroy_extra_widgets (AlmanahAddDefinitionDialog *self)
{
	gtk_container_foreach (GTK_CONTAINER (self->priv->vbox), (GtkCallback) gtk_widget_destroy, NULL);
}

static void
response_cb (GtkDialog *dialog, gint response_id, AlmanahAddDefinitionDialog *self)
{
	AlmanahAddDefinitionDialogPrivate *priv = self->priv;

	/* Save the entered data */
	g_assert (priv->definition != NULL);
	almanah_definition_close_dialog (priv->definition, priv->vbox);

	/* Make sure to remove all the custom widgets for the currently-selected definition type */
	gtk_widget_hide_all (GTK_WIDGET (dialog));
	destroy_extra_widgets (self);
}

void
add_type_combo_box_changed_cb (GtkComboBox *combo_box, AlmanahAddDefinitionDialog *self)
{
	GtkTreeIter iter;
	AlmanahDefinitionType type_id;
	AlmanahAddDefinitionDialogPrivate *priv = self->priv;

	destroy_extra_widgets (self);

	if (gtk_combo_box_get_active_iter (combo_box, &iter) == FALSE)
		return;
	gtk_tree_model_get (gtk_combo_box_get_model (combo_box), &iter, 1, &type_id, -1);

	/* Create a new definition of the correct type if necessary */
	if (priv->definition == NULL || almanah_definition_get_type_id (priv->definition) != type_id) {
		if (priv->definition != NULL)
			g_object_unref (priv->definition);

		priv->definition = almanah_definition_new (type_id);
	}

	almanah_definition_build_dialog (priv->definition, priv->vbox);
	if (priv->text != NULL)
		almanah_definition_parse_text (priv->definition, priv->text);
}

const gchar *
almanah_add_definition_dialog_get_text (AlmanahAddDefinitionDialog *self)
{
	return self->priv->text;
}

void
almanah_add_definition_dialog_set_text (AlmanahAddDefinitionDialog *self, const gchar *text)
{
	AlmanahAddDefinitionDialogPrivate *priv = self->priv;
	AlmanahDefinition *definition;

	g_free (self->priv->text);
	self->priv->text = g_strdup (text);

	/* See if a definition for this text already exists... */
	definition = almanah_storage_manager_get_definition (almanah->storage_manager, text);
	if (definition != NULL) {
		/* Use the definition we've got from the DB */
		if (priv->definition != NULL)
			g_object_unref (priv->definition);
		priv->definition = definition;

		/* Rebuild the UI so it contains all the data from the new definition */
		gtk_combo_box_set_active (priv->type_combo_box, -1); /* just to make sure the selection does actually change */
		gtk_combo_box_set_active (priv->type_combo_box, almanah_definition_get_type_id (definition) - 1);
	} else {
		/* Select a default definition type */
		gtk_combo_box_set_active (priv->type_combo_box, -1); /* just to make sure the selection does actually change */
		gtk_combo_box_set_active (priv->type_combo_box, 0);
	}

	g_object_notify (G_OBJECT (self), "text");
}

AlmanahDefinition *
almanah_add_definition_dialog_get_definition (AlmanahAddDefinitionDialog *self)
{
	g_assert (self->priv->text != NULL);
	return self->priv->definition;
}
