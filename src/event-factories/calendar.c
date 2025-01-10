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

#include "../event.h"
#include "calendar.h"
#include "calendar-client.h"
#include "../event-factory.h"
#include "../events/calendar-appointment.h"
#include "../events/calendar-task.h"

static void almanah_calendar_event_factory_dispose (GObject *object);
static void query_events (AlmanahEventFactory *event_factory, GDate *date);
static GSList *get_events (AlmanahEventFactory *event_factory, GDate *date);
static void events_changed_cb (CalendarClient *client, AlmanahCalendarEventFactory *self);

typedef struct {
	CalendarClient *client;
} AlmanahCalendarEventFactoryPrivate;

struct _AlmanahCalendarEventFactory {
	AlmanahEventFactory parent;
	AlmanahCalendarEventFactoryPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahCalendarEventFactory, almanah_calendar_event_factory, ALMANAH_TYPE_EVENT_FACTORY)

static void
almanah_calendar_event_factory_class_init (AlmanahCalendarEventFactoryClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AlmanahEventFactoryClass *event_factory_class = ALMANAH_EVENT_FACTORY_CLASS (klass);

	gobject_class->dispose = almanah_calendar_event_factory_dispose;

	event_factory_class->type_id = ALMANAH_EVENT_FACTORY_CALENDAR;
	event_factory_class->query_events = query_events;
	event_factory_class->get_events = get_events;
}

static void
almanah_calendar_event_factory_init (AlmanahCalendarEventFactory *self)
{
	AlmanahCalendarEventFactoryPrivate *priv = almanah_calendar_event_factory_get_instance_private (self);

	priv->client = calendar_client_new ();

	g_signal_connect (priv->client, "tasks-changed", G_CALLBACK (events_changed_cb), self);
	g_signal_connect (priv->client, "appointments-changed", G_CALLBACK (events_changed_cb), self);
}

static void
almanah_calendar_event_factory_dispose (GObject *object)
{
	AlmanahCalendarEventFactoryPrivate *priv = almanah_calendar_event_factory_get_instance_private (ALMANAH_CALENDAR_EVENT_FACTORY (object));

	if (priv->client != NULL)
		g_object_unref (priv->client);
	priv->client = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_calendar_event_factory_parent_class)->dispose (object);
}

static void
query_events (AlmanahEventFactory *event_factory, GDate *date)
{
	AlmanahCalendarEventFactory *self = ALMANAH_CALENDAR_EVENT_FACTORY (event_factory);
	AlmanahCalendarEventFactoryPrivate *priv = almanah_calendar_event_factory_get_instance_private (self);

	calendar_client_select_day (priv->client, g_date_get_day (date));
	calendar_client_select_month (priv->client, g_date_get_month (date) - 1, g_date_get_year (date));
	g_signal_emit_by_name (self, "events-updated");
}

static void
events_changed_cb (CalendarClient *client, AlmanahCalendarEventFactory *self)
{
	g_signal_emit_by_name (self, "events-updated");
}

static inline time_t
date_to_time (GDate *date)
{
	struct tm localtime_tm = { 0, };

	localtime_tm.tm_mday = g_date_get_day (date);
	localtime_tm.tm_mon = g_date_get_month (date) - 1;
	localtime_tm.tm_year = g_date_get_year (date) - 1900;
	localtime_tm.tm_isdst = -1;

	return mktime (&localtime_tm);
}

static GSList *
get_events (AlmanahEventFactory *event_factory, GDate *date)
{
	GSList *calendar_events, *e, *events = NULL;
	AlmanahCalendarEventFactoryPrivate *priv = almanah_calendar_event_factory_get_instance_private (ALMANAH_CALENDAR_EVENT_FACTORY (event_factory));

	calendar_events = calendar_client_get_events (priv->client, CALENDAR_EVENT_ALL);

	for (e = calendar_events; e != NULL; e = g_slist_next (e)) {
		CalendarEvent *calendar_event = e->data;
		AlmanahEvent *event;

		/* Create a new event and use it to replace the CalendarEvent */
		if (calendar_event->type == CALENDAR_EVENT_TASK) {
			/* Task */
			time_t today_time, yesterday_time;

			today_time = date_to_time (date);
			yesterday_time = today_time - (24 * 60 * 60);

			/* Have to filter out tasks by date */
			if (calendar_event->event.task.start_time <= today_time &&
			    (calendar_event->event.task.completed_time == 0 || calendar_event->event.task.completed_time >= yesterday_time)) {
				event = ALMANAH_EVENT (almanah_calendar_task_event_new (calendar_event->event.task.uid, calendar_event->event.task.summary, calendar_event->event.task.start_time));
			} else {
				event = NULL;
			}
		} else {
			/* Appointment */
			event = ALMANAH_EVENT (almanah_calendar_appointment_event_new (calendar_event->event.appointment.summary, calendar_event->event.appointment.start_time));
		}

		if (event != NULL)
			events = g_slist_prepend (events, event);

		calendar_event_free (calendar_event);
	}
	g_slist_free (calendar_events);

	return events;
}
