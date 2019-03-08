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

#include <config.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "date-entry-dialog.h"
#include "interface.h"

static void almanah_date_entry_dialog_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_date_entry_dialog_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

/* GtkBuilder callbacks */
void ded_date_entry_notify_text_cb (GObject *gobject, GParamSpec *pspec, AlmanahDateEntryDialog *self);

typedef struct {
	GDate date;
	GtkWidget *ok_button;
	GtkEntry *date_entry;
} AlmanahDateEntryDialogPrivate;

struct _AlmanahDateEntryDialog {
	GtkDialog parent;
};

enum {
	PROP_DATE = 1
};

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahDateEntryDialog, almanah_date_entry_dialog, GTK_TYPE_DIALOG)

static void
almanah_date_entry_dialog_class_init (AlmanahDateEntryDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->set_property = almanah_date_entry_dialog_set_property;
	gobject_class->get_property = almanah_date_entry_dialog_get_property;

	g_object_class_install_property (gobject_class, PROP_DATE,
				g_param_spec_boxed ("date",
					"Date", "The current date selected by the dialog.",
					G_TYPE_DATE,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_date_entry_dialog_init (AlmanahDateEntryDialog *self)
{
	AlmanahDateEntryDialogPrivate *priv = almanah_date_entry_dialog_get_instance_private (self);

	g_date_clear (&(priv->date), 1);
	g_signal_connect (self, "response", G_CALLBACK (gtk_widget_hide), self);
	gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
	gtk_window_set_title (GTK_WINDOW (self), _("Select Date"));
}

static void
almanah_date_entry_dialog_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahDateEntryDialogPrivate *priv = almanah_date_entry_dialog_get_instance_private (ALMANAH_DATE_ENTRY_DIALOG (object));

	switch (property_id) {
		case PROP_DATE:
			g_value_set_boxed (value, &(priv->date));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_date_entry_dialog_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahDateEntryDialog *self = ALMANAH_DATE_ENTRY_DIALOG (object);

	switch (property_id) {
		case PROP_DATE:
			almanah_date_entry_dialog_set_date (self, g_value_get_boxed (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

AlmanahDateEntryDialog *
almanah_date_entry_dialog_new (void)
{
	GtkBuilder *builder;
	AlmanahDateEntryDialog *date_entry_dialog;
	AlmanahDateEntryDialogPrivate *priv;
	GError *error = NULL;
	const gchar *object_names[] = {
		"almanah_date_entry_dialog",
		NULL
	};

	builder = gtk_builder_new ();

	if (gtk_builder_add_objects_from_resource (builder, "/org/gnome/Almanah/ui/almanah.ui", (gchar**) object_names, &error) == 0) {
		/* Show an error */
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
							    GTK_DIALOG_MODAL,
							    GTK_MESSAGE_ERROR,
							    GTK_BUTTONS_OK,
							    _("UI data could not be loaded"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
		g_object_unref (builder);

		return NULL;
	}

	gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	date_entry_dialog = ALMANAH_DATE_ENTRY_DIALOG (gtk_builder_get_object (builder, "almanah_date_entry_dialog"));
	gtk_builder_connect_signals (builder, date_entry_dialog);

	if (date_entry_dialog == NULL) {
		g_object_unref (builder);
		return NULL;
	}

	priv = almanah_date_entry_dialog_get_instance_private (date_entry_dialog);

	/* Grab widgets */
	priv->ok_button = GTK_WIDGET (gtk_builder_get_object (builder, "almanah_ded_ok_button"));
	priv->date_entry = GTK_ENTRY (gtk_builder_get_object (builder, "almanah_ded_date_entry"));

	g_object_unref (builder);

	return date_entry_dialog;
}

gboolean
almanah_date_entry_dialog_run (AlmanahDateEntryDialog *self)
{
	AlmanahDateEntryDialogPrivate *priv = almanah_date_entry_dialog_get_instance_private (self);

	/* Reset the date, entry and consequently the OK button */
	gtk_entry_set_text (priv->date_entry, "");
	g_date_clear (&(priv->date), 1);

	return (gtk_dialog_run (GTK_DIALOG (self)) == GTK_RESPONSE_OK) ? TRUE : FALSE;
}

void
ded_date_entry_notify_text_cb (GObject *gobject, GParamSpec *param_spec, AlmanahDateEntryDialog *self)
{
	AlmanahDateEntryDialogPrivate *priv = almanah_date_entry_dialog_get_instance_private (self);
	GDate new_date;

	/* Enable/Disable the OK button based on whether the current date is valid */
	g_date_set_parse (&new_date, gtk_entry_get_text (priv->date_entry));

	if (g_date_valid (&new_date) == TRUE) {
		/* The date was parsed successfully; update priv->date and enable the OK button */
		priv->date = new_date;
		gtk_widget_set_sensitive (priv->ok_button, TRUE);
	} else {
		/* Failure due to the date entered being invalid; disable the OK button */
		gtk_widget_set_sensitive (priv->ok_button, FALSE);
	}
}

void
almanah_date_entry_dialog_get_date (AlmanahDateEntryDialog *self, GDate *date)
{
	AlmanahDateEntryDialogPrivate *priv = almanah_date_entry_dialog_get_instance_private (self);

	*date = priv->date;
}

void
almanah_date_entry_dialog_set_date (AlmanahDateEntryDialog *self, GDate *date)
{
	AlmanahDateEntryDialogPrivate *priv = almanah_date_entry_dialog_get_instance_private (self);

	priv->date = *date;
	g_object_notify (G_OBJECT (self), "date");
}
