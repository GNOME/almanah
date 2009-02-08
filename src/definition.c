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

#include "definition.h"
#include "definition-builtins.h"

typedef struct {
	AlmanahDefinitionType type_id;
	GType (*type_function) (void);
} DefinitionType;

/* TODO: This is still a little hacky */

#include "file.h"
#include "note.h"
#include "uri.h"
#include "contact.h"

const DefinitionType definition_types[] = {
	{ ALMANAH_DEFINITION_FILE, almanah_file_definition_get_type },
	{ ALMANAH_DEFINITION_NOTE, almanah_note_definition_get_type },
	{ ALMANAH_DEFINITION_URI, almanah_uri_definition_get_type },
	{ ALMANAH_DEFINITION_CONTACT, almanah_contact_definition_get_type }
};

static void almanah_definition_init (AlmanahDefinition *self);
static void almanah_definition_finalize (GObject *object);
static void almanah_definition_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_definition_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

struct _AlmanahDefinitionPrivate {
	gchar *text;
	gchar *value;
	gchar *value2;
};

enum {
	PROP_TYPE_ID = 1,
	PROP_NAME,
	PROP_DESCRIPTION,
	PROP_ICON_NAME,
	PROP_TEXT,
	PROP_VALUE,
	PROP_VALUE2
};

G_DEFINE_ABSTRACT_TYPE (AlmanahDefinition, almanah_definition, G_TYPE_OBJECT)
#define ALMANAH_DEFINITION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_DEFINITION, AlmanahDefinitionPrivate))

static void
almanah_definition_class_init (AlmanahDefinitionClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahDefinitionPrivate));

	gobject_class->set_property = almanah_definition_set_property;
	gobject_class->get_property = almanah_definition_get_property;
	gobject_class->finalize = almanah_definition_finalize;

	g_object_class_install_property (gobject_class, PROP_TYPE_ID,
				g_param_spec_enum ("type-id",
					"Type ID", "The type ID of this definition.",
					ALMANAH_TYPE_DEFINITION_TYPE, ALMANAH_DEFINITION_UNKNOWN,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_NAME,
				g_param_spec_string ("name",
					"Name", "The human-readable name for this definition type.",
					NULL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_DESCRIPTION,
				g_param_spec_string ("description",
					"Description", "The human-readable description for this definition type.",
					NULL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_ICON_NAME,
				g_param_spec_string ("icon-name",
					"Icon Name", "The icon name for this definition type.",
					NULL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_TEXT,
				g_param_spec_string ("text",
					"Text", "The text this definition defines.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_VALUE,
				g_param_spec_string ("value",
					"Value", "The first value of this definition.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_VALUE2,
				g_param_spec_string ("value2",
					"Value 2", "The second value of this definition.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_definition_init (AlmanahDefinition *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_DEFINITION, AlmanahDefinitionPrivate);
}

static void
almanah_definition_finalize (GObject *object)
{
	AlmanahDefinitionPrivate *priv = ALMANAH_DEFINITION (object)->priv;

	g_free (priv->value);
	g_free (priv->value2);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_definition_parent_class)->finalize (object);
}

static void
almanah_definition_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahDefinitionPrivate *priv = ALMANAH_DEFINITION (object)->priv;
	AlmanahDefinitionClass *klass = ALMANAH_DEFINITION_GET_CLASS (object);

	switch (property_id) {
		case PROP_TYPE_ID:
			g_value_set_enum (value, klass->type_id);
			break;
		case PROP_NAME:
			g_value_set_string (value, klass->name);
			break;
		case PROP_DESCRIPTION:
			g_value_set_string (value, klass->description);
			break;
		case PROP_ICON_NAME:
			g_value_set_string (value, klass->icon_name);
			break;
		case PROP_VALUE:
			g_value_set_string (value, priv->value);
			break;
		case PROP_VALUE2:
			g_value_set_string (value, priv->value2);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_definition_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahDefinition *self = ALMANAH_DEFINITION (object);

	switch (property_id) {
		case PROP_VALUE:
			almanah_definition_set_value (self, g_value_get_string (value));
			break;
		case PROP_VALUE2:
			almanah_definition_set_value2 (self, g_value_get_string (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

AlmanahDefinition *
almanah_definition_new (AlmanahDefinitionType type_id)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (definition_types); i++) {
		if (definition_types[i].type_id == type_id)
			return g_object_new (definition_types[i].type_function (), NULL);
	}

	return NULL;
}

AlmanahDefinitionType
almanah_definition_get_type_id (AlmanahDefinition *self)
{
	AlmanahDefinitionClass *klass = ALMANAH_DEFINITION_GET_CLASS (self);
	return klass->type_id;
}

const gchar *
almanah_definition_get_name (AlmanahDefinition *self)
{
	AlmanahDefinitionClass *klass = ALMANAH_DEFINITION_GET_CLASS (self);
	return klass->name;
}

const gchar *
almanah_definition_get_description (AlmanahDefinition *self)
{
	AlmanahDefinitionClass *klass = ALMANAH_DEFINITION_GET_CLASS (self);
	return klass->description;
}

const gchar *
almanah_definition_get_icon_name (AlmanahDefinition *self)
{
	AlmanahDefinitionClass *klass = ALMANAH_DEFINITION_GET_CLASS (self);
	return klass->icon_name;
}

gboolean
almanah_definition_view (AlmanahDefinition *self)
{
	AlmanahDefinitionClass *klass = ALMANAH_DEFINITION_GET_CLASS (self);
	g_assert (klass->view != NULL);
	return klass->view (self);
}

void
almanah_definition_build_dialog (AlmanahDefinition *self, GtkVBox *parent_vbox)
{
	AlmanahDefinitionClass *klass = ALMANAH_DEFINITION_GET_CLASS (self);
	g_assert (klass->build_dialog != NULL);
	return klass->build_dialog (self, parent_vbox);
}

void
almanah_definition_close_dialog (AlmanahDefinition *self, GtkVBox *parent_vbox)
{
	AlmanahDefinitionClass *klass = ALMANAH_DEFINITION_GET_CLASS (self);
	g_assert (klass->close_dialog != NULL);
	return klass->close_dialog (self, parent_vbox);
}

void
almanah_definition_parse_text (AlmanahDefinition *self, const gchar *text)
{
	AlmanahDefinitionClass *klass = ALMANAH_DEFINITION_GET_CLASS (self);

	almanah_definition_set_text (self, text);

	g_assert (klass->parse_text != NULL);
	return klass->parse_text (self, text);
}

gchar *
almanah_definition_get_blurb (AlmanahDefinition *self)
{
	AlmanahDefinitionClass *klass = ALMANAH_DEFINITION_GET_CLASS (self);
	g_assert (klass->get_blurb != NULL);
	return klass->get_blurb (self);
}

const gchar *
almanah_definition_get_text (AlmanahDefinition *self)
{
	return self->priv->text;
}

void
almanah_definition_set_text (AlmanahDefinition *self, const gchar *text)
{
	g_free (self->priv->text);
	self->priv->text = g_strdup (text);
	g_object_notify (G_OBJECT (self), "text");
}

const gchar *
almanah_definition_get_value (AlmanahDefinition *self)
{
	return self->priv->value;
}

void
almanah_definition_set_value (AlmanahDefinition *self, const gchar *value)
{
	g_free (self->priv->value);
	self->priv->value = g_strdup (value);
	g_object_notify (G_OBJECT (self), "value");
}

const gchar *
almanah_definition_get_value2 (AlmanahDefinition *self)
{
	return self->priv->value2;
}

void
almanah_definition_set_value2 (AlmanahDefinition *self, const gchar *value)
{
	g_free (self->priv->value2);
	self->priv->value2 = g_strdup (value);
	g_object_notify (G_OBJECT (self), "value2");
}

void
almanah_definition_populate_model (GtkListStore *list_store, guint type_id_column, guint name_column, guint icon_name_column)
{
	GtkTreeIter iter;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (definition_types); i++) {
		AlmanahDefinition *definition = g_object_new (definition_types[i].type_function (), NULL);

		if (definition == NULL)
			continue;

		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter,
				    type_id_column, definition_types[i].type_id,
				    name_column, almanah_definition_get_name (definition),
				    icon_name_column, almanah_definition_get_icon_name (definition),
				    -1);
	}
}
