/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2009 <philip@tecnocode.co.uk>
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

#ifndef ALMANAH_F_SPOT_PHOTO_EVENT_H
#define ALMANAH_F_SPOT_PHOTO_EVENT_H

#include <glib.h>
#include <glib-object.h>

#include "event.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_F_SPOT_PHOTO_EVENT		(almanah_f_spot_photo_event_get_type ())
#define ALMANAH_F_SPOT_PHOTO_EVENT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_F_SPOT_PHOTO_EVENT, AlmanahFSpotPhotoEvent))
#define ALMANAH_F_SPOT_PHOTO_EVENT_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_F_SPOT_PHOTO_EVENT, AlmanahFSpotPhotoEventClass))
#define ALMANAH_IS_F_SPOT_PHOTO_EVENT(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_F_SPOT_PHOTO_EVENT))
#define ALMANAH_IS_F_SPOT_PHOTO_EVENT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_F_SPOT_PHOTO_EVENT))
#define ALMANAH_F_SPOT_PHOTO_EVENT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_F_SPOT_PHOTO_EVENT, AlmanahFSpotPhotoEventClass))

typedef struct _AlmanahFSpotPhotoEventPrivate	AlmanahFSpotPhotoEventPrivate;

typedef struct {
	AlmanahEvent parent;
	AlmanahFSpotPhotoEventPrivate *priv;
} AlmanahFSpotPhotoEvent;

typedef struct {
	AlmanahEventClass parent;
} AlmanahFSpotPhotoEventClass;

GType almanah_f_spot_photo_event_get_type (void);
AlmanahFSpotPhotoEvent *almanah_f_spot_photo_event_new (const gchar *uri, const gchar *name);

G_END_DECLS

#endif /* !ALMANAH_F_SPOT_PHOTO_EVENT_H */
