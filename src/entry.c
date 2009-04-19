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

#include <config.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "entry.h"
#include "main.h"

static void almanah_entry_finalize (GObject *object);
static void almanah_entry_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_entry_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

struct _AlmanahEntryPrivate {
	GDate date;
	guint8 *data;
	gsize length;
	gboolean is_empty;
	gboolean is_important;
};

enum {
	PROP_DAY = 1,
	PROP_MONTH,
	PROP_YEAR,
	PROP_IS_IMPORTANT
};

G_DEFINE_TYPE (AlmanahEntry, almanah_entry, G_TYPE_OBJECT)
#define ALMANAH_ENTRY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_ENTRY, AlmanahEntryPrivate))

static void
almanah_entry_class_init (AlmanahEntryClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahEntryPrivate));

	gobject_class->set_property = almanah_entry_set_property;
	gobject_class->get_property = almanah_entry_get_property;
	gobject_class->finalize = almanah_entry_finalize;

	g_object_class_install_property (gobject_class, PROP_DAY,
				g_param_spec_uint ("day",
					"Day", "The day for which this is the entry.",
					1, 31, 1,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_MONTH,
				g_param_spec_uint ("month",
					"Month", "The month for which this is the entry.",
					1, 12, 1,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_YEAR,
				g_param_spec_uint ("year",
					"Year", "The year for which this is the entry.",
					1, (1 << 16) - 1, 1,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_IS_IMPORTANT,
				g_param_spec_boolean ("is-important",
					"Important?", "Whether the entry is particularly important to the user.",
					FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_entry_init (AlmanahEntry *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_ENTRY, AlmanahEntryPrivate);
	self->priv->data = NULL;
	self->priv->length = 0;
	g_date_clear (&(self->priv->date), 1);
}

static void
almanah_entry_finalize (GObject *object)
{
	AlmanahEntryPrivate *priv = ALMANAH_ENTRY (object)->priv;

	g_free (priv->data);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_entry_parent_class)->finalize (object);
}

static void
almanah_entry_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahEntryPrivate *priv = ALMANAH_ENTRY (object)->priv;

	switch (property_id) {
		case PROP_DAY:
			g_value_set_uint (value, g_date_get_day (&(priv->date)));
			break;
		case PROP_MONTH:
			g_value_set_uint (value, g_date_get_month (&(priv->date)));
			break;
		case PROP_YEAR:
			g_value_set_uint (value, g_date_get_year (&(priv->date)));
			break;
		case PROP_IS_IMPORTANT:
			g_value_set_boolean (value, priv->is_important);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_entry_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahEntryPrivate *priv = ALMANAH_ENTRY (object)->priv;

	switch (property_id) {
		case PROP_DAY:
			g_date_set_day (&(priv->date), g_value_get_uint (value));
			break;
		case PROP_MONTH:
			g_date_set_month (&(priv->date), g_value_get_uint (value));
			break;
		case PROP_YEAR:
			g_date_set_year (&(priv->date), g_value_get_uint (value));
			break;
		case PROP_IS_IMPORTANT:
			priv->is_important = g_value_get_boolean (value);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

AlmanahEntry *
almanah_entry_new (GDate *date)
{
	return g_object_new (ALMANAH_TYPE_ENTRY,
			     "day", g_date_get_day (date),
			     "month", g_date_get_month (date),
			     "year", g_date_get_year (date),
			     NULL);
}

/* NOTE: There's a difference between content and data, as recognised by AlmanahEntry.
 * Content is deserialized, and handled in terms of GtkTextBuffers.
 * Data is serialized, and handled in terms of a guint8 *data and gsize length. */
const guint8 *
almanah_entry_get_data (AlmanahEntry *self, gsize *length)
{
	if (length != NULL)
		*length = self->priv->length;
	return self->priv->data;
}

void
almanah_entry_set_data (AlmanahEntry *self, const guint8 *data, gsize length)
{
	AlmanahEntryPrivate *priv = self->priv;

	g_free (priv->data);

	priv->data = g_memdup (data, length * sizeof (*data));
	priv->length = length;
	priv->is_empty = FALSE;
}

gboolean
almanah_entry_get_content (AlmanahEntry *self, GtkTextBuffer *text_buffer, gboolean create_tags, GError **error)
{
	GdkAtom format_atom;
	GtkTextIter start_iter;
	AlmanahEntryPrivate *priv = self->priv;
	GError *deserialise_error = NULL;

	format_atom = gtk_text_buffer_register_deserialize_tagset (text_buffer, PACKAGE_NAME);
	gtk_text_buffer_deserialize_set_can_create_tags (text_buffer, format_atom, create_tags);
	gtk_text_buffer_get_start_iter (text_buffer, &start_iter);

	/* Try deserializing the (hopefully) serialized data first */
	if (gtk_text_buffer_deserialize (text_buffer, text_buffer,
					 format_atom,
					 &start_iter,
					 priv->data, priv->length,
					 &deserialise_error) == FALSE) {
		/* Since that failed, check the data's in the old format, and try to just load it as text */
		if (g_strcmp0 ((gchar*) priv->data, "GTKTEXTBUFFERCONTENTS-0001") != 0) {
			gtk_text_buffer_set_text (text_buffer, (gchar*) priv->data, priv->length);
			g_error_free (deserialise_error);
			return TRUE;
		}

		g_propagate_error (error, deserialise_error);
		return FALSE;
	}

	return TRUE;
}

void
almanah_entry_set_content (AlmanahEntry *self, GtkTextBuffer *text_buffer)
{
	GtkTextIter start, end;
	GdkAtom format_atom;
	AlmanahEntryPrivate *priv = self->priv;

	/* Update our cached empty status */
	self->priv->is_empty = (gtk_text_buffer_get_char_count (text_buffer) == 0) ? TRUE : FALSE;

	g_free (priv->data);

	gtk_text_buffer_get_bounds (text_buffer, &start, &end);
	format_atom = gtk_text_buffer_register_serialize_tagset (text_buffer, PACKAGE_NAME);
	priv->data = gtk_text_buffer_serialize (text_buffer, text_buffer,
						format_atom,
						&start, &end,
						&(priv->length));
}

/* NOTE: Designed for use on the stack */
void
almanah_entry_get_date (AlmanahEntry *self, GDate *date)
{
	g_date_set_dmy (date,
			g_date_get_day (&(self->priv->date)),
			g_date_get_month (&(self->priv->date)),
			g_date_get_year (&(self->priv->date)));
}

AlmanahEntryEditability
almanah_entry_get_editability (AlmanahEntry *self)
{
	GDate current_date;
	gint days_between;

	/* Entries are always editable if we have import mode on */
	if (almanah->import_mode == TRUE)
		return ALMANAH_ENTRY_EDITABLE;

	g_date_set_time_t (&current_date, time (NULL));

	/* Entries can't be edited before they've happened */
	days_between = g_date_days_between (&(self->priv->date), &current_date);

	if (days_between < 0)
		return ALMANAH_ENTRY_FUTURE;
	else if (days_between > ALMANAH_ENTRY_CUTOFF_AGE)
		return ALMANAH_ENTRY_PAST;
	else
		return ALMANAH_ENTRY_EDITABLE;
}

gboolean
almanah_entry_is_empty (AlmanahEntry *self)
{
	return (self->priv->is_empty == TRUE ||
		self->priv->length == 0 ||
		self->priv->data == NULL ||
		self->priv->data[0] == '\0') ? TRUE : FALSE;
}

gboolean
almanah_entry_is_important (AlmanahEntry *self)
{
	return self->priv->is_important;
}

void
almanah_entry_set_is_important (AlmanahEntry *self, gboolean is_important)
{
	self->priv->is_important = is_important;
	g_object_notify (G_OBJECT (self), "is-important");
}
