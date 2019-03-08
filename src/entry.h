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

typedef enum {
	ALMANAH_ENTRY_ERROR_INVALID_DATA_VERSION,
} AlmanahEntryError;

GQuark almanah_entry_error_quark (void) G_GNUC_CONST;
#define ALMANAH_ENTRY_ERROR		(almanah_entry_error_quark ())

#define ALMANAH_TYPE_ENTRY      (almanah_entry_get_type ())

G_DECLARE_FINAL_TYPE (AlmanahEntry, almanah_entry, ALMANAH, ENTRY, GObject)

AlmanahEntry *almanah_entry_new (GDate *date);

const guint8 *almanah_entry_get_data (AlmanahEntry *self, gsize *length, guint *version);
void almanah_entry_set_data (AlmanahEntry *self, const guint8 *data, gsize length, guint version);
gboolean almanah_entry_get_content (AlmanahEntry *self, GtkTextBuffer *text_buffer, gboolean create_tags, GError **error);
void almanah_entry_set_content (AlmanahEntry *self, GtkTextBuffer *text_buffer);

void almanah_entry_get_date (AlmanahEntry *self, GDate *date);
AlmanahEntryEditability almanah_entry_get_editability (AlmanahEntry *self);
gboolean almanah_entry_is_empty (AlmanahEntry *self);

gboolean almanah_entry_is_important (AlmanahEntry *self);
void almanah_entry_set_is_important (AlmanahEntry *self, gboolean is_important);

void almanah_entry_get_last_edited (AlmanahEntry *self, GDate *last_edited);
void almanah_entry_set_last_edited (AlmanahEntry *self, GDate *last_edited);

G_END_DECLS

#endif /* !ALMANAH_ENTRY_H */
