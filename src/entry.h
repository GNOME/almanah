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

#ifndef ALMANAH_ENTRY_H
#define ALMANAH_ENTRY_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	ALMANAH_ENTRY_EDITABLE = 1,
	ALMANAH_ENTRY_FUTURE = 2,
	ALMANAH_ENTRY_PAST = 0
} AlmanahEntryEditability;

/* The number of days after which a diary entry requires confirmation to be edited */
#define ALMANAH_ENTRY_CUTOFF_AGE 14

#define ALMANAH_TYPE_ENTRY		(almanah_entry_get_type ())
#define ALMANAH_ENTRY(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_ENTRY, AlmanahEntry))
#define ALMANAH_ENTRY_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_ENTRY, AlmanahEntryClass))
#define ALMANAH_IS_ENTRY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_ENTRY))
#define ALMANAH_IS_ENTRY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_ENTRY))
#define ALMANAH_ENTRY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_ENTRY, AlmanahEntryClass))

typedef struct _AlmanahEntryPrivate	AlmanahEntryPrivate;

typedef struct {
	GObject parent;
	AlmanahEntryPrivate *priv;
} AlmanahEntry;

typedef struct {
	GObjectClass parent;
} AlmanahEntryClass;

GType almanah_entry_get_type (void);
AlmanahEntry *almanah_entry_new (GDate *date);

const guint8 *almanah_entry_get_data (AlmanahEntry *self, gsize *length);
void almanah_entry_set_data (AlmanahEntry *self, const guint8 *data, gsize length);
gboolean almanah_entry_get_content (AlmanahEntry *self, GtkTextBuffer *text_buffer, gboolean create_tags, GError **error);
void almanah_entry_set_content (AlmanahEntry *self, GtkTextBuffer *text_buffer);

void almanah_entry_get_date (AlmanahEntry *self, GDate *date);
AlmanahEntryEditability almanah_entry_get_editability (AlmanahEntry *self);
gboolean almanah_entry_is_empty (AlmanahEntry *self);

G_END_DECLS

#endif /* !ALMANAH_ENTRY_H */
