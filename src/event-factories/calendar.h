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

#define ALMANAH_TYPE_CALENDAR_EVENT_FACTORY     (almanah_calendar_event_factory_get_type ())

G_DECLARE_FINAL_TYPE (AlmanahCalendarEventFactory, almanah_calendar_event_factory, ALMANAH, CALENDAR_EVENT_FACTORY, AlmanahEventFactory)

G_END_DECLS

#endif /* !ALMANAH_CALENDAR_EVENT_FACTORY_H */
