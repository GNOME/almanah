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

#include "link-factory.h"
#include "link-factory-builtins.h"

static void almanah_link_factory_init (AlmanahLinkFactory *self);
static void almanah_link_factory_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);

struct _AlmanahLinkFactoryPrivate {
	GDate date;
};

enum {
	PROP_TYPE_ID = 1
};

enum {
	SIGNAL_LINKS_UPDATED,
	LAST_SIGNAL
};

static guint link_factory_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_ABSTRACT_TYPE (AlmanahLinkFactory, almanah_link_factory, G_TYPE_OBJECT)
#define ALMANAH_LINK_FACTORY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_LINK_FACTORY, AlmanahLinkFactoryPrivate))

static void
almanah_link_factory_class_init (AlmanahLinkFactoryClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahLinkFactoryPrivate));

	gobject_class->get_property = almanah_link_factory_get_property;

	g_object_class_install_property (gobject_class, PROP_TYPE_ID,
				g_param_spec_enum ("type-id",
					"Type ID", "The type ID of this link factory.",
					ALMANAH_TYPE_LINK_FACTORY_TYPE, ALMANAH_LINK_FACTORY_UNKNOWN,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	link_factory_signals[SIGNAL_LINKS_UPDATED] = g_signal_new ("links-updated",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				0, NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);
}

static void
almanah_link_factory_init (AlmanahLinkFactory *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_LINK_FACTORY, AlmanahLinkFactoryPrivate);
}

static void
almanah_link_factory_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahLinkFactoryClass *klass = ALMANAH_LINK_FACTORY_GET_CLASS (object);

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

AlmanahLinkFactoryType
almanah_link_factory_get_type_id (AlmanahLinkFactory *self)
{
	AlmanahLinkFactoryClass *klass = ALMANAH_LINK_FACTORY_GET_CLASS (self);
	return klass->type_id;
}

void
almanah_link_factory_query_links (AlmanahLinkFactory *self, GDate *date)
{
	AlmanahLinkFactoryClass *klass = ALMANAH_LINK_FACTORY_GET_CLASS (self);
	g_assert (klass->query_links != NULL);
	return klass->query_links (self, date);
}

GSList *
almanah_link_factory_get_links (AlmanahLinkFactory *self, GDate *date)
{
	AlmanahLinkFactoryClass *klass = ALMANAH_LINK_FACTORY_GET_CLASS (self);
	g_assert (klass->get_links != NULL);
	return klass->get_links (self, date);
}
