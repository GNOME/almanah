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

#include "event-factory.h"
#include "enums.h"

static void almanah_event_factory_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);

typedef struct {
	GDate date;
} AlmanahEventFactoryPrivate;

enum {
	PROP_TYPE_ID = 1
};

enum {
	SIGNAL_EVENTS_UPDATED,
	LAST_SIGNAL
};

static guint event_factory_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (AlmanahEventFactory, almanah_event_factory, G_TYPE_OBJECT)

static void
almanah_event_factory_class_init (AlmanahEventFactoryClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = almanah_event_factory_get_property;

	g_object_class_install_property (gobject_class, PROP_TYPE_ID,
				g_param_spec_enum ("type-id",
					"Type ID", "The type ID of this event factory.",
					ALMANAH_TYPE_EVENT_FACTORY_TYPE, ALMANAH_EVENT_FACTORY_UNKNOWN,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	event_factory_signals[SIGNAL_EVENTS_UPDATED] = g_signal_new ("events-updated",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				0, NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);
}

static void
almanah_event_factory_init (AlmanahEventFactory *self)
{
}

static void
almanah_event_factory_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahEventFactoryClass *klass = ALMANAH_EVENT_FACTORY_GET_CLASS (object);

	switch (property_id) {
		case PROP_TYPE_ID:
			g_value_set_enum (value, klass->type_id);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

AlmanahEventFactoryType
almanah_event_factory_get_type_id (AlmanahEventFactory *self)
{
	AlmanahEventFactoryClass *klass = ALMANAH_EVENT_FACTORY_GET_CLASS (self);
	return klass->type_id;
}

void
almanah_event_factory_query_events (AlmanahEventFactory *self, GDate *date)
{
	AlmanahEventFactoryClass *klass = ALMANAH_EVENT_FACTORY_GET_CLASS (self);
	g_assert (klass->query_events != NULL);
	return klass->query_events (self, date);
}

GSList *
almanah_event_factory_get_events (AlmanahEventFactory *self, GDate *date)
{
	AlmanahEventFactoryClass *klass = ALMANAH_EVENT_FACTORY_GET_CLASS (self);
	g_assert (klass->get_events != NULL);
	return klass->get_events (self, date);
}
