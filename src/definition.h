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

#ifndef ALMANAH_DEFINITION_H
#define ALMANAH_DEFINITION_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
	ALMANAH_DEFINITION_UNKNOWN = 0,
	ALMANAH_DEFINITION_FILE = 1,
	ALMANAH_DEFINITION_NOTE,
	ALMANAH_DEFINITION_URI
} AlmanahDefinitionType;

#define ALMANAH_TYPE_DEFINITION			(almanah_definition_get_type ())
#define ALMANAH_DEFINITION(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_DEFINITION, AlmanahDefinition))
#define ALMANAH_DEFINITION_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_DEFINITION, AlmanahDefinitionClass))
#define ALMANAH_IS_DEFINITION(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_DEFINITION))
#define ALMANAH_IS_DEFINITION_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_DEFINITION))
#define ALMANAH_DEFINITION_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_DEFINITION, AlmanahDefinitionClass))

typedef struct _AlmanahDefinitionPrivate	AlmanahDefinitionPrivate;

typedef struct {
	GObject parent;
	AlmanahDefinitionPrivate *priv;
} AlmanahDefinition;

typedef struct {
	GObjectClass parent;

	AlmanahDefinitionType type_id;
	gchar *name;
	gchar *description;
	gchar *icon_name;

	gboolean (*view) (AlmanahDefinition *definition);
	void (*build_dialog) (AlmanahDefinition *definition, GtkVBox *parent_vbox);
	void (*close_dialog) (AlmanahDefinition *definition, GtkVBox *parent_vbox);
	void (*parse_text) (AlmanahDefinition *definition, const gchar *text);
} AlmanahDefinitionClass;

GType almanah_definition_get_type (void);

AlmanahDefinition *almanah_definition_new (AlmanahDefinitionType type_id);

AlmanahDefinitionType almanah_definition_get_type_id (AlmanahDefinition *self);
const gchar *almanah_definition_get_name (AlmanahDefinition *self);
const gchar *almanah_definition_get_description (AlmanahDefinition *self);
const gchar *almanah_definition_get_icon_name (AlmanahDefinition *self);

gboolean almanah_definition_view (AlmanahDefinition *self);
void almanah_definition_build_dialog (AlmanahDefinition *self, GtkVBox *parent_vbox);
void almanah_definition_close_dialog (AlmanahDefinition *self, GtkVBox *parent_vbox);
void almanah_definition_parse_text (AlmanahDefinition *self, const gchar *text);

const gchar *almanah_definition_get_text (AlmanahDefinition *self);
void almanah_definition_set_text (AlmanahDefinition *self, const gchar *text);

const gchar *almanah_definition_get_value (AlmanahDefinition *self);
void almanah_definition_set_value (AlmanahDefinition *self, const gchar *value);
const gchar *almanah_definition_get_value2 (AlmanahDefinition *self);
void almanah_definition_set_value2 (AlmanahDefinition *self, const gchar *value);

void almanah_definition_populate_model (GtkListStore *list_store, guint type_id_column, guint name_column, guint icon_name_column);

G_END_DECLS

#endif /* !ALMANAH_DEFINITION_H */
