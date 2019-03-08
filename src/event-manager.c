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

#include "event-manager.h"
#include "event-factory.h"
#include "enums.h"

typedef struct {
	AlmanahEventFactoryType type_id;
	GType (*type_function) (void);
} EventFactoryType;

/* TODO: This is still a little hacky */

#ifdef HAVE_EVO
#include "calendar.h"
#endif /* HAVE_EVO */

const EventFactoryType event_factory_types[] = {
#ifdef HAVE_EVO
	{ ALMANAH_EVENT_FACTORY_CALENDAR, almanah_calendar_event_factory_get_type },
#endif /* HAVE_EVO */
};

static void almanah_event_manager_dispose (GObject *object);
static void events_updated_cb (AlmanahEventFactory *factory, AlmanahEventManager *self);

typedef struct {
	AlmanahEventFactory **factories;
} AlmanahEventManagerPrivate;

struct _AlmanahEventManager {
	GObject parent;
};

enum {
	SIGNAL_EVENTS_UPDATED,
	LAST_SIGNAL
};

static guint event_manager_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahEventManager, almanah_event_manager, G_TYPE_OBJECT)

static void
almanah_event_manager_class_init (AlmanahEventManagerClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = almanah_event_manager_dispose;

	event_manager_signals[SIGNAL_EVENTS_UPDATED] = g_signal_new ("events-updated",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				0, NULL, NULL,
				g_cclosure_marshal_VOID__ENUM,
				G_TYPE_NONE, 1, ALMANAH_TYPE_EVENT_FACTORY_TYPE);
}

static void
almanah_event_manager_init (AlmanahEventManager *self)
{
	AlmanahEventManagerPrivate *priv = almanah_event_manager_get_instance_private (self);
	guint i;

	/* Set up the list of AlmanahEventFactories */
	priv->factories = g_new (AlmanahEventFactory*, G_N_ELEMENTS (event_factory_types) + 1);
	for (i = 0; i < G_N_ELEMENTS (event_factory_types); i++) {
		priv->factories[i] = g_object_new (event_factory_types[i].type_function (), NULL);
		g_signal_connect (priv->factories[i], "events-updated", G_CALLBACK (events_updated_cb), self);
	}
	priv->factories[i] = NULL;
}

static void
almanah_event_manager_dispose (GObject *object)
{
	AlmanahEventManagerPrivate *priv = almanah_event_manager_get_instance_private (ALMANAH_EVENT_MANAGER (object));
	guint i = 0;

	/* Free the factories */
	if (priv->factories != NULL) {
		for (i = 0; priv->factories[i] != NULL; i++)
			g_object_unref (priv->factories[i]);
		g_free (priv->factories);
	}
	priv->factories = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_event_manager_parent_class)->dispose (object);
}

AlmanahEventManager *
almanah_event_manager_new (void)
{
	return g_object_new (ALMANAH_TYPE_EVENT_MANAGER, NULL);
}

static void
events_updated_cb (AlmanahEventFactory *factory, AlmanahEventManager *self)
{
	g_signal_emit (self, event_manager_signals[SIGNAL_EVENTS_UPDATED], 0, almanah_event_factory_get_type_id (factory));
}

void
almanah_event_manager_query_events (AlmanahEventManager *self, AlmanahEventFactoryType type_id, GDate *date)
{
	AlmanahEventManagerPrivate *priv = almanah_event_manager_get_instance_private (self);
	guint i;

	g_debug ("almanah_event_manager_query_events called for factory %u and date %u-%u-%u.", type_id,
	         g_date_get_year (date), g_date_get_month (date), g_date_get_day (date));

	if (type_id != ALMANAH_EVENT_FACTORY_UNKNOWN) {
		/* Just query that factory */
		for (i = 0; priv->factories[i] != NULL; i++) {
			if (almanah_event_factory_get_type_id (priv->factories[i]) == type_id)
				almanah_event_factory_query_events (priv->factories[i], date);
		}

		return;
	}

	/* Otherwise, query all factories */
	for (i = 0; priv->factories[i] != NULL; i++)
		almanah_event_factory_query_events (priv->factories[i], date);
}

GSList *
almanah_event_manager_get_events (AlmanahEventManager *self, AlmanahEventFactoryType type_id, GDate *date)
{
	AlmanahEventManagerPrivate *priv = almanah_event_manager_get_instance_private (self);
	GSList *list = NULL, *end = NULL;
	guint i;

	g_debug ("almanah_event_manager_get_events called for factory %u and date %u-%u-%u.", type_id,
	         g_date_get_year (date), g_date_get_month (date), g_date_get_day (date));

	if (type_id != ALMANAH_EVENT_FACTORY_UNKNOWN) {
		/* Just return the events for the specified event factory */
		for (i = 0; priv->factories[i] != NULL; i++) {
			if (almanah_event_factory_get_type_id (priv->factories[i]) == type_id)
				return almanah_event_factory_get_events (priv->factories[i], date);
		}

		return NULL;
	}

	/* Otherwise, return a concatenation of all factories' events */
	for (i = 0; priv->factories[i] != NULL; i++) {
		GSList *end2;

		end2 = almanah_event_factory_get_events (priv->factories[i], date);
		end = g_slist_concat (end, end2); /* assignment's only to shut gcc up */
		end = end2;

		if (list == NULL)
			list = end;
	}

	return list;
}
