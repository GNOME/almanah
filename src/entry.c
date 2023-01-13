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
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "entry.h"
#include "widgets/hyperlink-tag.h"

GQuark
almanah_entry_error_quark (void)
{
	return g_quark_from_static_string ("almanah-entry-error-quark");
}

typedef enum {
	/* Unset */
	DATA_FORMAT_UNSET = 0,
	/* Plain text or GtkTextBuffer's default serialisation format, as used in Almanah versions <= 0.8.0 */
	DATA_FORMAT_PLAIN_TEXT__GTK_TEXT_BUFFER = 1,
	/* Custom XML serialisation format using schema data/entry-2.0.rnc. */
	DATA_FORMAT_XML_2_0 = 2,
} DataFormat;

static void almanah_entry_finalize (GObject *object);
static void almanah_entry_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_entry_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

static guint8 *serialise_entry_xml_2_0 (GtkTextBuffer *register_buffer, GtkTextBuffer *content_buffer, const GtkTextIter *start,
                                        const GtkTextIter *end, gsize *length, gpointer user_data);
static gboolean deserialise_entry_xml_2_0 (GtkTextBuffer *register_buffer, GtkTextBuffer *content_buffer, GtkTextIter *iter, const guint8 *data,
                                           gsize length, gboolean create_tags, gpointer user_data, GError **error);

typedef struct {
	GDate date;
	guint8 *data;
	gsize length;
	DataFormat version; /* version of the *format* used for ->data */
	gboolean is_empty;
	gboolean is_important;
	GDate last_edited; /* date the entry was last edited *in the database*; e.g. this isn't updated when almanah_entry_set_content() is called */
} AlmanahEntryPrivate;

struct _AlmanahEntry {
	GObject parent;
};

enum {
	PROP_DAY = 1,
	PROP_MONTH,
	PROP_YEAR,
	PROP_IS_IMPORTANT,
	PROP_LAST_EDITED_DAY,
	PROP_LAST_EDITED_MONTH,
	PROP_LAST_EDITED_YEAR
};

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahEntry, almanah_entry, G_TYPE_OBJECT)

static void
almanah_entry_class_init (AlmanahEntryClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

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
	g_object_class_install_property (gobject_class, PROP_LAST_EDITED_DAY,
				g_param_spec_uint ("last-edited-day",
					"Last Edited Day", "The day when this entry was last edited.",
					1, 31, 1,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_LAST_EDITED_MONTH,
				g_param_spec_uint ("last-edited-month",
					"Last Edited Month", "The month when this entry was last edited.",
					1, 12, 1,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_LAST_EDITED_YEAR,
				g_param_spec_uint ("last-edited-year",
					"Last Edited Year", "The year when this entry was last edited.",
					1, (1 << 16) - 1, 1,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_entry_init (AlmanahEntry *self)
{
	AlmanahEntryPrivate *priv = almanah_entry_get_instance_private (self);
	priv->data = NULL;
	priv->length = 0;
	priv->version = DATA_FORMAT_UNSET;
	g_date_clear (&(priv->date), 1);
	g_date_clear (&(priv->last_edited), 1);
}

static void
almanah_entry_finalize (GObject *object)
{
	AlmanahEntryPrivate *priv = almanah_entry_get_instance_private (ALMANAH_ENTRY (object));

	g_free (priv->data);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_entry_parent_class)->finalize (object);
}

static void
almanah_entry_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahEntryPrivate *priv = almanah_entry_get_instance_private (ALMANAH_ENTRY (object));

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
		case PROP_LAST_EDITED_DAY:
			g_value_set_uint (value, g_date_get_day (&(priv->last_edited)));
			break;
		case PROP_LAST_EDITED_MONTH:
			g_value_set_uint (value, g_date_get_month (&(priv->last_edited)));
			break;
		case PROP_LAST_EDITED_YEAR:
			g_value_set_uint (value, g_date_get_year (&(priv->last_edited)));
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
	AlmanahEntry *entry = ALMANAH_ENTRY (object);
	AlmanahEntryPrivate *priv = almanah_entry_get_instance_private (entry);

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
			almanah_entry_set_is_important (entry, g_value_get_boolean (value));
			break;
		case PROP_LAST_EDITED_DAY:
			g_date_set_day (&(priv->last_edited), g_value_get_uint (value));
			break;
		case PROP_LAST_EDITED_MONTH:
			g_date_set_month (&(priv->last_edited), g_value_get_uint (value));
			break;
		case PROP_LAST_EDITED_YEAR:
			g_date_set_year (&(priv->last_edited), g_value_get_uint (value));
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
 * Data is serialized, and handled in terms of a guint8 *data and gsize length, as well as an associated data format version.
 * Internally, the data format version is structured according to DataFormat; but externally it's just an opaque guint. */
const guint8 *
almanah_entry_get_data (AlmanahEntry *self, gsize *length, guint *version)
{
	AlmanahEntryPrivate *priv = almanah_entry_get_instance_private (self);

	if (length != NULL)
		*length = priv->length;

	if (version != NULL) {
		*version = priv->version;
	}

	return priv->data;
}

void
almanah_entry_set_data (AlmanahEntry *self, const guint8 *data, gsize length, guint version)
{
	AlmanahEntryPrivate *priv = almanah_entry_get_instance_private (self);

	g_free (priv->data);

	priv->data = g_memdup2 (data, length * sizeof (*data));
	priv->length = length;
	priv->version = version;
	priv->is_empty = FALSE;
}

gboolean
almanah_entry_get_content (AlmanahEntry *self, GtkTextBuffer *text_buffer, gboolean create_tags, GError **error)
{
	AlmanahEntryPrivate *priv = almanah_entry_get_instance_private (self);

	/* Deserialise the data according to the version of the data format attached to the entry */
	switch (priv->version) {
		case DATA_FORMAT_XML_2_0: {
			GdkAtom format_atom;
			GtkTextIter start_iter;

			format_atom = gtk_text_buffer_register_deserialize_format (text_buffer, "application/x-almanah-entry-xml",
			                                                           (GtkTextBufferDeserializeFunc) deserialise_entry_xml_2_0,
			                                                           NULL, NULL);
			gtk_text_buffer_get_start_iter (text_buffer, &start_iter);

			/* Try deserializing the serialized data */
			return gtk_text_buffer_deserialize (text_buffer, text_buffer, format_atom, &start_iter, priv->data, priv->length, error);
		}
		case DATA_FORMAT_PLAIN_TEXT__GTK_TEXT_BUFFER: {
			GdkAtom format_atom;
			GtkTextIter start_iter;
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
		case DATA_FORMAT_UNSET:
		default: {
			/* Invalid/Unset version number */
			g_set_error (error, ALMANAH_ENTRY_ERROR, ALMANAH_ENTRY_ERROR_INVALID_DATA_VERSION,
			             _("Invalid data version number %u."), priv->version);

			return FALSE;
		}
	}
}

void
almanah_entry_set_content (AlmanahEntry *self, GtkTextBuffer *text_buffer)
{
	GtkTextIter start, end;
	GdkAtom format_atom;
	AlmanahEntryPrivate *priv = almanah_entry_get_instance_private (self);

	/* Update our cached empty status */
	priv->is_empty = (gtk_text_buffer_get_char_count (text_buffer) == 0) ? TRUE : FALSE;

	g_free (priv->data);

	gtk_text_buffer_get_bounds (text_buffer, &start, &end);
	format_atom = gtk_text_buffer_register_serialize_format (text_buffer, "application/x-almanah-entry-xml",
	                                                         (GtkTextBufferSerializeFunc) serialise_entry_xml_2_0,
	                                                         NULL, NULL);
	priv->data = gtk_text_buffer_serialize (text_buffer, text_buffer, format_atom, &start, &end, &(priv->length));

	/* Always serialise data in the latest format */
	priv->version = DATA_FORMAT_XML_2_0;
}

/* NOTE: Designed for use on the stack */
void
almanah_entry_get_date (AlmanahEntry *self, GDate *date)
{
	AlmanahEntryPrivate *priv = almanah_entry_get_instance_private (self);

	g_date_set_dmy (date,
			g_date_get_day (&(priv->date)),
			g_date_get_month (&(priv->date)),
			g_date_get_year (&(priv->date)));
}

AlmanahEntryEditability
almanah_entry_get_editability (AlmanahEntry *self)
{
	AlmanahEntryPrivate *priv = almanah_entry_get_instance_private (self);
	GDate current_date;
	gint days_between;

	g_date_set_time_t (&current_date, time (NULL));

	/* Entries can't be edited before they've happened */
	days_between = g_date_days_between (&(priv->date), &current_date);

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
	AlmanahEntryPrivate *priv = almanah_entry_get_instance_private (self);

	return (priv->is_empty == TRUE ||
		priv->length == 0 ||
		priv->data == NULL ||
		priv->data[0] == '\0') ? TRUE : FALSE;
}

gboolean
almanah_entry_is_important (AlmanahEntry *self)
{
	AlmanahEntryPrivate *priv = almanah_entry_get_instance_private (self);

	return priv->is_important;
}

void
almanah_entry_set_is_important (AlmanahEntry *self, gboolean is_important)
{
	AlmanahEntryPrivate *priv = almanah_entry_get_instance_private (self);

	/* Make sure we only notify if the property value really has changed */
	if (priv->is_important != is_important) {
		priv->is_important = is_important;
		g_object_notify (G_OBJECT (self), "is-important");
	}
}

/* NOTE: Designed for use on the stack */
void
almanah_entry_get_last_edited (AlmanahEntry *self, GDate *last_edited)
{
	g_return_if_fail (ALMANAH_IS_ENTRY (self));
	g_return_if_fail (last_edited != NULL);

	AlmanahEntryPrivate *priv = almanah_entry_get_instance_private (self);

	*last_edited = priv->last_edited;
}

/* NOTE: Designed for use on the stack */
void
almanah_entry_set_last_edited (AlmanahEntry *self, GDate *last_edited)
{
	g_return_if_fail (ALMANAH_IS_ENTRY (self));
	g_return_if_fail (last_edited != NULL && g_date_valid (last_edited) == TRUE);

	AlmanahEntryPrivate *priv = almanah_entry_get_instance_private (self);

	priv->last_edited = *last_edited;
}

/* Copied from GTK+'s gtktextbufferserialize.c, LGPLv2.1+:
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2004 Nokia Corporation
 */
static void
find_list_delta (GSList  *old_list,
                 GSList  *new_list,
		 GList  **added,
                 GList  **removed)
{
  GSList *tmp;
  GList *tmp_added, *tmp_removed;

  tmp_added = NULL;
  tmp_removed = NULL;

  /* Find added tags */
  tmp = new_list;
  while (tmp)
    {
      if (!g_slist_find (old_list, tmp->data))
	tmp_added = g_list_prepend (tmp_added, tmp->data);

      tmp = tmp->next;
    }

  *added = tmp_added;

  /* Find removed tags */
  tmp = old_list;
  while (tmp)
    {
      if (!g_slist_find (new_list, tmp->data))
	tmp_removed = g_list_prepend (tmp_removed, tmp->data);

      tmp = tmp->next;
    }

  /* We reverse the list here to match the xml semantics */
  *removed = g_list_reverse (tmp_removed);
}

/* Returns NULL for unknown/unhandled tags */
static const gchar *
get_text_tag_element_name (GtkTextTag *tag)
{
	gchar *name;
	const gchar *element_name = NULL;

	if (ALMANAH_IS_HYPERLINK_TAG (tag)) {
		return "link";
	}

	g_object_get (G_OBJECT (tag), "name", &name, NULL);

	/* Unknown tag */
	if (name == NULL) {
		return NULL;
	}

	/* Handle the normal tags */
	if (strcmp (name, "bold") == 0) {
		element_name = "bold";
	} else if (strcmp (name, "italic") == 0) {
		element_name = "italic";
	} else if (strcmp (name, "underline") == 0) {
		element_name = "underline";
	}

	g_free (name);

	return element_name;
}

static guint8 *
serialise_entry_xml_2_0 (GtkTextBuffer *register_buffer, GtkTextBuffer *content_buffer, const GtkTextIter *start, const GtkTextIter *end, gsize *length,
                         gpointer user_data)
{
	GString *markup;
	GtkTextIter iter, old_iter;
	GSList *active_tags, *old_tag_list;

	markup = g_string_new (NULL);

	/* Markup preamble */
	g_string_append (markup,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<entry xmlns=\"http://www.gnome.org/almanah-diary/entry/2.0/\">");

	/* Serialise the text. We maintain a stack of the currently open tags so that the outputted markup is properly nested. We progress through
	 * the text buffer between tag toggle points, comparing the list of open tags at each point to determine which tags have opened and which
	 * have closed, and reflecting that in the markup as appropriate. */
	active_tags = NULL;
	old_tag_list = NULL;

	for (old_iter = iter = *start; gtk_text_iter_compare (&iter, end) <= 0; old_iter = iter, gtk_text_iter_forward_to_tag_toggle (&iter, NULL)) {
		GSList *new_tag_list;
		GList *added, *removed;
		const GList *i;

		/* Append the text */
		if (!gtk_text_iter_equal (&old_iter, &iter)) {
			gchar *text, *escaped_text;

			text = gtk_text_iter_get_slice (&old_iter, &iter);
			escaped_text = g_markup_escape_text (text, -1);
			g_free (text);

			g_string_append (markup, escaped_text);
			g_free (escaped_text);
		}

		/* Calculate which tags have been opened and closed */
		new_tag_list = gtk_text_iter_get_tags (&iter);
		find_list_delta (old_tag_list, new_tag_list, &added, &removed);

		/* Handle removed tags first so that we retain proper nesting */
		for (i = removed; i != NULL; i = i->next) {
			GtkTextTag *tag;
			const gchar *element_name;

			tag = GTK_TEXT_TAG (i->data);
			element_name = get_text_tag_element_name (tag);

			/* Ignore unknown/unhandled tags */
			if (element_name == NULL) {
				continue;
			}

			/* Close the tag */
			if (g_slist_find (active_tags, tag)) {
				/* Close all tags that were opened after this one (i.e. which are above this on in the stack), but ensure that they're
				 * re-opened again afterwards by pushing them onto the added list. */
				while (active_tags->data != tag) {
					GtkTextTag *tag2;
					const gchar *element_name2;

					tag2 = GTK_TEXT_TAG (active_tags->data);
					element_name2 = get_text_tag_element_name (tag2);

					active_tags = g_slist_remove (active_tags, tag2);

					g_string_append (markup, "</");
					g_string_append (markup, element_name2);
					g_string_append_c (markup, '>');

					/* Push the tag onto the added list iff it's not also being closed now */
					if (g_list_find (removed, tag2) == NULL) {
						added = g_list_prepend (added, tag2);
					}
				}

				/* Close this tag */
				active_tags = g_slist_remove (active_tags, active_tags->data);

				g_string_append (markup, "</");
				g_string_append (markup, element_name);
				g_string_append_c (markup, '>');
			}
		}

		for (i = added; i != NULL; i = i->next) {
			GtkTextTag *tag;
			const gchar *element_name;

			tag = GTK_TEXT_TAG (i->data);
			element_name = get_text_tag_element_name (tag);

			/* Ignore unknown/unhandled tags */
			if (element_name == NULL) {
				continue;
			}

			g_string_append_c (markup, '<');
			g_string_append (markup, element_name);

			if (ALMANAH_IS_HYPERLINK_TAG (tag)) {
				gchar *escaped_uri;

				escaped_uri = g_markup_escape_text (almanah_hyperlink_tag_get_uri (ALMANAH_HYPERLINK_TAG (tag)), -1);
				g_string_append (markup, " uri=\"");
				g_string_append (markup, escaped_uri);
				g_string_append_c (markup, '"');
				g_free (escaped_uri);
			}

			g_string_append_c (markup, '>');

			active_tags = g_slist_prepend (active_tags, tag);
		}

		g_list_free (added);
		g_list_free (removed);

		/* Swap the new and old tag lists */
		g_slist_free (old_tag_list);
		old_tag_list = new_tag_list;

		if (gtk_text_iter_equal (&iter, end)) {
			break;
		}
	}

	g_slist_free (old_tag_list);

	/* Close any tags which remain open */
	while (active_tags != NULL) {
		GtkTextTag *tag;
		const gchar *element_name;

		tag = GTK_TEXT_TAG (active_tags->data);
		element_name = get_text_tag_element_name (tag);

		active_tags = g_slist_remove (active_tags, tag);

		g_string_append (markup, "</");
		g_string_append (markup, element_name);
		g_string_append_c (markup, '>');
	}

	/* Markup postamble */
	g_string_append (markup, "</entry>");

	*length = markup->len;

	return (guint8*) g_string_free (markup, FALSE);
}

typedef struct {
	GtkTextBuffer *buffer;
	GtkTextIter *iter;
	gboolean in_entry;
	GSList *active_tags;
} DeserialiseContext;

static void
start_element_cb (GMarkupParseContext *parse_context, const gchar *element_name, const gchar **attribute_names, const gchar **attribute_values,
                  gpointer user_data, GError **error)
{
	DeserialiseContext *deserialise_context = (DeserialiseContext*) user_data;

	if (strcmp (element_name, "entry") == 0) {
		if (deserialise_context->in_entry) {
			g_set_error_literal (error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, "<entry> elements can only be at the top level.");
			return;
		}

		deserialise_context->in_entry = TRUE;
		return;
	} else {
		GtkTextTagTable *table;
		GtkTextTag *tag = NULL;

		if (!deserialise_context->in_entry) {
			g_set_error_literal (error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, "An <entry> element must be at the top level.");
			return;
		}

		table = gtk_text_buffer_get_tag_table (deserialise_context->buffer);

		if (strcmp (element_name, "bold") == 0 ||
		    strcmp (element_name, "italic") == 0 ||
		    strcmp (element_name, "underline") == 0) {
			/* Just retrieve the predefined tag from the tag table */
			tag = gtk_text_tag_table_lookup (table, element_name);
		} else if (strcmp (element_name, "link") == 0) {
			guint i = 0;
			const gchar *uri;

			/* Extract the URI */
			while (attribute_names[i] != NULL && strcmp (attribute_names[i], "uri") != 0) {
				i++;
			}

			uri = attribute_values[i];

			if (uri == NULL || *uri == '\0') {
				g_set_error_literal (error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, "A <link> element must have a 'uri' attribute.");
				return;
			}

			/* Create the tag and register it in the tag table */
			tag = GTK_TEXT_TAG (almanah_hyperlink_tag_new (uri));
			gtk_text_tag_table_add (table, tag);
			g_object_unref (tag); /* the tag table keeps a reference */
		}

		/* Ignore unrecognised tags (which can happen when searching, for example). */
		if (tag != NULL) {
			/* Push the tag onto the stack of active tags which will be applied to the next text run */
			deserialise_context->active_tags = g_slist_prepend (deserialise_context->active_tags, tag);
		}
	}
}

static void
end_element_cb (GMarkupParseContext *parse_context, const gchar *element_name, gpointer user_data, GError **error)
{
	DeserialiseContext *deserialise_context = (DeserialiseContext*) user_data;

	if (strcmp (element_name, "entry") == 0) {
		/* We should be finished parsing now */
		deserialise_context->in_entry = FALSE;
		return;
	} else {
		if (strcmp (element_name, "bold") == 0 ||
		    strcmp (element_name, "italic") == 0 ||
		    strcmp (element_name, "underline") == 0 ||
		    strcmp (element_name, "link") == 0) {
			/* Pop the topmost tag off the active tags stack. Note that the stack might be empty if our text tag table is empty (which can
			 * happen when we're searching, because we don't bother setting up the text tags for that). */
			if (deserialise_context->active_tags != NULL) {
				deserialise_context->active_tags = g_slist_remove (deserialise_context->active_tags,
				                                                   deserialise_context->active_tags->data);
			}
		}

		/* Ignore unrecognised tags */
	}
}

static void
text_cb (GMarkupParseContext *parse_context, const gchar *text, gsize text_len, gpointer user_data, GError **error)
{
	DeserialiseContext *deserialise_context = (DeserialiseContext*) user_data;
	GtkTextIter start_iter;
	gint start_offset;
	const GSList *i;

	/* Add the text to the text buffer, and apply all the tags in the current active tags stack to it */
	start_offset = gtk_text_iter_get_offset (deserialise_context->iter);
	gtk_text_buffer_insert (deserialise_context->buffer, deserialise_context->iter, text, text_len);
	gtk_text_buffer_get_iter_at_offset (deserialise_context->buffer, &start_iter, start_offset);

	for (i = deserialise_context->active_tags; i != NULL; i = i->next) {
		gtk_text_buffer_apply_tag (deserialise_context->buffer, GTK_TEXT_TAG (i->data), &start_iter, deserialise_context->iter);
	}
}

static gboolean
deserialise_entry_xml_2_0 (GtkTextBuffer *register_buffer, GtkTextBuffer *content_buffer, GtkTextIter *iter, const guint8 *data, gsize length,
                           gboolean create_tags, gpointer user_data, GError **error)
{
	GMarkupParseContext *parse_context;
	gboolean success;

	DeserialiseContext deserialise_context = {
		content_buffer,
		iter,
		FALSE,
	};

	const GMarkupParser parser = {
		start_element_cb,
		end_element_cb,
		text_cb,
		NULL,
		NULL,
	};

	parse_context = g_markup_parse_context_new (&parser, 0, &deserialise_context, NULL);
	success = g_markup_parse_context_parse (parse_context, (const gchar*) data, length, error);
	g_markup_parse_context_free (parse_context);

	return success;
}
