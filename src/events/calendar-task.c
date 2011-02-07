/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2008-2009 <philip@tecnocode.co.uk>
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
#include "calendar-task.h"
#include "main.h"

static void almanah_calendar_task_event_finalize (GObject *object);
static const gchar *almanah_calendar_task_event_format_value (AlmanahEvent *event);
static gboolean almanah_calendar_task_event_view (AlmanahEvent *event);

struct _AlmanahCalendarTaskEventPrivate {
	gchar *uid;
	gchar *summary;
};

G_DEFINE_TYPE (AlmanahCalendarTaskEvent, almanah_calendar_task_event, ALMANAH_TYPE_EVENT)
#define ALMANAH_CALENDAR_TASK_EVENT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_CALENDAR_TASK_EVENT, AlmanahCalendarTaskEventPrivate))

static void
almanah_calendar_task_event_class_init (AlmanahCalendarTaskEventClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AlmanahEventClass *event_class = ALMANAH_EVENT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahCalendarTaskEventPrivate));

	gobject_class->finalize = almanah_calendar_task_event_finalize;

	event_class->name = _("Calendar Task");
	event_class->description = _("A task on an Evolution calendar.");
	event_class->icon_name = "stock_task";

	event_class->format_value = almanah_calendar_task_event_format_value;
	event_class->view = almanah_calendar_task_event_view;
}

static void
almanah_calendar_task_event_init (AlmanahCalendarTaskEvent *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_CALENDAR_TASK_EVENT, AlmanahCalendarTaskEventPrivate);
}

static void
almanah_calendar_task_event_finalize (GObject *object)
{
	AlmanahCalendarTaskEventPrivate *priv = ALMANAH_CALENDAR_TASK_EVENT_GET_PRIVATE (object);

	g_free (priv->uid);
	g_free (priv->summary);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_calendar_task_event_parent_class)->finalize (object);
}

AlmanahCalendarTaskEvent *
almanah_calendar_task_event_new (const gchar *uid, const gchar *summary)
{
	AlmanahCalendarTaskEvent *event = g_object_new (ALMANAH_TYPE_CALENDAR_TASK_EVENT, NULL);
	event->priv->uid = g_strdup (uid);
	event->priv->summary = g_strdup (summary);
	return event;
}

static const gchar *
almanah_calendar_task_event_format_value (AlmanahEvent *event)
{
	return ALMANAH_CALENDAR_TASK_EVENT (event)->priv->summary;
}

static gboolean
almanah_calendar_task_event_view (AlmanahEvent *event)
{
	AlmanahCalendarTaskEventPrivate *priv = ALMANAH_CALENDAR_TASK_EVENT (event)->priv;
	gchar *command_line;
	gboolean retval;
	GError *error = NULL;

	command_line = g_strdup_printf ("evolution task:%s", priv->uid);

	if (almanah->debug == TRUE)
		g_debug ("Executing \"%s\".", command_line);

	retval = almanah_run_on_screen (gtk_widget_get_screen (almanah->main_window), command_line, &error);
	g_free (command_line);

	if (retval == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (almanah->main_window),
							    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							    _("Error launching Evolution"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);

		return FALSE;
	}

	return TRUE;
}
