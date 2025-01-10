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
#include "interface.h"

static void almanah_calendar_task_event_finalize (GObject *object);
static const gchar *almanah_calendar_task_event_format_value (AlmanahEvent *event);
static const gchar *almanah_calendar_task_event_format_time (AlmanahEvent *event);
static gboolean almanah_calendar_task_event_view (AlmanahEvent *event, GtkWindow *parent_window);

typedef struct {
	gchar *uid;
	gchar *summary;
	gchar *time;
} AlmanahCalendarTaskEventPrivate;

struct _AlmanahCalendarTaskEvent {
	AlmanahEvent parent;
};

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahCalendarTaskEvent, almanah_calendar_task_event, ALMANAH_TYPE_EVENT)

static void
almanah_calendar_task_event_class_init (AlmanahCalendarTaskEventClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AlmanahEventClass *event_class = ALMANAH_EVENT_CLASS (klass);

	gobject_class->finalize = almanah_calendar_task_event_finalize;

	event_class->name = _("Calendar Task");
	event_class->description = _("A task on an Evolution calendar.");
	event_class->icon_name = "evolution-tasks";

	event_class->format_value = almanah_calendar_task_event_format_value;
	event_class->format_time = almanah_calendar_task_event_format_time;
	event_class->view = almanah_calendar_task_event_view;
}

static void
almanah_calendar_task_event_init (AlmanahCalendarTaskEvent *self)
{
}

static void
almanah_calendar_task_event_finalize (GObject *object)
{
	AlmanahCalendarTaskEventPrivate *priv = almanah_calendar_task_event_get_instance_private (ALMANAH_CALENDAR_TASK_EVENT (object));

	g_free (priv->uid);
	g_free (priv->summary);
	g_free (priv->time);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_calendar_task_event_parent_class)->finalize (object);
}

AlmanahCalendarTaskEvent *
almanah_calendar_task_event_new (const gchar *uid, const gchar *summary, time_t start_time)
{
	AlmanahCalendarTaskEvent *event = g_object_new (ALMANAH_TYPE_CALENDAR_TASK_EVENT, NULL);
	AlmanahCalendarTaskEventPrivate *priv = almanah_calendar_task_event_get_instance_private (event);
	struct tm date_tm;

	priv->uid = g_strdup (uid);
	priv->summary = g_strdup (summary);

	localtime_r ((const time_t*) &(start_time), &date_tm);
	/* Translators: This is a time string with the format hh:mm */
	priv->time = g_strdup_printf (_("%.2d:%.2d"), date_tm.tm_hour, date_tm.tm_min);

	return event;
}

static const gchar *
almanah_calendar_task_event_format_value (AlmanahEvent *event)
{
	AlmanahCalendarTaskEventPrivate *priv = almanah_calendar_task_event_get_instance_private (ALMANAH_CALENDAR_TASK_EVENT (event));

	return priv->summary;
}

static const gchar *
almanah_calendar_task_event_format_time (AlmanahEvent *event)
{
	AlmanahCalendarTaskEventPrivate *priv = almanah_calendar_task_event_get_instance_private (ALMANAH_CALENDAR_TASK_EVENT (event));

	return priv->time;
}

static gboolean
almanah_calendar_task_event_view (AlmanahEvent *event, GtkWindow *parent_window)
{
	AlmanahCalendarTaskEventPrivate *priv = almanah_calendar_task_event_get_instance_private (ALMANAH_CALENDAR_TASK_EVENT (event));
	gchar *command_line;
	gboolean retval;
	GError *error = NULL;

	command_line = g_strdup_printf ("evolution task:%s", priv->uid);

	g_debug ("Executing \"%s\".", command_line);

	retval = almanah_run_on_screen (gtk_widget_get_screen (GTK_WIDGET (parent_window)), command_line, &error);
	g_free (command_line);

	if (retval == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (parent_window,
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
