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

#include "event.h"
#include "f-spot-photo.h"
#include "main.h"

static void almanah_f_spot_photo_event_finalize (GObject *object);
static const gchar *almanah_f_spot_photo_event_format_value (AlmanahEvent *event);
static gboolean almanah_f_spot_photo_event_view (AlmanahEvent *event);

struct _AlmanahFSpotPhotoEventPrivate {
	gchar *uri;
	gchar *name;
};

G_DEFINE_TYPE (AlmanahFSpotPhotoEvent, almanah_f_spot_photo_event, ALMANAH_TYPE_EVENT)
#define ALMANAH_F_SPOT_PHOTO_EVENT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_F_SPOT_PHOTO_EVENT, AlmanahFSpotPhotoEventPrivate))

static void
almanah_f_spot_photo_event_class_init (AlmanahFSpotPhotoEventClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AlmanahEventClass *event_class = ALMANAH_EVENT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahFSpotPhotoEventPrivate));

	gobject_class->finalize = almanah_f_spot_photo_event_finalize;

	event_class->name = _("F-Spot Photo");
	event_class->description = _("A photo stored in F-Spot.");
	event_class->icon_name = "f-spot";

	event_class->format_value = almanah_f_spot_photo_event_format_value;
	event_class->view = almanah_f_spot_photo_event_view;
}

static void
almanah_f_spot_photo_event_init (AlmanahFSpotPhotoEvent *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_F_SPOT_PHOTO_EVENT, AlmanahFSpotPhotoEventPrivate);
}

static void
almanah_f_spot_photo_event_finalize (GObject *object)
{
	AlmanahFSpotPhotoEventPrivate *priv = ALMANAH_F_SPOT_PHOTO_EVENT_GET_PRIVATE (object);

	g_free (priv->uri);
	g_free (priv->name);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_f_spot_photo_event_parent_class)->finalize (object);
}

AlmanahFSpotPhotoEvent *
almanah_f_spot_photo_event_new (const gchar *uri, const gchar *name)
{
	AlmanahFSpotPhotoEvent *event = g_object_new (ALMANAH_TYPE_F_SPOT_PHOTO_EVENT, NULL);
	event->priv->uri = g_strdup (uri);
	event->priv->name = g_strdup (name);
	return event;
}

static const gchar *
almanah_f_spot_photo_event_format_value (AlmanahEvent *event)
{
	return ALMANAH_F_SPOT_PHOTO_EVENT (event)->priv->name;
}

static gboolean
almanah_f_spot_photo_event_view (AlmanahEvent *event)
{
	AlmanahFSpotPhotoEventPrivate *priv = ALMANAH_F_SPOT_PHOTO_EVENT (event)->priv;
	gchar *command_line;
	gboolean retval;
	GError *error = NULL;

	command_line = g_strdup_printf ("f-spot --view \"%s\"", priv->uri);

	if (almanah->debug == TRUE)
		g_debug ("Executing \"%s\".", command_line);

	retval = gdk_spawn_command_line_on_screen (gtk_widget_get_screen (almanah->main_window), command_line, &error);
	g_free (command_line);

	if (retval == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (almanah->main_window),
							    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							    _("Error launching F-Spot"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);

		return FALSE;
	}

	return TRUE;
}
