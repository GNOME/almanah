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

#ifndef ALMANAH_CALENDAR_EVENT_FACTORY_H
#define ALMANAH_CALENDAR_EVENT_FACTORY_H

#include <glib.h>
#include <glib-object.h>

#include "../event-factory.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_CALENDAR_EVENT_FACTORY		(almanah_calendar_event_factory_get_type ())
#define ALMANAH_CALENDAR_EVENT_FACTORY(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_CALENDAR_EVENT_FACTORY, AlmanahCalendarEventFactory))
#define ALMANAH_CALENDAR_EVENT_FACTORY_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_CALENDAR_EVENT_FACTORY, AlmanahCalendarEventFactoryClass))
#define ALMANAH_IS_CALENDAR_EVENT_FACTORY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_CALENDAR_EVENT_FACTORY))
#define ALMANAH_IS_CALENDAR_EVENT_FACTORY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_CALENDAR_EVENT_FACTORY))
#define ALMANAH_CALENDAR_EVENT_FACTORY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_CALENDAR_EVENT_FACTORY, AlmanahCalendarEventFactoryClass))

typedef struct {
	AlmanahEventFactory parent;
} AlmanahCalendarEventFactory;

typedef struct {
	AlmanahEventFactoryClass parent;
} AlmanahCalendarEventFactoryClass;

GType almanah_calendar_event_factory_get_type (void);

G_END_DECLS

#endif /* !ALMANAH_CALENDAR_EVENT_FACTORY_H */
