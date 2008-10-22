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
#include <glib/gi18n.h>
#include <math.h>
#include <string.h>

#include "link.h"
#include "main.h"

typedef struct {
	gchar *type_id;
	GType (*type_function) (void);
} AlmanahLinkType;

/* TODO: This is still a little hacky */

#include "links/file.h"
#include "links/note.h"
#include "links/uri.h"

static const AlmanahLinkType link_types[] = {
	{ "file", &almanah_file_link_get_type },
	{ "note", &almanah_note_link_get_type },
	{ "uri", &almanah_uri_link_get_type }
};

static void almanah_link_init (AlmanahLink *self);
static void almanah_link_finalize (GObject *object);
static void almanah_link_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_link_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

struct _AlmanahLinkPrivate {
	gchar *value;
	gchar *value2;
};

enum {
	PROP_TYPE_ID = 1,
	PROP_NAME,
	PROP_DESCRIPTION,
	PROP_ICON_NAME,
	PROP_VALUE,
	PROP_VALUE2
};

G_DEFINE_ABSTRACT_TYPE (AlmanahLink, almanah_link, G_TYPE_OBJECT)
#define ALMANAH_LINK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_LINK, AlmanahLinkPrivate))

static void
almanah_link_class_init (AlmanahLinkClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahLinkPrivate));

	gobject_class->set_property = almanah_link_set_property;
	gobject_class->get_property = almanah_link_get_property;
	gobject_class->finalize = almanah_link_finalize;

	g_object_class_install_property (gobject_class, PROP_TYPE_ID,
				g_param_spec_string ("type-id",
					"Type ID", "The type ID of this link.",
					NULL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_NAME,
				g_param_spec_string ("name",
					"Name", "The human-readable name for this link type.",
					NULL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_DESCRIPTION,
				g_param_spec_string ("description",
					"Description", "The human-readable description for this link type.",
					NULL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_ICON_NAME,
				g_param_spec_string ("icon-name",
					"Icon Name", "The icon name for this link type.",
					NULL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_VALUE,
				g_param_spec_string ("value",
					"Value", "The first value of this link.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_VALUE2,
				g_param_spec_string ("value2",
					"Value 2", "The second value of this link.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_link_init (AlmanahLink *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_LINK, AlmanahLinkPrivate);
}

static void
almanah_link_finalize (GObject *object)
{
	AlmanahLinkPrivate *priv = ALMANAH_LINK (object)->priv;

	g_free (priv->value);
	g_free (priv->value2);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_link_parent_class)->finalize (object);
}

static void
almanah_link_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahLinkPrivate *priv = ALMANAH_LINK (object)->priv;
	AlmanahLinkClass *klass = ALMANAH_LINK_GET_CLASS (object);

	switch (property_id) {
		case PROP_TYPE_ID:
			g_value_set_string (value, g_strdup (klass->type_id));
			break;
		case PROP_NAME:
			g_value_set_string (value, g_strdup (klass->name));
			break;
		case PROP_DESCRIPTION:
			g_value_set_string (value, g_strdup (klass->description));
			break;
		case PROP_ICON_NAME:
			g_value_set_string (value, g_strdup (klass->icon_name));
			break;
		case PROP_VALUE:
			g_value_set_string (value, g_strdup (priv->value));
			break;
		case PROP_VALUE2:
			g_value_set_string (value, g_strdup (priv->value2));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_link_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahLinkPrivate *priv = ALMANAH_LINK (object)->priv;

	switch (property_id) {
		case PROP_VALUE:
			g_free (priv->value);
			priv->value = g_value_dup_string (value);
			break;
		case PROP_VALUE2:
			g_free (priv->value2);
			priv->value2 = g_value_dup_string (value);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

AlmanahLink *
almanah_link_new (const gchar *type_id)
{
	guint lower_limit, upper_limit;

	/* Do a binary search */
	lower_limit = 0;
	upper_limit = G_N_ELEMENTS (link_types) - 1;

	/* TODO: Use GQuarks to make things less heavy on the strcmp()s? */
	while (TRUE) {
		guint temp;
		gint comparison;

		temp = ceil ((lower_limit + upper_limit) / 2);
		comparison = strcmp (type_id, link_types[temp].type_id);

		/* Exit condition */
		if (lower_limit == upper_limit && comparison != 0)
			return NULL;

		if (comparison < 0) {
			upper_limit = temp - 1; /* It's in the lower half */
		} else if (comparison > 0) {
			lower_limit = temp + 1; /* It's in the upper half */
		} else {
			/* Match! */
			return g_object_new (link_types[temp].type_function (), NULL);
		}
	};

	return NULL;
}

gchar *
almanah_link_format_value (AlmanahLink *self)
{
	AlmanahLinkClass *klass = ALMANAH_LINK_GET_CLASS (self);
	g_assert (klass->format_value != NULL);
	return klass->format_value (self);
}

gboolean
almanah_link_view (AlmanahLink *self)
{
	AlmanahLinkClass *klass = ALMANAH_LINK_GET_CLASS (self);
	g_assert (klass->view != NULL);

	if (almanah->debug)
		g_debug ("Viewing %s link ('%s', '%s')", klass->type_id, self->priv->value, self->priv->value2);

	return klass->view (self);
}

void
almanah_link_build_dialog (AlmanahLink *self, GtkVBox *parent_vbox)
{
	AlmanahLinkClass *klass = ALMANAH_LINK_GET_CLASS (self);
	g_assert (klass->build_dialog != NULL);
	return klass->build_dialog (self, parent_vbox);
}

void
almanah_link_get_values (AlmanahLink *self)
{
	AlmanahLinkClass *klass = ALMANAH_LINK_GET_CLASS (self);
	g_assert (klass->get_values != NULL);
	return klass->get_values (self);
}

void
almanah_link_populate_model (GtkListStore *list_store, guint type_id_column, guint name_column, guint icon_name_column)
{
	GtkTreeIter iter;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (link_types); i++) {
		AlmanahLink *link = g_object_new (link_types[i].type_function (), NULL);

		if (link == NULL)
			continue;

		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter,
				    type_id_column, link_types[i].type_id,
				    name_column, almanah_link_get_name (link),
				    icon_name_column, almanah_link_get_icon_name (link),
				    -1);
	}
}

const gchar *
almanah_link_get_type_id (AlmanahLink *self)
{
	AlmanahLinkClass *klass = ALMANAH_LINK_GET_CLASS (self);
	return klass->type_id;
}

const gchar *
almanah_link_get_name (AlmanahLink *self)
{
	AlmanahLinkClass *klass = ALMANAH_LINK_GET_CLASS (self);
	return klass->name;
}

const gchar *
almanah_link_get_description (AlmanahLink *self)
{
	AlmanahLinkClass *klass = ALMANAH_LINK_GET_CLASS (self);
	return klass->description;
}

const gchar *
almanah_link_get_icon_name (AlmanahLink *self)
{
	AlmanahLinkClass *klass = ALMANAH_LINK_GET_CLASS (self);
	return klass->icon_name;
}

gchar *
almanah_link_get_value (AlmanahLink *self)
{
	return g_strdup (self->priv->value);
}

void
almanah_link_set_value (AlmanahLink *self, const gchar *value)
{
	g_free (self->priv->value);
	self->priv->value = g_strdup (value);
}

gchar *
almanah_link_get_value2 (AlmanahLink *self)
{
	return g_strdup (self->priv->value2);
}

/* TODO: Perhaps an almanah_link_set_values (AlmanahLink *self, const gchar *value, const gchar *value2) API would be better? */

void
almanah_link_set_value2 (AlmanahLink *self, const gchar *value)
{
	g_free (self->priv->value2);
	self->priv->value2 = g_strdup (value);
}

