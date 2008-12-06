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

#ifndef ALMANAH_LINK_FACTORY_H
#define ALMANAH_LINK_FACTORY_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	ALMANAH_LINK_FACTORY_UNKNOWN = 0,
	ALMANAH_LINK_FACTORY_CALENDAR = 1
} AlmanahLinkFactoryType;

#define ALMANAH_TYPE_LINK_FACTORY		(almanah_link_factory_get_type ())
#define ALMANAH_LINK_FACTORY(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_LINK_FACTORY, AlmanahLinkFactory))
#define ALMANAH_LINK_FACTORY_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_LINK_FACTORY, AlmanahLinkFactoryClass))
#define ALMANAH_IS_LINK_FACTORY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_LINK_FACTORY))
#define ALMANAH_IS_LINK_FACTORY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_LINK_FACTORY))
#define ALMANAH_LINK_FACTORY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_LINK_FACTORY, AlmanahLinkFactoryClass))

typedef struct _AlmanahLinkFactoryPrivate	AlmanahLinkFactoryPrivate;

typedef struct {
	GObject parent;
	AlmanahLinkFactoryPrivate *priv;
} AlmanahLinkFactory;

typedef struct {
	GObjectClass parent;

	AlmanahLinkFactoryType type_id;

	void (*query_links) (AlmanahLinkFactory *link_factory, GDate *date);
	GSList *(*get_links) (AlmanahLinkFactory *link_factory, GDate *date);
} AlmanahLinkFactoryClass;

GType almanah_link_factory_get_type (void);

AlmanahLinkFactoryType almanah_link_factory_get_type_id (AlmanahLinkFactory *self);

void almanah_link_factory_query_links (AlmanahLinkFactory *self, GDate *date);
GSList *almanah_link_factory_get_links (AlmanahLinkFactory *self, GDate *date);

G_END_DECLS

#endif /* !ALMANAH_LINK_FACTORY_H */
