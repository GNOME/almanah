/*
 * Almanah
 * Copyright (C) Philip Withnall 2011 <philip@tecnocode.co.uk>
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

#include "uri-entry-dialog.h"

static void almanah_uri_entry_dialog_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_uri_entry_dialog_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

/* GtkBuilder callbacks */
G_MODULE_EXPORT void ued_uri_entry_notify_text_cb (GObject *gobject, GParamSpec *pspec, AlmanahUriEntryDialog *self);

typedef struct {
	gchar *uri;
	GtkWidget *ok_button;
	GtkEntry *uri_entry;
} AlmanahUriEntryDialogPrivate;

struct _AlmanahUriEntryDialog {
	GtkDialog parent;
};

enum {
	PROP_URI = 1
};

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahUriEntryDialog, almanah_uri_entry_dialog, GTK_TYPE_DIALOG)

static void
almanah_uri_entry_dialog_class_init (AlmanahUriEntryDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->set_property = almanah_uri_entry_dialog_set_property;
	gobject_class->get_property = almanah_uri_entry_dialog_get_property;

	g_object_class_install_property (gobject_class, PROP_URI,
	                                 g_param_spec_string ("uri",
	                                                      "URI", "The current URI entered in the dialog.",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Almanah/ui/uri-entry-dialog.ui");

	gtk_widget_class_bind_template_child_private (widget_class, AlmanahUriEntryDialog, ok_button);
	gtk_widget_class_bind_template_child_private (widget_class, AlmanahUriEntryDialog, uri_entry);
}

static void
almanah_uri_entry_dialog_init (AlmanahUriEntryDialog *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));

	g_signal_connect (self, "response", (GCallback) gtk_widget_hide, self);
	gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
	gtk_window_set_title (GTK_WINDOW (self), _ ("Enter URI"));
}

static void
almanah_uri_entry_dialog_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahUriEntryDialogPrivate *priv = almanah_uri_entry_dialog_get_instance_private (ALMANAH_URI_ENTRY_DIALOG (object));

	switch (property_id) {
		case PROP_URI:
			g_value_set_string (value, priv->uri);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_uri_entry_dialog_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahUriEntryDialog *self = ALMANAH_URI_ENTRY_DIALOG (object);

	switch (property_id) {
		case PROP_URI:
			almanah_uri_entry_dialog_set_uri (self, g_value_get_string (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

AlmanahUriEntryDialog *
almanah_uri_entry_dialog_new (void)
{
	AlmanahUriEntryDialog *uri_entry_dialog;

	uri_entry_dialog = ALMANAH_URI_ENTRY_DIALOG (g_object_new (ALMANAH_TYPE_URI_ENTRY_DIALOG, NULL));

	return uri_entry_dialog;
}

gboolean
almanah_uri_entry_dialog_run (AlmanahUriEntryDialog *self)
{
	AlmanahUriEntryDialogPrivate *priv = almanah_uri_entry_dialog_get_instance_private (self);

	/* Reset the URI entry and consequently the OK button */
	gtk_entry_set_text (priv->uri_entry, "");
	g_free (priv->uri);
	priv->uri = NULL;

	return (gtk_dialog_run (GTK_DIALOG (self)) == GTK_RESPONSE_OK) ? TRUE : FALSE;
}

static gboolean
is_uri_valid (const gchar *uri)
{
	g_autofree gchar *tmp = NULL;

	/* We assume that g_uri_parse_scheme() will fail if the URI is invalid. */
	tmp = g_uri_parse_scheme (uri);

	return (tmp != NULL) ? TRUE : FALSE;
}

G_MODULE_EXPORT void
ued_uri_entry_notify_text_cb (GObject *gobject, GParamSpec *param_spec, AlmanahUriEntryDialog *self)
{
	AlmanahUriEntryDialogPrivate *priv = almanah_uri_entry_dialog_get_instance_private (self);

	/* Enable/Disable the OK button based on whether the current URI is valid. */
	if (is_uri_valid (gtk_entry_get_text (priv->uri_entry)) == TRUE) {
		/* The URI was parsed successfully; update priv->uri and enable the OK button */
		priv->uri = g_strdup (gtk_entry_get_text (priv->uri_entry));
		gtk_widget_set_sensitive (priv->ok_button, TRUE);
	} else {
		/* Failure due to the URI entered being invalid; disable the OK button */
		gtk_widget_set_sensitive (priv->ok_button, FALSE);
	}
}

const gchar *
almanah_uri_entry_dialog_get_uri (AlmanahUriEntryDialog *self)
{
	g_return_val_if_fail (ALMANAH_IS_URI_ENTRY_DIALOG (self), NULL);

	AlmanahUriEntryDialogPrivate *priv = almanah_uri_entry_dialog_get_instance_private (self);

	return priv->uri;
}

void
almanah_uri_entry_dialog_set_uri (AlmanahUriEntryDialog *self, const gchar *uri)
{
	g_return_if_fail (ALMANAH_IS_URI_ENTRY_DIALOG (self));
	g_return_if_fail (uri == NULL || is_uri_valid (uri) == TRUE);

	AlmanahUriEntryDialogPrivate *priv = almanah_uri_entry_dialog_get_instance_private (self);

	g_free (priv->uri);
	priv->uri = g_strdup (uri);

	g_object_notify (G_OBJECT (self), "uri");
}
