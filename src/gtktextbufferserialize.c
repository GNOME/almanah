/* gtktextbufferserialize.c
 *
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2004 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/* FIXME: We should use other error codes for the
 * parts that deal with the format errors
 */

#include "config.h"

#include <errno.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gdk-pixbuf/gdk-pixdata.h"
#include "gtktextbufferserialize.h"

static gboolean
deserialize_value (const gchar *str,
                   GValue *value)
{
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	if (g_value_type_transformable (G_TYPE_STRING, value->g_type)) {
		GValue text_value = G_VALUE_INIT;
		gboolean retval;

		g_value_init (&text_value, G_TYPE_STRING);
		g_value_set_static_string (&text_value, str);

		retval = g_value_transform (&text_value, value);
		g_value_unset (&text_value);

		return retval;
	} else if (value->g_type == G_TYPE_BOOLEAN) {
		gboolean v;

		v = strcmp (str, "TRUE") == 0;

		g_value_set_boolean (value, v);

		return TRUE;
	} else if (value->g_type == G_TYPE_INT) {
		gchar *tmp;
		int v;

		errno = 0;
		v = g_ascii_strtoll (str, &tmp, 10);

		if (errno || tmp == NULL || tmp == str) {
			return FALSE;
		}

		g_value_set_int (value, v);

		return TRUE;
	} else if (value->g_type == G_TYPE_DOUBLE) {
		gchar *tmp;
		gdouble v;

		v = g_ascii_strtod (str, &tmp);

		if (tmp == NULL || tmp == str) {
			return FALSE;
		}

		g_value_set_double (value, v);

		return TRUE;
	} else if (value->g_type == GDK_TYPE_COLOR) {
		GdkColor color;
		const gchar *old;
		gchar *tmp;

		old = str;
		tmp = NULL;
		errno = 0;
		color.red = g_ascii_strtoll (old, &tmp, 16);

		if (errno || tmp == old) {
			return FALSE;
		}

		old = tmp;
		if (*old++ != ':') {
			return FALSE;
		}

		tmp = NULL;
		errno = 0;
		color.green = g_ascii_strtoll (old, &tmp, 16);
		if (errno || tmp == old) {
			return FALSE;
		}

		old = tmp;
		if (*old++ != ':') {
			return FALSE;
		}

		tmp = NULL;
		errno = 0;
		color.blue = g_ascii_strtoll (old, &tmp, 16);

		if (errno || tmp == old || *tmp != '\0') {
			return FALSE;
		}

		g_value_set_boxed (value, &color);

		return TRUE;
	} else if (G_VALUE_HOLDS_ENUM (value)) {
		GEnumClass *class = G_ENUM_CLASS (g_type_class_peek (value->g_type));
		GEnumValue *enum_value;

		enum_value = g_enum_get_value_by_name (class, str);

		if (enum_value) {
			g_value_set_enum (value, enum_value->value);
			return TRUE;
		}

		return FALSE;
	} else {
		g_warning ("Type %s can not be deserialized", g_type_name (value->g_type));
	}
	G_GNUC_END_IGNORE_DEPRECATIONS

	return FALSE;
}

typedef enum {
	STATE_START,
	STATE_TEXT_VIEW_MARKUP,
	STATE_TAGS,
	STATE_TAG,
	STATE_ATTR,
	STATE_TEXT,
	STATE_APPLY_TAG,
	STATE_PIXBUF
} ParseState;

typedef struct
{
	gchar *text;
	GdkPixbuf *pixbuf;
	GSList *tags;
} TextSpan;

typedef struct
{
	GtkTextTag *tag;
	gint prio;
} TextTagPrio;

typedef struct
{
	GSList *states;

	GList *headers;

	GtkTextBuffer *buffer;

	/* Tags that are defined in <tag> elements */
	GHashTable *defined_tags;

	/* Tags that are anonymous */
	GHashTable *anonymous_tags;

	/* Tag name substitutions */
	GHashTable *substitutions;

	/* Current tag */
	GtkTextTag *current_tag;

	/* Priority of current tag */
	gint current_tag_prio;

	/* Id of current tag */
	gint current_tag_id;

	/* Tags and their priorities */
	GList *tag_priorities;

	GSList *tag_stack;

	GList *spans;

	gboolean create_tags;

	gboolean parsed_text;
	gboolean parsed_tags;
} ParseInfo;

static void
set_error (GError **err,
           GMarkupParseContext *context,
           int error_domain,
           int error_code,
           const char *format,
           ...) G_GNUC_PRINTF (5, 6);

static void
set_error (GError **err,
           GMarkupParseContext *context,
           int error_domain,
           int error_code,
           const char *format,
           ...)
{
	int line, ch;
	va_list args;
	char *str;

	g_markup_parse_context_get_position (context, &line, &ch);

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	g_set_error (err, error_domain, error_code,
	             ("Line %d character %d: %s"),
	             line, ch, str);

	g_free (str);
}

static void
push_state (ParseInfo *info,
            ParseState state)
{
	info->states = g_slist_prepend (info->states, GINT_TO_POINTER (state));
}

static void
pop_state (ParseInfo *info)
{
	g_return_if_fail (info->states != NULL);

	info->states = g_slist_remove (info->states, info->states->data);
}

static ParseState
peek_state (ParseInfo *info)
{
	g_return_val_if_fail (info->states != NULL, STATE_START);

	return GPOINTER_TO_INT (info->states->data);
}

#define ELEMENT_IS(name) (strcmp (element_name, (name)) == 0)

static gboolean
check_id_or_name (GMarkupParseContext *context,
                  const gchar *element_name,
                  const gchar **attribute_names,
                  const gchar **attribute_values,
                  gint *id,
                  const gchar **name,
                  GError **error)
{
	gboolean has_id = FALSE;
	gboolean has_name = FALSE;
	int i;

	*id = 0;
	*name = NULL;

	for (i = 0; attribute_names[i] != NULL; i++) {
		if (strcmp (attribute_names[i], "name") == 0) {
			*name = attribute_values[i];

			if (has_id) {
				set_error (error, context,
				           G_MARKUP_ERROR,
				           G_MARKUP_ERROR_PARSE,
				           _ ("Both \"id\" and \"name\" were found on the <%s> element"),
				           element_name);
				return FALSE;
			}

			if (has_name) {
				set_error (error, context,
				           G_MARKUP_ERROR,
				           G_MARKUP_ERROR_PARSE,
				           _ ("The attribute \"%s\" was found twice on the <%s> element"),
				           "name", element_name);
				return FALSE;
			}

			has_name = TRUE;
		} else if (strcmp (attribute_names[i], "id") == 0) {
			gchar *tmp;

			if (has_name) {
				set_error (error, context,
				           G_MARKUP_ERROR,
				           G_MARKUP_ERROR_PARSE,
				           _ ("Both \"id\" and \"name\" were found on the <%s> element"),
				           element_name);
				return FALSE;
			}

			if (has_id) {
				set_error (error, context,
				           G_MARKUP_ERROR,
				           G_MARKUP_ERROR_PARSE,
				           _ ("The attribute \"%s\" was found twice on the <%s> element"),
				           "id", element_name);
				return FALSE;
			}

			has_id = TRUE;

			/* Try parsing the integer */
			tmp = NULL;
			errno = 0;
			*id = g_ascii_strtoll (attribute_values[i], &tmp, 10);

			if (errno || tmp == attribute_values[i]) {
				set_error (error, context,
				           G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
				           _ ("<%s> element has invalid ID \"%s\""),
				           element_name, attribute_values[i]);
				return FALSE;
			}
		}
	}

	if (!has_id && !has_name) {
		set_error (error, context,
		           G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
		           _ ("<%s> element has neither a \"name\" nor an \"id\" attribute"), element_name);
		return FALSE;
	}

	return TRUE;
}

typedef struct
{
	const char *name;
	const char **retloc;
} LocateAttr;

static gboolean
locate_attributes (GMarkupParseContext *context,
                   const char *element_name,
                   const char **attribute_names,
                   const char **attribute_values,
                   gboolean allow_unknown_attrs,
                   GError **error,
                   const char *first_attribute_name,
                   const char **first_attribute_retloc,
                   ...)
{
	va_list args;
	const char *name;
	const char **retloc;
	int n_attrs;
#define MAX_ATTRS 24
	LocateAttr attrs[MAX_ATTRS];
	gboolean retval;
	int i;

	g_return_val_if_fail (first_attribute_name != NULL, FALSE);
	g_return_val_if_fail (first_attribute_retloc != NULL, FALSE);

	retval = TRUE;

	n_attrs = 1;
	attrs[0].name = first_attribute_name;
	attrs[0].retloc = first_attribute_retloc;
	*first_attribute_retloc = NULL;

	va_start (args, first_attribute_retloc);

	name = va_arg (args, const char *);
	retloc = va_arg (args, const char **);

	while (name != NULL) {
		g_return_val_if_fail (retloc != NULL, FALSE);

		g_assert (n_attrs < MAX_ATTRS);

		attrs[n_attrs].name = name;
		attrs[n_attrs].retloc = retloc;
		n_attrs += 1;
		*retloc = NULL;

		name = va_arg (args, const char *);
		retloc = va_arg (args, const char **);
	}

	va_end (args);

	if (!retval) {
		return retval;
	}

	i = 0;
	while (attribute_names[i]) {
		int j;
		gboolean found;

		found = FALSE;
		j = 0;
		while (j < n_attrs) {
			if (strcmp (attrs[j].name, attribute_names[i]) == 0) {
				retloc = attrs[j].retloc;

				if (*retloc != NULL) {
					set_error (error, context,
					           G_MARKUP_ERROR,
					           G_MARKUP_ERROR_PARSE,
					           _ ("Attribute \"%s\" repeated twice on the same <%s> element"),
					           attrs[j].name, element_name);
					retval = FALSE;
					goto out;
				}

				*retloc = attribute_values[i];
				found = TRUE;
			}

			++j;
		}

		if (!found && !allow_unknown_attrs) {
			set_error (error, context,
			           G_MARKUP_ERROR,
			           G_MARKUP_ERROR_PARSE,
			           _ ("Attribute \"%s\" is invalid on <%s> element in this context"),
			           attribute_names[i], element_name);
			retval = FALSE;
			goto out;
		}

		++i;
	}

out:
	return retval;
}

static gboolean
check_no_attributes (GMarkupParseContext *context,
                     const char *element_name,
                     const char **attribute_names,
                     const char **attribute_values,
                     GError **error)
{
	if (attribute_names[0] != NULL) {
		set_error (error, context,
		           G_MARKUP_ERROR,
		           G_MARKUP_ERROR_PARSE,
		           _ ("Attribute \"%s\" is invalid on <%s> element in this context"),
		           attribute_names[0], element_name);
		return FALSE;
	}

	return TRUE;
}

static GtkTextTag *
tag_exists (GMarkupParseContext *context,
            const gchar *name,
            gint id,
            ParseInfo *info,
            GError **error)
{
	GtkTextTagTable *tag_table;
	const gchar *real_name;

	tag_table = gtk_text_buffer_get_tag_table (info->buffer);

	if (info->create_tags) {
		/* If we have an anonymous tag, just return it directly */
		if (!name) {
			return g_hash_table_lookup (info->anonymous_tags,
			                            GINT_TO_POINTER (id));
		}

		/* First, try the substitutions */
		real_name = g_hash_table_lookup (info->substitutions, name);

		if (real_name) {
			return gtk_text_tag_table_lookup (tag_table, real_name);
		}

		/* Next, try the list of defined tags */
		if (g_hash_table_lookup (info->defined_tags, name) != NULL) {
			return gtk_text_tag_table_lookup (tag_table, name);
		}

		set_error (error, context,
		           G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
		           _ ("Tag \"%s\" has not been defined."), name);

		return NULL;
	} else {
		GtkTextTag *tag;

		if (!name) {
			set_error (error, context,
			           G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
			           _ ("Anonymous tag found and tags can not be created."));
			return NULL;
		}

		tag = gtk_text_tag_table_lookup (tag_table, name);

		if (tag) {
			return tag;
		}

		set_error (error, context,
		           G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
		           _ ("Tag \"%s\" does not exist in buffer and tags can not be created."), name);

		return NULL;
	}
}

typedef struct
{
	const gchar *id;
	gint length;
	const gchar *start;
} Header;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static GdkPixbuf *
get_pixbuf_from_headers (GList *headers,
                         int id,
                         GError **error)
{
	Header *header;
	GdkPixdata pixdata;
	GdkPixbuf *pixbuf;

	header = g_list_nth_data (headers, id);

	if (!header) {
		return NULL;
	}

	if (!gdk_pixdata_deserialize (&pixdata, header->length,
	                              (const guint8 *) header->start, error)) {
		return NULL;
	}

	pixbuf = gdk_pixbuf_from_pixdata (&pixdata, TRUE, error);

	return pixbuf;
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void
parse_apply_tag_element (GMarkupParseContext *context,
                         const gchar *element_name,
                         const gchar **attribute_names,
                         const gchar **attribute_values,
                         ParseInfo *info,
                         GError **error)
{
	const gchar *name, *priority;
	gint id;
	GtkTextTag *tag;

	g_assert (peek_state (info) == STATE_TEXT ||
	          peek_state (info) == STATE_APPLY_TAG);

	if (ELEMENT_IS ("apply_tag")) {
		if (!locate_attributes (context, element_name, attribute_names, attribute_values, TRUE, error,
		                        "priority", &priority, NULL)) {
			return;
		}

		if (!check_id_or_name (context, element_name, attribute_names, attribute_values,
		                       &id, &name, error)) {
			return;
		}

		tag = tag_exists (context, name, id, info, error);

		if (!tag) {
			return;
		}

		info->tag_stack = g_slist_prepend (info->tag_stack, tag);

		push_state (info, STATE_APPLY_TAG);
	} else if (ELEMENT_IS ("pixbuf")) {
		int int_id;
		GdkPixbuf *pixbuf;
		TextSpan *span;
		const gchar *pixbuf_id;

		if (!locate_attributes (context, element_name, attribute_names, attribute_values, FALSE, error,
		                        "index", &pixbuf_id, NULL)) {
			return;
		}

		int_id = atoi (pixbuf_id);
		pixbuf = get_pixbuf_from_headers (info->headers, int_id, error);

		span = g_slice_new0 (TextSpan);
		span->pixbuf = pixbuf;
		span->tags = NULL;

		info->spans = g_list_prepend (info->spans, span);

		if (!pixbuf) {
			return;
		}

		push_state (info, STATE_PIXBUF);
	} else {
		set_error (error, context,
		           G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
		           _ ("Element <%s> is not allowed below <%s>"),
		           element_name, peek_state (info) == STATE_TEXT ? "text" : "apply_tag");
	}
}

static void
parse_attr_element (GMarkupParseContext *context,
                    const gchar *element_name,
                    const gchar **attribute_names,
                    const gchar **attribute_values,
                    ParseInfo *info,
                    GError **error)
{
	const gchar *name, *type, *value;
	GType gtype;
	GValue gvalue = G_VALUE_INIT;
	GParamSpec *pspec;

	g_assert (peek_state (info) == STATE_TAG);

	if (ELEMENT_IS ("attr")) {
		if (!locate_attributes (context, element_name, attribute_names, attribute_values, FALSE, error,
		                        "name", &name, "type", &type, "value", &value, NULL)) {
			return;
		}

		gtype = g_type_from_name (type);

		if (gtype == G_TYPE_INVALID) {
			set_error (error, context,
			           G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
			           _ ("\"%s\" is not a valid attribute type"), type);
			return;
		}

		if (!(pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (info->current_tag), name))) {
			set_error (error, context,
			           G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
			           _ ("\"%s\" is not a valid attribute name"), name);
			return;
		}

		g_value_init (&gvalue, gtype);

		if (!deserialize_value (value, &gvalue)) {
			set_error (error, context,
			           G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
			           _ ("\"%s\" could not be converted to a value of type \"%s\" for attribute \"%s\""),
			           value, type, name);
			return;
		}

		if (g_param_value_validate (pspec, &gvalue)) {
			set_error (error, context,
			           G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
			           _ ("\"%s\" is not a valid value for attribute \"%s\""),
			           value, name);
			g_value_unset (&gvalue);
			return;
		}

		g_object_set_property (G_OBJECT (info->current_tag),
		                       name, &gvalue);

		g_value_unset (&gvalue);

		push_state (info, STATE_ATTR);
	} else {
		set_error (error, context,
		           G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
		           _ ("Element <%s> is not allowed below <%s>"),
		           element_name, "tag");
	}
}

static gchar *
get_tag_name (ParseInfo *info,
              const gchar *tag_name)
{
	GtkTextTagTable *tag_table;
	gchar *name;
	gint i;

	name = g_strdup (tag_name);

	if (!info->create_tags) {
		return name;
	}

	i = 0;
	tag_table = gtk_text_buffer_get_tag_table (info->buffer);

	while (gtk_text_tag_table_lookup (tag_table, name) != NULL) {
		g_free (name);
		name = g_strdup_printf ("%s-%d", tag_name, ++i);
	}

	if (i != 0) {
		g_hash_table_insert (info->substitutions, g_strdup (tag_name), g_strdup (name));
	}

	return name;
}

static void
parse_tag_element (GMarkupParseContext *context,
                   const gchar *element_name,
                   const gchar **attribute_names,
                   const gchar **attribute_values,
                   ParseInfo *info,
                   GError **error)
{
	const gchar *name, *priority;
	gchar *tag_name;
	gint id;
	gint prio;
	gchar *tmp;

	g_assert (peek_state (info) == STATE_TAGS);

	if (ELEMENT_IS ("tag")) {
		if (!locate_attributes (context, element_name, attribute_names, attribute_values, TRUE, error,
		                        "priority", &priority, NULL)) {
			return;
		}

		if (!check_id_or_name (context, element_name, attribute_names, attribute_values,
		                       &id, &name, error)) {
			return;
		}

		if (name) {
			if (g_hash_table_lookup (info->defined_tags, name) != NULL) {
				set_error (error, context,
				           G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
				           _ ("Tag \"%s\" already defined"), name);
				return;
			}
		}

		tmp = NULL;
		errno = 0;
		prio = g_ascii_strtoll (priority, &tmp, 10);

		if (errno || tmp == priority) {
			set_error (error, context,
			           G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
			           _ ("Tag \"%s\" has invalid priority \"%s\""), name, priority);
			return;
		}

		if (name) {
			tag_name = get_tag_name (info, name);
			info->current_tag = gtk_text_tag_new (tag_name);
			g_free (tag_name);
		} else {
			info->current_tag = gtk_text_tag_new (NULL);
			info->current_tag_id = id;
		}

		info->current_tag_prio = prio;

		push_state (info, STATE_TAG);
	} else {
		set_error (error, context,
		           G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
		           _ ("Element <%s> is not allowed below <%s>"),
		           element_name, "tags");
	}
}

static void
start_element_handler (GMarkupParseContext *context,
                       const gchar *element_name,
                       const gchar **attribute_names,
                       const gchar **attribute_values,
                       gpointer user_data,
                       GError **error)
{
	ParseInfo *info = user_data;

	switch (peek_state (info)) {
		case STATE_START:
			if (ELEMENT_IS ("text_view_markup")) {
				if (!check_no_attributes (context, element_name,
				                          attribute_names, attribute_values, error)) {
					return;
				}

				push_state (info, STATE_TEXT_VIEW_MARKUP);
				break;
			} else {
				set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
				           _ ("Outermost element in text must be <text_view_markup> not <%s>"),
				           element_name);
			}
			break;
		case STATE_TEXT_VIEW_MARKUP:
			if (ELEMENT_IS ("tags")) {
				if (info->parsed_tags) {
					set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
					           _ ("A <%s> element has already been specified"), "tags");
					return;
				}

				if (!check_no_attributes (context, element_name,
				                          attribute_names, attribute_values, error)) {
					return;
				}

				push_state (info, STATE_TAGS);
				break;
			} else if (ELEMENT_IS ("text")) {
				if (info->parsed_text) {
					set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
					           _ ("A <%s> element has already been specified"), "text");
					return;
				} else if (!info->parsed_tags) {
					set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
					           _ ("A <text> element can't occur before a <tags> element"));
					return;
				}

				if (!check_no_attributes (context, element_name,
				                          attribute_names, attribute_values, error)) {
					return;
				}

				push_state (info, STATE_TEXT);
				break;
			} else {
				set_error (error, context,
				           G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
				           _ ("Element <%s> is not allowed below <%s>"),
				           element_name, "text_view_markup");
			}
			break;
		case STATE_TAGS:
			parse_tag_element (context, element_name,
			                   attribute_names, attribute_values,
			                   info, error);
			break;
		case STATE_TAG:
			parse_attr_element (context, element_name,
			                    attribute_names, attribute_values,
			                    info, error);
			break;
		case STATE_TEXT:
		case STATE_APPLY_TAG:
			parse_apply_tag_element (context, element_name,
			                         attribute_names, attribute_values,
			                         info, error);
			break;
		default:
			g_assert_not_reached ();
			break;
	}
}

static gint
sort_tag_prio (TextTagPrio *a,
               TextTagPrio *b)
{
	if (a->prio < b->prio) {
		return -1;
	} else if (a->prio > b->prio) {
		return 1;
	} else {
		return 0;
	}
}

static void
end_element_handler (GMarkupParseContext *context,
                     const gchar *element_name,
                     gpointer user_data,
                     GError **error)
{
	ParseInfo *info = user_data;
	GList *list;

	switch (peek_state (info)) {
		case STATE_TAGS:
			pop_state (info);
			g_assert (peek_state (info) == STATE_TEXT_VIEW_MARKUP);

			info->parsed_tags = TRUE;

			/* Sort list and add the tags */
			info->tag_priorities = g_list_sort (info->tag_priorities,
			                                    (GCompareFunc) sort_tag_prio);
			list = info->tag_priorities;
			while (list) {
				TextTagPrio *prio = list->data;

				if (info->create_tags) {
					gtk_text_tag_table_add (gtk_text_buffer_get_tag_table (info->buffer),
					                        prio->tag);
				}

				g_object_unref (prio->tag);
				prio->tag = NULL;

				list = list->next;
			}

			break;
		case STATE_TAG:
			pop_state (info);
			g_assert (peek_state (info) == STATE_TAGS);

			gchar *name;
			g_object_get (info->current_tag, "name", &name, NULL);

			if (name) {
				/* Add tag to defined tags hash */
				g_hash_table_insert (info->defined_tags,
				                     name, name);
			} else {
				g_hash_table_insert (info->anonymous_tags,
				                     GINT_TO_POINTER (info->current_tag_id),
				                     info->current_tag);
			}

			if (info->create_tags) {
				TextTagPrio *prio;

				/* add the tag to the list */
				prio = g_slice_new0 (TextTagPrio);
				prio->prio = info->current_tag_prio;
				prio->tag = info->current_tag;

				info->tag_priorities = g_list_prepend (info->tag_priorities, prio);
			}

			info->current_tag = NULL;
			break;
		case STATE_ATTR:
			pop_state (info);
			g_assert (peek_state (info) == STATE_TAG);
			break;
		case STATE_APPLY_TAG:
			pop_state (info);
			g_assert (peek_state (info) == STATE_APPLY_TAG ||
			          peek_state (info) == STATE_TEXT);

			/* Pop tag */
			info->tag_stack = g_slist_delete_link (info->tag_stack,
			                                       info->tag_stack);

			break;
		case STATE_TEXT:
			pop_state (info);
			g_assert (peek_state (info) == STATE_TEXT_VIEW_MARKUP);

			info->spans = g_list_reverse (info->spans);
			info->parsed_text = TRUE;
			break;
		case STATE_TEXT_VIEW_MARKUP:
			pop_state (info);
			g_assert (peek_state (info) == STATE_START);
			break;
		case STATE_PIXBUF:
			pop_state (info);
			g_assert (peek_state (info) == STATE_APPLY_TAG ||
			          peek_state (info) == STATE_TEXT);
			break;
		default:
			g_assert_not_reached ();
			break;
	}
}

static gboolean
all_whitespace (const char *text,
                int text_len)
{
	const char *p;
	const char *end;

	p = text;
	end = text + text_len;

	while (p != end) {
		if (!g_ascii_isspace (*p)) {
			return FALSE;
		}

		p = g_utf8_next_char (p);
	}

	return TRUE;
}

static void
text_handler (GMarkupParseContext *context,
              const gchar *text,
              gsize text_len,
              gpointer user_data,
              GError **error)
{
	ParseInfo *info = user_data;
	TextSpan *span;

	if (all_whitespace (text, text_len) &&
	    peek_state (info) != STATE_TEXT &&
	    peek_state (info) != STATE_APPLY_TAG) {
		return;
	}

	switch (peek_state (info)) {
		case STATE_START:
			g_assert_not_reached (); /* gmarkup shouldn't do this */
			break;
		case STATE_TEXT:
		case STATE_APPLY_TAG:
			if (text_len == 0) {
				return;
			}

			span = g_slice_new0 (TextSpan);
			span->text = g_strndup (text, text_len);
			span->tags = g_slist_copy (info->tag_stack);

			info->spans = g_list_prepend (info->spans, span);
			break;
		default:
			g_assert_not_reached ();
			break;
	}
}

static void
parse_info_init (ParseInfo *info,
                 GtkTextBuffer *buffer,
                 gboolean create_tags,
                 GList *headers)
{
	info->states = g_slist_prepend (NULL, GINT_TO_POINTER (STATE_START));

	info->create_tags = create_tags;
	info->headers = headers;
	info->defined_tags = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	info->substitutions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	info->anonymous_tags = g_hash_table_new_full (NULL, NULL, NULL, NULL);
	info->tag_stack = NULL;
	info->spans = NULL;
	info->parsed_text = FALSE;
	info->parsed_tags = FALSE;
	info->current_tag = NULL;
	info->current_tag_prio = -1;
	info->tag_priorities = NULL;

	info->buffer = buffer;
}

static void
text_span_free (TextSpan *span)
{
	g_free (span->text);
	g_slist_free (span->tags);
	g_slice_free (TextSpan, span);
}

static void
parse_info_free (ParseInfo *info)
{
	GList *list;

	g_slist_free (info->tag_stack);
	g_slist_free (info->states);

	g_hash_table_destroy (info->substitutions);
	g_hash_table_destroy (info->defined_tags);

	if (info->current_tag) {
		g_object_unref (info->current_tag);
	}

	list = info->spans;
	while (list) {
		text_span_free (list->data);

		list = list->next;
	}
	g_list_free (info->spans);

	list = info->tag_priorities;
	while (list) {
		TextTagPrio *prio = list->data;

		if (prio->tag) {
			g_object_unref (prio->tag);
		}
		g_slice_free (TextTagPrio, prio);

		list = list->next;
	}
	g_list_free (info->tag_priorities);
}

static void
insert_text (ParseInfo *info,
             GtkTextIter *iter)
{
	GtkTextIter start_iter;
	GtkTextMark *mark;
	GList *tmp;
	GSList *tags;

	start_iter = *iter;

	mark = gtk_text_buffer_create_mark (info->buffer, "deserialize_insert_point",
	                                    &start_iter, TRUE);

	tmp = info->spans;
	while (tmp) {
		TextSpan *span = tmp->data;

		if (span->text) {
			gtk_text_buffer_insert (info->buffer, iter, span->text, -1);
		} else {
			gtk_text_buffer_insert_pixbuf (info->buffer, iter, span->pixbuf);
			g_object_unref (span->pixbuf);
		}
		gtk_text_buffer_get_iter_at_mark (info->buffer, &start_iter, mark);

		/* Apply tags */
		tags = span->tags;
		while (tags) {
			GtkTextTag *tag = tags->data;

			gtk_text_buffer_apply_tag (info->buffer, tag,
			                           &start_iter, iter);

			tags = tags->next;
		}

		gtk_text_buffer_move_mark (info->buffer, mark, iter);

		tmp = tmp->next;
	}

	gtk_text_buffer_delete_mark (info->buffer, mark);
}

static int
read_int (const guchar *start)
{
	int result;

	result =
	    start[0] << 24 |
	    start[1] << 16 |
	    start[2] << 8 |
	    start[3];

	return result;
}

static gboolean
header_is (Header *header,
           const gchar *id)
{
	return (strncmp (header->id, id, strlen (id)) == 0);
}

static GList *
read_headers (const gchar *start,
              gint len,
              GError **error)
{
	int i = 0;
	int section_len;
	Header *header;
	GList *headers = NULL;
	GList *l;

	while (i < len) {
		if (i + 30 >= len) {
			goto error;
		}

		if (strncmp (start + i, "GTKTEXTBUFFERCONTENTS-0001", 26) == 0 ||
		    strncmp (start + i, "GTKTEXTBUFFERPIXBDATA-0001", 26) == 0) {
			section_len = read_int ((const guchar *) start + i + 26);

			if (i + 30 + section_len > len) {
				goto error;
			}

			header = g_slice_new0 (Header);
			header->id = start + i;
			header->length = section_len;
			header->start = start + i + 30;

			i += 30 + section_len;

			headers = g_list_prepend (headers, header);
		} else {
			break;
		}
	}

	return g_list_reverse (headers);

error:
	for (l = headers; l != NULL; l = l->next) {
		header = l->data;
		g_slice_free (Header, header);
	}

	g_list_free (headers);

	g_set_error_literal (error,
	                     G_MARKUP_ERROR,
	                     G_MARKUP_ERROR_PARSE,
	                     _ ("Serialized data is malformed"));

	return NULL;
}

static gboolean
deserialize_text (GtkTextBuffer *buffer,
                  GtkTextIter *iter,
                  const gchar *text,
                  gint len,
                  gboolean create_tags,
                  GError **error,
                  GList *headers)
{
	GMarkupParseContext *context;
	ParseInfo info;
	gboolean retval = FALSE;

	static const GMarkupParser rich_text_parser = {
		start_element_handler,
		end_element_handler,
		text_handler,
		NULL,
		NULL
	};

	parse_info_init (&info, buffer, create_tags, headers);

	context = g_markup_parse_context_new (&rich_text_parser,
	                                      0, &info, NULL);

	if (!g_markup_parse_context_parse (context,
	                                   text,
	                                   len,
	                                   error)) {
		goto out;
	}

	if (!g_markup_parse_context_end_parse (context, error)) {
		goto out;
	}

	retval = TRUE;

	/* Now insert the text */
	insert_text (&info, iter);

out:
	parse_info_free (&info);

	g_markup_parse_context_free (context);

	return retval;
}

gboolean
almanah_deserialise_entry_gtk_text_buffer (GtkTextBuffer *content_buffer,
                                           GtkTextIter *iter,
                                           const guint8 *text,
                                           gsize length,
                                           gboolean create_tags,
                                           GError **error)
{
	GList *headers;
	GList *l;
	Header *header;
	gboolean retval;

	headers = read_headers ((gchar *) text, length, error);

	if (!headers) {
		return FALSE;
	}

	header = headers->data;
	if (!header_is (header, "GTKTEXTBUFFERCONTENTS-0001")) {
		g_set_error_literal (error,
		                     G_MARKUP_ERROR,
		                     G_MARKUP_ERROR_PARSE,
		                     _ ("Serialized data is malformed. First section isn't GTKTEXTBUFFERCONTENTS-0001"));

		retval = FALSE;
		goto out;
	}

	retval = deserialize_text (content_buffer, iter,
	                           header->start, header->length,
	                           create_tags, error, headers->next);

out:
	for (l = headers; l != NULL; l = l->next) {
		header = l->data;
		g_slice_free (Header, header);
	}

	g_list_free (headers);

	return retval;
}
