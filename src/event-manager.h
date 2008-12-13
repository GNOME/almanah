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

#define ALMANAH_TYPE_EVENT_MANAGER		(almanah_event_manager_get_type ())
#define ALMANAH_EVENT_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_EVENT_MANAGER, AlmanahEventManager))
#define ALMANAH_EVENT_MANAGER_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_EVENT_MANAGER, AlmanahEventManagerClass))
#define ALMANAH_IS_EVENT_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_EVENT_MANAGER))
#define ALMANAH_IS_EVENT_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_EVENT_MANAGER))
#define ALMANAH_EVENT_MANAGER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_EVENT_MANAGER, AlmanahEventManagerClass))

typedef struct _AlmanahEventManagerPrivate	AlmanahEventManagerPrivate;

typedef struct {
	GObject parent;
	AlmanahEventManagerPrivate *priv;
} AlmanahEventManager;

typedef struct {
	GObjectClass parent;
} AlmanahEventManagerClass;

GType almanah_event_manager_get_type (void);

AlmanahEventManager *almanah_event_manager_new (void);

void almanah_event_manager_query_events (AlmanahEventManager *self, AlmanahEventFactoryType type_id, GDate *date);
GSList *almanah_event_manager_get_events (AlmanahEventManager *self, AlmanahEventFactoryType type_id, GDate *date);

G_END_DECLS

#endif /* !ALMANAH_EVENT_MANAGER_H */
