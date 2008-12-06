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

#ifndef ALMANAH_NOTE_DEFINITION_H
#define ALMANAH_NOTE_DEFINITION_H

#include <glib.h>
#include <glib-object.h>

#include "../definition.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_NOTE_DEFINITION		(almanah_note_definition_get_type ())
#define ALMANAH_NOTE_DEFINITION(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_NOTE_DEFINITION, AlmanahNoteDefinition))
#define ALMANAH_NOTE_DEFINITION_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_NOTE_DEFINITION, AlmanahNoteDefinitionClass))
#define ALMANAH_IS_NOTE_DEFINITION(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_NOTE_DEFINITION))
#define ALMANAH_IS_NOTE_DEFINITION_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_NOTE_DEFINITION))
#define ALMANAH_NOTE_DEFINITION_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_NOTE_DEFINITION, AlmanahNoteDefinitionClass))

typedef struct _AlmanahNoteDefinitionPrivate	AlmanahNoteDefinitionPrivate;

typedef struct {
	AlmanahDefinition parent;
	AlmanahNoteDefinitionPrivate *priv;
} AlmanahNoteDefinition;

typedef struct {
	AlmanahDefinitionClass parent;
} AlmanahNoteDefinitionClass;

GType almanah_note_definition_get_type (void);

G_END_DECLS

#endif /* !ALMANAH_NOTE_DEFINITION_H */
