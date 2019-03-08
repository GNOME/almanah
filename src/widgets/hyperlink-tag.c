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

typedef struct {
	gchar *uri;
} AlmanahHyperlinkTagPrivate;

struct _AlmanahHyperlinkTag {
	GtkTextTag parent;
};

enum {
	PROP_URI = 1
};

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahHyperlinkTag, almanah_hyperlink_tag, GTK_TYPE_TEXT_TAG)

static void
almanah_hyperlink_tag_class_init (AlmanahHyperlinkTagClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

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
	AlmanahHyperlinkTagPrivate *priv = almanah_hyperlink_tag_get_instance_private (self);

	priv->uri = NULL;
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
	AlmanahHyperlinkTagPrivate *priv = almanah_hyperlink_tag_get_instance_private (ALMANAH_HYPERLINK_TAG (object));

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
	AlmanahHyperlinkTagPrivate *priv = almanah_hyperlink_tag_get_instance_private (ALMANAH_HYPERLINK_TAG (object));

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

	AlmanahHyperlinkTagPrivate *priv = almanah_hyperlink_tag_get_instance_private (self);

	return priv->uri;
}

void
almanah_hyperlink_tag_set_uri (AlmanahHyperlinkTag *self, const gchar *uri)
{
	g_return_if_fail (ALMANAH_IS_HYPERLINK_TAG (self));
	g_return_if_fail (uri != NULL && *uri != '\0');

	AlmanahHyperlinkTagPrivate *priv = almanah_hyperlink_tag_get_instance_private (self);

	g_free (priv->uri);
	priv->uri = g_strdup (uri);;
	g_object_notify (G_OBJECT (self), "uri");
}
