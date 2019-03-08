/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2008-2009 <philip@tecnocode.co.uk>
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

#include "event.h"

static void almanah_event_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);

enum {
	PROP_NAME = 1,
	PROP_DESCRIPTION,
	PROP_ICON_NAME
};

G_DEFINE_ABSTRACT_TYPE (AlmanahEvent, almanah_event, G_TYPE_OBJECT)

static void
almanah_event_class_init (AlmanahEventClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = almanah_event_get_property;

	g_object_class_install_property (gobject_class, PROP_NAME,
				g_param_spec_string ("name",
					"Name", "The human-readable name for this event type.",
					NULL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_DESCRIPTION,
				g_param_spec_string ("description",
					"Description", "The human-readable description for this event type.",
					NULL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_ICON_NAME,
				g_param_spec_string ("icon-name",
					"Icon Name", "The icon name for this event type.",
					NULL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_event_init (AlmanahEvent *self)
{
}

static void
almanah_event_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahEventClass *klass = ALMANAH_EVENT_GET_CLASS (object);

	switch (property_id) {
		case PROP_NAME:
			g_value_set_string (value, klass->name);
			break;
		case PROP_DESCRIPTION:
			g_value_set_string (value, klass->description);
			break;
		case PROP_ICON_NAME:
			g_value_set_string (value, klass->icon_name);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

const gchar *
almanah_event_format_value (AlmanahEvent *self)
{
	AlmanahEventClass *klass = ALMANAH_EVENT_GET_CLASS (self);
	g_assert (klass->format_value != NULL);
	return klass->format_value (self);
}

const gchar *
almanah_event_format_time (AlmanahEvent *self)
{
	AlmanahEventClass *klass = ALMANAH_EVENT_GET_CLASS (self);
	g_assert (klass->format_time != NULL);
	return klass->format_time (self);
}

gboolean
almanah_event_view (AlmanahEvent *self, GtkWindow *parent_window)
{
	AlmanahEventClass *klass = ALMANAH_EVENT_GET_CLASS (self);
	g_assert (klass->view != NULL);
	return klass->view (self, parent_window);
}

const gchar *
almanah_event_get_name (AlmanahEvent *self)
{
	AlmanahEventClass *klass = ALMANAH_EVENT_GET_CLASS (self);
	return klass->name;
}

const gchar *
almanah_event_get_description (AlmanahEvent *self)
{
	AlmanahEventClass *klass = ALMANAH_EVENT_GET_CLASS (self);
	return klass->description;
}

const gchar *
almanah_event_get_icon_name (AlmanahEvent *self)
{
	AlmanahEventClass *klass = ALMANAH_EVENT_GET_CLASS (self);
	return klass->icon_name;
}
