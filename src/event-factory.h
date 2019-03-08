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

#ifndef ALMANAH_EVENT_FACTORY_H
#define ALMANAH_EVENT_FACTORY_H

#include <config.h>
#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	ALMANAH_EVENT_FACTORY_UNKNOWN = 0,
	ALMANAH_EVENT_FACTORY_CALENDAR
} AlmanahEventFactoryType;

#define ALMANAH_TYPE_EVENT_FACTORY      (almanah_event_factory_get_type ())

G_DECLARE_DERIVABLE_TYPE (AlmanahEventFactory, almanah_event_factory, ALMANAH, EVENT_FACTORY, GObject)

struct _AlmanahEventFactoryClass {
	GObjectClass parent;

	AlmanahEventFactoryType type_id;

	void (*query_events) (AlmanahEventFactory *event_factory, GDate *date);
	GSList *(*get_events) (AlmanahEventFactory *event_factory, GDate *date);
};

AlmanahEventFactoryType almanah_event_factory_get_type_id (AlmanahEventFactory *self);

void almanah_event_factory_query_events (AlmanahEventFactory *self, GDate *date);
GSList *almanah_event_factory_get_events (AlmanahEventFactory *self, GDate *date);

G_END_DECLS

#endif /* !ALMANAH_EVENT_FACTORY_H */
