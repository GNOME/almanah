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

#ifndef ALMANAH_EVENT_MANAGER_H
#define ALMANAH_EVENT_MANAGER_H

#include <glib.h>
#include <glib-object.h>

#include "event-factory.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_EVENT_MANAGER      (almanah_event_manager_get_type ())

G_DECLARE_FINAL_TYPE (AlmanahEventManager, almanah_event_manager, ALMANAH, EVENT_MANAGER, GObject)

AlmanahEventManager *almanah_event_manager_new (void);

void almanah_event_manager_query_events (AlmanahEventManager *self, AlmanahEventFactoryType type_id, GDate *date);
GSList *almanah_event_manager_get_events (AlmanahEventManager *self, AlmanahEventFactoryType type_id, GDate *date);

G_END_DECLS

#endif /* !ALMANAH_EVENT_MANAGER_H */
