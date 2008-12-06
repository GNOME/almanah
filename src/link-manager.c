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

#include "link-manager.h"
#include "link-factory.h"
#include "link-factory-builtins.h"

typedef struct {
	AlmanahLinkFactoryType type_id;
	GType (*type_function) (void);
} LinkFactoryType;

/* TODO: This is still a little hacky */

#include "calendar.h"
#include "main.h"

const LinkFactoryType link_factory_types[] = {
	{ ALMANAH_LINK_FACTORY_CALENDAR, almanah_calendar_link_factory_get_type }
};

static void almanah_link_manager_init (AlmanahLinkManager *self);
static void almanah_link_manager_dispose (GObject *object);
static void links_updated_cb (AlmanahLinkFactory *factory, AlmanahLinkManager *self);

struct _AlmanahLinkManagerPrivate {
	AlmanahLinkFactory **factories;
};

enum {
	SIGNAL_LINKS_UPDATED,
	LAST_SIGNAL
};

static guint link_manager_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (AlmanahLinkManager, almanah_link_manager, G_TYPE_OBJECT)
#define ALMANAH_LINK_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_LINK_MANAGER, AlmanahLinkManagerPrivate))

static void
almanah_link_manager_class_init (AlmanahLinkManagerClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahLinkManagerPrivate));

	gobject_class->dispose = almanah_link_manager_dispose;

	link_manager_signals[SIGNAL_LINKS_UPDATED] = g_signal_new ("links-updated",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				0, NULL, NULL,
				g_cclosure_marshal_VOID__ENUM,
				G_TYPE_NONE, 1, ALMANAH_TYPE_LINK_FACTORY_TYPE);
}

static void
almanah_link_manager_init (AlmanahLinkManager *self)
{
	guint i;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_LINK_MANAGER, AlmanahLinkManagerPrivate);

	/* Set up the list of AlmanahLinkFactories */
	self->priv->factories = g_new (AlmanahLinkFactory*, G_N_ELEMENTS (link_factory_types) + 1);
	for (i = 0; i < G_N_ELEMENTS (link_factory_types); i++) {
		self->priv->factories[i] = g_object_new (link_factory_types[i].type_function (), NULL);
		g_signal_connect (self->priv->factories[i], "links-updated", G_CALLBACK (links_updated_cb), self);
	}
	self->priv->factories[i] = NULL;
}

static void
almanah_link_manager_dispose (GObject *object)
{
	guint i = 0;
	AlmanahLinkManagerPrivate *priv = ALMANAH_LINK_MANAGER_GET_PRIVATE (object);

	/* Free the factories */
	if (priv->factories != NULL) {
		for (i = 0; priv->factories[i] != NULL; i++)
			g_object_unref (priv->factories[i]);
		g_free (priv->factories);
	}
	priv->factories = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_link_manager_parent_class)->dispose (object);
}

AlmanahLinkManager *
almanah_link_manager_new (void)
{
	return g_object_new (ALMANAH_TYPE_LINK_MANAGER, NULL);
}

static void
links_updated_cb (AlmanahLinkFactory *factory, AlmanahLinkManager *self)
{
	g_signal_emit (self, link_manager_signals[SIGNAL_LINKS_UPDATED], 0, almanah_link_factory_get_type_id (factory));
}

void
almanah_link_manager_query_links (AlmanahLinkManager *self, AlmanahLinkFactoryType type_id, GDate *date)
{
	AlmanahLinkManagerPrivate *priv = ALMANAH_LINK_MANAGER_GET_PRIVATE (self);
	guint i;

	if (almanah->debug == TRUE)
		g_debug ("almanah_link_manager_query_links called for factory %u and date %u-%u-%u.", type_id, g_date_get_year (date), g_date_get_month (date), g_date_get_day (date));

	if (type_id != ALMANAH_LINK_FACTORY_UNKNOWN) {
		/* Just query that factory */
		for (i = 0; priv->factories[i] != NULL; i++) {
			if (almanah_link_factory_get_type_id (priv->factories[i]) == type_id)
				almanah_link_factory_query_links (priv->factories[i], date);
		}

		return;
	}

	/* Otherwise, query all factories */
	for (i = 0; priv->factories[i] != NULL; i++)
		almanah_link_factory_query_links (priv->factories[i], date);
}

GSList *
almanah_link_manager_get_links (AlmanahLinkManager *self, AlmanahLinkFactoryType type_id, GDate *date)
{
	AlmanahLinkManagerPrivate *priv = ALMANAH_LINK_MANAGER_GET_PRIVATE (self);
	GSList *list = NULL, *end = NULL;
	guint i;

	if (almanah->debug == TRUE)
		g_debug ("almanah_link_manager_get_links called for factory %u and date %u-%u-%u.", type_id, g_date_get_year (date), g_date_get_month (date), g_date_get_day (date));

	if (type_id != ALMANAH_LINK_FACTORY_UNKNOWN) {
		/* Just return the links for the specified link factory */
		for (i = 0; priv->factories[i] != NULL; i++) {
			if (almanah_link_factory_get_type_id (priv->factories[i]) == type_id)
				return almanah_link_factory_get_links (priv->factories[i], date);
		}

		return NULL;
	}

	/* Otherwise, return a concatenation of all factories' links */
	for (i = 0; priv->factories[i] != NULL; i++) {
		GSList *end2;

		end2 = almanah_link_factory_get_links (priv->factories[i], date);
		end = g_slist_concat (end, end2); /* assignment's only to shut gcc up */
		end = end2;

		if (list == NULL)
			list = end;
	}

	return list;
}
