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

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <stdlib.h>

#include "../main.h"
#include "../event.h"
#include "f-spot.h"
#include "../event-factory.h"
#include "../events/f-spot-photo.h"

static void almanah_f_spot_event_factory_dispose (GObject *object);
static void almanah_f_spot_event_factory_finalize (GObject *object);
static void query_events (AlmanahEventFactory *event_factory, GDate *date);
static GSList *get_events (AlmanahEventFactory *event_factory, GDate *date);
static void cancel_query (DBusGProxyCall *call, AlmanahFSpotEventFactory *self);

struct _AlmanahFSpotEventFactoryPrivate {
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GSList *photos;
	GSList *current_queries;
};

G_DEFINE_TYPE (AlmanahFSpotEventFactory, almanah_f_spot_event_factory, ALMANAH_TYPE_EVENT_FACTORY)
#define ALMANAH_F_SPOT_EVENT_FACTORY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_F_SPOT_EVENT_FACTORY, AlmanahFSpotEventFactoryPrivate))

static void
almanah_f_spot_event_factory_class_init (AlmanahFSpotEventFactoryClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AlmanahEventFactoryClass *event_factory_class = ALMANAH_EVENT_FACTORY_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahFSpotEventFactoryPrivate));

	gobject_class->dispose = almanah_f_spot_event_factory_dispose;
	gobject_class->finalize = almanah_f_spot_event_factory_finalize;

	event_factory_class->type_id = ALMANAH_EVENT_FACTORY_F_SPOT;
	event_factory_class->query_events = query_events;
	event_factory_class->get_events = get_events;
}

static void
almanah_f_spot_event_factory_init (AlmanahFSpotEventFactory *self)
{
	GError *error = NULL;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_F_SPOT_EVENT_FACTORY, AlmanahFSpotEventFactoryPrivate);

	self->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (self->priv->connection == NULL) {
		/* TODO: something neater than this */
		g_error ("Error connecting to D-Bus for F-Spot event factory: %s", error->message);
		g_error_free (error);
		exit (1);
	}

	/* Connect to the F-Spot D-Bus service */
	self->priv->proxy = dbus_g_proxy_new_for_name (self->priv->connection,
					       "org.gnome.FSpot",
					       "/org/gnome/FSpot/PhotoRemoteControl",
					       "org.gnome.FSpot.PhotoRemoteControl");
}

static void
almanah_f_spot_event_factory_dispose (GObject *object)
{
	AlmanahFSpotEventFactoryPrivate *priv = ALMANAH_F_SPOT_EVENT_FACTORY_GET_PRIVATE (object);

	if (priv->proxy != NULL)
		g_object_unref (priv->proxy);
	priv->proxy = NULL;

	if (priv->connection != NULL)
		dbus_g_connection_unref (priv->connection);
	priv->connection = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_f_spot_event_factory_parent_class)->dispose (object);
}

static void
almanah_f_spot_event_factory_finalize (GObject *object)
{
	AlmanahFSpotEventFactoryPrivate *priv = ALMANAH_F_SPOT_EVENT_FACTORY_GET_PRIVATE (object);

	g_slist_foreach (priv->current_queries, (GFunc) cancel_query, object);
	g_slist_free (priv->current_queries);

	g_slist_foreach (priv->photos, (GFunc) g_object_unref,  NULL);
	g_slist_free (priv->photos);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_f_spot_event_factory_parent_class)->finalize (object);
}

static void
query_info_complete_cb (DBusGProxy *proxy, DBusGProxyCall *call, AlmanahFSpotEventFactory *self)
{
	AlmanahFSpotEventFactoryPrivate *priv = self->priv;
	GError *error = NULL;
	GHashTable *properties;
	GValue *uri, *name;
	AlmanahEvent *event;

	/* Remove the call from the list of current queries */
	priv->current_queries = g_slist_remove (priv->current_queries, call);

	if (dbus_g_proxy_end_call (proxy, call, &error,
				   dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), &properties,
				   G_TYPE_INVALID) == FALSE) {
		g_error ("Error getting photo properties: %s", error->message);
		g_error_free (error);
		exit (1);
	}

	name = g_hash_table_lookup (properties, "Name");
	uri = g_hash_table_lookup (properties, "Uri");

	if (almanah->debug == TRUE)
		g_debug ("Received properties for F-Spot photo \"%s\".", g_value_get_string (name));

	/* Create a new AlmanahFSpotPhotoEvent for the photo and add it to the list */
	event = ALMANAH_EVENT (almanah_f_spot_photo_event_new (g_value_get_string (uri), g_value_get_string (name)));
	priv->photos = g_slist_prepend (priv->photos, event);
	g_signal_emit_by_name (self, "events-updated");

	g_hash_table_destroy (properties);
}

static void
query_events_complete_cb (DBusGProxy *proxy, DBusGProxyCall *call, AlmanahFSpotEventFactory *self)
{
	AlmanahFSpotEventFactoryPrivate *priv = self->priv;
	GError *error = NULL;
	GArray *id_array;
	guint i;

	/* Remove the call from the list of current queries */
	priv->current_queries = g_slist_remove (priv->current_queries, call);

	if (dbus_g_proxy_end_call (proxy, call, &error,
				   DBUS_TYPE_G_UINT_ARRAY, &id_array,
				   G_TYPE_INVALID) == FALSE) {
		/* Ignore errors about unknown services or methods; it means the F-Spot service isn't available or is out-of-date */
		if (g_error_matches (error, DBUS_GERROR, DBUS_GERROR_SERVICE_UNKNOWN) == TRUE ||
		    g_error_matches (error, DBUS_GERROR, DBUS_GERROR_UNKNOWN_METHOD) == TRUE)
			return;

		g_error ("Error getting photo list from F-Spot: %s", error->message);
		g_error_free (error);
		exit (1);
	}

	if (almanah->debug == TRUE)
		g_debug ("Received list of photos from F-Spot.");

	/* Now query for the photo information */
	for (i = 0; i < id_array->len; i++) {
		DBusGProxyCall *sub_call;
		guint photo_id;

		photo_id = g_array_index (id_array, guint, i);
		sub_call = dbus_g_proxy_begin_call (priv->proxy, "GetPhotoProperties", 
						    (DBusGProxyCallNotify) query_info_complete_cb, g_object_ref (self), g_object_unref,
						    G_TYPE_UINT, photo_id,
						    G_TYPE_INVALID);
		priv->current_queries = g_slist_prepend (priv->current_queries, sub_call);
	}
	g_array_free (id_array, TRUE);
}

static inline gint64
date_to_int64 (GDate *date)
{
	struct tm localtime_tm = { 0, };

	localtime_tm.tm_mday = g_date_get_day (date);
	localtime_tm.tm_mon = g_date_get_month (date) - 1;
	localtime_tm.tm_year = g_date_get_year (date) - 1900;
	localtime_tm.tm_isdst = -1;

	return mktime (&localtime_tm);
}

static void
cancel_query (DBusGProxyCall *call, AlmanahFSpotEventFactory *self)
{
	dbus_g_proxy_cancel_call (self->priv->proxy, call);
}

static void
query_events (AlmanahEventFactory *event_factory, GDate *date)
{
	DBusGProxyCall *call;
	GDate next_date;
	AlmanahFSpotEventFactoryPrivate *priv = ALMANAH_F_SPOT_EVENT_FACTORY (event_factory)->priv;

	/* Cancel all current queries */
	g_slist_foreach (priv->current_queries, (GFunc) cancel_query, event_factory);
	g_slist_free (priv->current_queries);
	priv->current_queries = NULL;

	/* Create a new list of photos for the new query */
	g_slist_foreach (priv->photos, (GFunc) g_object_unref, NULL);
	g_slist_free (priv->photos);
	priv->photos = NULL;

	/* Sort out the dates */
	next_date = *date;
	g_date_add_days (&next_date, 1);

	/* Execute the D-Bus query */
	call = dbus_g_proxy_begin_call (priv->proxy, "QueryByDate",
					(DBusGProxyCallNotify) query_events_complete_cb, g_object_ref (event_factory), g_object_unref,
					G_TYPE_INT64, date_to_int64 (date),
					G_TYPE_INT64, date_to_int64 (&next_date),
					G_TYPE_INVALID);

	/* Append the query to the list of current queries, so that it can be cancelled in future if we find
	 * we need to query for events for a different date. */
	priv->current_queries = g_slist_prepend (priv->current_queries, call);
}

static GSList *
get_events (AlmanahEventFactory *event_factory, GDate *date)
{
	GSList *photos = ALMANAH_F_SPOT_EVENT_FACTORY (event_factory)->priv->photos;
	g_slist_foreach (photos, (GFunc) g_object_ref, NULL);
	return g_slist_copy (photos);
}
