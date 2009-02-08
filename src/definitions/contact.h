/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2009 <philip@tecnocode.co.uk>
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

#ifndef ALMANAH_CONTACT_DEFINITION_H
#define ALMANAH_CONTACT_DEFINITION_H

#include <glib.h>
#include <glib-object.h>

#include "../definition.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_CONTACT_DEFINITION		(almanah_contact_definition_get_type ())
#define ALMANAH_CONTACT_DEFINITION(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_CONTACT_DEFINITION, AlmanahContactDefinition))
#define ALMANAH_CONTACT_DEFINITION_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_CONTACT_DEFINITION, AlmanahContactDefinitionClass))
#define ALMANAH_IS_CONTACT_DEFINITION(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_CONTACT_DEFINITION))
#define ALMANAH_IS_CONTACT_DEFINITION_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_CONTACT_DEFINITION))
#define ALMANAH_CONTACT_DEFINITION_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_CONTACT_DEFINITION, AlmanahContactDefinitionClass))

typedef struct _AlmanahContactDefinitionPrivate	AlmanahContactDefinitionPrivate;

typedef struct {
	AlmanahDefinition parent;
	AlmanahContactDefinitionPrivate *priv;
} AlmanahContactDefinition;

typedef struct {
	AlmanahDefinitionClass parent;
} AlmanahContactDefinitionClass;

GType almanah_contact_definition_get_type (void);

G_END_DECLS

#endif /* !ALMANAH_CONTACT_DEFINITION_H */
