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

#include "add-link-dialog.h"
#include "link.h"
#include "interface.h"

static void almanah_add_link_dialog_init (AlmanahAddLinkDialog *self);
static void almanah_add_link_dialog_dispose (GObject *object);
static void ald_response_cb (GtkDialog *dialog, gint response_id, AlmanahAddLinkDialog *add_link_dialog);
static void ald_show_cb (GtkWidget *widget, AlmanahAddLinkDialog *add_link_dialog);

struct _AlmanahAddLinkDialogPrivate {
	GtkComboBox *ald_type_combo_box;
	GtkVBox *ald_vbox;
	GtkListStore *ald_type_store;
	AlmanahLink *link;
};

G_DEFINE_TYPE (AlmanahAddLinkDialog, almanah_add_link_dialog, GTK_TYPE_DIALOG)
#define ALMANAH_ADD_LINK_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_ADD_LINK_DIALOG, AlmanahAddLinkDialogPrivate))

static void
almanah_add_link_dialog_class_init (AlmanahAddLinkDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	g_type_class_add_private (klass, sizeof (AlmanahAddLinkDialogPrivate));
	gobject_class->dispose = almanah_add_link_dialog_dispose;
}

static void
almanah_add_link_dialog_init (AlmanahAddLinkDialog *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_ADD_LINK_DIALOG, AlmanahAddLinkDialogPrivate);

	g_signal_connect (self, "response", G_CALLBACK (ald_response_cb), self);
	g_signal_connect (self, "show", G_CALLBACK (ald_show_cb), self);
	gtk_dialog_set_has_separator (GTK_DIALOG (self), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
	gtk_window_set_title (GTK_WINDOW (self), _("Add Link"));
}

static void
almanah_add_link_dialog_dispose (GObject *object)
{
	AlmanahAddLinkDialogPrivate *priv = ALMANAH_ADD_LINK_DIALOG (object)->priv;

	if (priv->link != NULL) {
		g_object_unref (priv->link);
		priv->link = NULL;
	}

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_add_link_dialog_parent_class)->dispose (object);
}

AlmanahAddLinkDialog *
almanah_add_link_dialog_new (void)
{
	GtkBuilder *builder;
	AlmanahAddLinkDialog *add_link_dialog;
	AlmanahAddLinkDialogPrivate *priv;
	GError *error = NULL;
	const gchar *interface_filename = almanah_get_interface_filename ();
	const gchar *object_names[] = {
		"dry_add_link_dialog",
		"dry_ald_type_store",
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
	add_link_dialog = ALMANAH_ADD_LINK_DIALOG (gtk_builder_get_object (builder, "dry_add_link_dialog"));
	gtk_builder_connect_signals (builder, add_link_dialog);

	if (add_link_dialog == NULL) {
		g_object_unref (builder);
		return NULL;
	}

	priv = add_link_dialog->priv;

	/* Grab our child widgets */
	priv->ald_type_combo_box = GTK_COMBO_BOX (gtk_builder_get_object (builder, "dry_ald_type_combo_box"));
	priv->ald_vbox = GTK_VBOX (gtk_builder_get_object (builder, "dry_ald_vbox"));
	priv->ald_type_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "dry_ald_type_store"));

	almanah_link_populate_model (priv->ald_type_store, 1, 0, 2);
	gtk_combo_box_set_active (priv->ald_type_combo_box, 0);

	g_object_unref (builder);

	return add_link_dialog;
}

static void
destroy_extra_widgets (AlmanahAddLinkDialog *self)
{
	gtk_container_foreach (GTK_CONTAINER (self->priv->ald_vbox), (GtkCallback) gtk_widget_destroy, NULL);
}

static void
ald_response_cb (GtkDialog *dialog, gint response_id, AlmanahAddLinkDialog *add_link_dialog)
{
	/* Save the entered data */
	g_assert (add_link_dialog->priv->link != NULL);
	almanah_link_get_values (add_link_dialog->priv->link);

	/* Make sure to remove all the custom widgets for the currently-selected link type */
	gtk_widget_hide_all (GTK_WIDGET (dialog));
	destroy_extra_widgets (add_link_dialog);
}

void
ald_type_combo_box_changed_cb (GtkComboBox *self, AlmanahAddLinkDialog *add_link_dialog)
{
	GtkTreeIter iter;
	gchar *type_id;
	AlmanahAddLinkDialogPrivate *priv = add_link_dialog->priv;

	destroy_extra_widgets (add_link_dialog);

	if (gtk_combo_box_get_active_iter (self, &iter) == FALSE)
		return;

	gtk_tree_model_get (gtk_combo_box_get_model (self), &iter, 1, &type_id, -1);

	if (priv->link != NULL)
		g_object_unref (priv->link);

	priv->link = almanah_link_new (type_id);
	almanah_link_build_dialog (priv->link, priv->ald_vbox);

	g_free (type_id);
}

static void
ald_show_cb (GtkWidget *widget, AlmanahAddLinkDialog *add_link_dialog)
{
	/* Select a default link type */
	ald_type_combo_box_changed_cb (add_link_dialog->priv->ald_type_combo_box, add_link_dialog);
}

AlmanahLink *
almanah_add_link_dialog_get_link (AlmanahAddLinkDialog *self)
{
	return g_object_ref (self->priv->link);
}
