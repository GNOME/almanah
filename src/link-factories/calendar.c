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

#include "../link.h"
#include "calendar.h"
#include "calendar-client.h"
#include "../link-factory.h"
#include "../links/calendar-appointment.h"
#include "../links/calendar-task.h"

static void almanah_calendar_link_factory_init (AlmanahCalendarLinkFactory *self);
static void almanah_calendar_link_factory_dispose (GObject *object);
static void query_links (AlmanahLinkFactory *link_factory, GDate *date);
static GSList *get_links (AlmanahLinkFactory *link_factory, GDate *date);
static void events_changed_cb (CalendarClient *client, AlmanahCalendarLinkFactory *self);

struct _AlmanahCalendarLinkFactoryPrivate {
	CalendarClient *client;
};

G_DEFINE_TYPE (AlmanahCalendarLinkFactory, almanah_calendar_link_factory, ALMANAH_TYPE_LINK_FACTORY)
#define ALMANAH_CALENDAR_LINK_FACTORY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_CALENDAR_LINK_FACTORY, AlmanahCalendarLinkFactoryPrivate))

static void
almanah_calendar_link_factory_class_init (AlmanahCalendarLinkFactoryClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AlmanahLinkFactoryClass *link_factory_class = ALMANAH_LINK_FACTORY_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahCalendarLinkFactoryPrivate));

	gobject_class->dispose = almanah_calendar_link_factory_dispose;

	link_factory_class->type_id = ALMANAH_LINK_FACTORY_CALENDAR;
	link_factory_class->query_links = query_links;
	link_factory_class->get_links = get_links;
}

static void
almanah_calendar_link_factory_init (AlmanahCalendarLinkFactory *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_CALENDAR_LINK_FACTORY, AlmanahCalendarLinkFactoryPrivate);
	self->priv->client = calendar_client_new ();

	g_signal_connect (self->priv->client, "tasks-changed", G_CALLBACK (events_changed_cb), self);
	g_signal_connect (self->priv->client, "appointments-changed", G_CALLBACK (events_changed_cb), self);
}

static void
almanah_calendar_link_factory_dispose (GObject *object)
{
	AlmanahCalendarLinkFactoryPrivate *priv = ALMANAH_CALENDAR_LINK_FACTORY_GET_PRIVATE (object);

	if (priv->client != NULL)
		g_object_unref (priv->client);
	priv->client = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_calendar_link_factory_parent_class)->dispose (object);
}

static void
query_links (AlmanahLinkFactory *link_factory, GDate *date)
{
	AlmanahCalendarLinkFactory *self = ALMANAH_CALENDAR_LINK_FACTORY (link_factory);

	calendar_client_select_day (self->priv->client, g_date_get_day (date));
	calendar_client_select_month (self->priv->client, g_date_get_month (date) - 1, g_date_get_year (date));
	g_signal_emit_by_name (self, "links-updated");
}

static void
events_changed_cb (CalendarClient *client, AlmanahCalendarLinkFactory *self)
{
	g_signal_emit_by_name (self, "links-updated");
}

static inline GTime
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
get_links (AlmanahLinkFactory *link_factory, GDate *date)
{
	GSList *events, *e, *links = NULL;
	AlmanahCalendarLinkFactoryPrivate *priv = ALMANAH_CALENDAR_LINK_FACTORY_GET_PRIVATE (link_factory);

	events = calendar_client_get_events (priv->client, CALENDAR_EVENT_ALL);

	for (e = events; e != NULL; e = g_slist_next (e)) {
		CalendarEvent *event = e->data;
		AlmanahLink *link;

		/* Create a new link and use it to replace the CalendarEvent */
		if (event->type == CALENDAR_EVENT_TASK) {
			/* Task */
			GTime today_time, yesterday_time;

			today_time = date_to_time (date);
			yesterday_time = today_time - (24 * 60 * 60);

			/* Have to filter out tasks by date */
			if (event->event.task.start_time <= today_time &&
			    (event->event.task.completed_time == 0 || event->event.task.completed_time >= yesterday_time)) {
				link = ALMANAH_LINK (almanah_calendar_task_link_new (event->event.task.uid, event->event.task.summary));
			} else {
				link = NULL;
			}
		} else {
			/* Appointment */
			link = ALMANAH_LINK (almanah_calendar_appointment_link_new (event->event.appointment.summary, event->event.appointment.start_time));
		}

		if (link != NULL)
			links = g_slist_prepend (links, link);

		calendar_event_free (event);
	}
	g_slist_free (events);

	return links;
}
