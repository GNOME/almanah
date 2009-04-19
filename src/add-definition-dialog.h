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

#ifndef ALMANAH_ADD_DEFINITION_DIALOG_H
#define ALMANAH_ADD_DEFINITION_DIALOG_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "definition.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_ADD_DEFINITION_DIALOG		(almanah_add_definition_dialog_get_type ())
#define ALMANAH_ADD_DEFINITION_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_ADD_DEFINITION_DIALOG, AlmanahAddDefinitionDialog))
#define ALMANAH_ADD_DEFINITION_DIALOG_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_ADD_DEFINITION_DIALOG, AlmanahAddDefinitionDialogClass))
#define ALMANAH_IS_ADD_DEFINITION_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_ADD_DEFINITION_DIALOG))
#define ALMANAH_IS_ADD_DEFINITION_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_ADD_DEFINITION_DIALOG))
#define ALMANAH_ADD_DEFINITION_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_ADD_DEFINITION_DIALOG, AlmanahAddDefinitionDialogClass))

typedef struct _AlmanahAddDefinitionDialogPrivate	AlmanahAddDefinitionDialogPrivate;

typedef struct {
	GtkDialog parent;
	AlmanahAddDefinitionDialogPrivate *priv;
} AlmanahAddDefinitionDialog;

typedef struct {
	GtkDialogClass parent;
} AlmanahAddDefinitionDialogClass;

GType almanah_add_definition_dialog_get_type (void);

AlmanahAddDefinitionDialog *almanah_add_definition_dialog_new (void);

const gchar *almanah_add_definition_dialog_get_text (AlmanahAddDefinitionDialog *self);
void almanah_add_definition_dialog_set_text (AlmanahAddDefinitionDialog *self, const gchar *text);

AlmanahDefinition *almanah_add_definition_dialog_get_definition (AlmanahAddDefinitionDialog *self);

G_END_DECLS

#endif /* !ALMANAH_ADD_DEFINITION_DIALOG_H */
