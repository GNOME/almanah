/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2011 <philip@tecnocode.co.uk>
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
#include <gtk/gtk.h>

#include "hyperlink-tag.h"

static void constructed (GObject *object);
static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void finalize (GObject *object);

struct _AlmanahHyperlinkTagPrivate {
	gchar *uri;
};

enum {
	PROP_URI = 1
};

G_DEFINE_TYPE (AlmanahHyperlinkTag, almanah_hyperlink_tag, GTK_TYPE_TEXT_TAG)

static void
almanah_hyperlink_tag_class_init (AlmanahHyperlinkTagClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahHyperlinkTagPrivate));

	gobject_class->constructed = constructed;
	gobject_class->get_property = get_property;
	gobject_class->set_property = set_property;
	gobject_class->finalize = finalize;

	g_object_class_install_property (gobject_class, PROP_URI,
	                                 g_param_spec_string ("uri",
	                                                      "URI", "The URI which this hyperlink points to.",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_hyperlink_tag_init (AlmanahHyperlinkTag *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_HYPERLINK_TAG, AlmanahHyperlinkTagPrivate);
	self->priv->uri = NULL;
}

static void
constructed (GObject *object)
{
	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_hyperlink_tag_parent_class)->constructed (object);

	/* Set our default appearance */
	g_object_set (object,
	              "foreground", "blue",
	              "underline", PANGO_UNDERLINE_SINGLE,
	              NULL);
}

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahHyperlinkTagPrivate *priv = ALMANAH_HYPERLINK_TAG (object)->priv;

	switch (property_id) {
		case PROP_URI:
			g_value_set_string (value, priv->uri);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_URI:
			almanah_hyperlink_tag_set_uri (ALMANAH_HYPERLINK_TAG (object), g_value_get_string (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
finalize (GObject *object)
{
	AlmanahHyperlinkTagPrivate *priv = ALMANAH_HYPERLINK_TAG (object)->priv;

	g_free (priv->uri);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_hyperlink_tag_parent_class)->finalize (object);
}

AlmanahHyperlinkTag *
almanah_hyperlink_tag_new (const gchar *uri)
{
	return g_object_new (ALMANAH_TYPE_HYPERLINK_TAG,
	                     "uri", uri,
	                     NULL);
}

const gchar *
almanah_hyperlink_tag_get_uri (AlmanahHyperlinkTag *self)
{
	g_return_val_if_fail (ALMANAH_IS_HYPERLINK_TAG (self), NULL);

	return self->priv->uri;
}

void
almanah_hyperlink_tag_set_uri (AlmanahHyperlinkTag *self, const gchar *uri)
{
	g_return_if_fail (ALMANAH_IS_HYPERLINK_TAG (self));
	g_return_if_fail (uri != NULL && *uri != '\0');

	g_free (self->priv->uri);
	self->priv->uri = g_strdup (uri);;
	g_object_notify (G_OBJECT (self), "uri");
}
