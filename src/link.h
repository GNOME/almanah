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

#ifndef ALMANAH_LINK_H
#define ALMANAH_LINK_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ALMANAH_TYPE_LINK		(almanah_link_get_type ())
#define ALMANAH_LINK(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_LINK, AlmanahLink))
#define ALMANAH_LINK_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_LINK, AlmanahLinkClass))
#define ALMANAH_IS_LINK(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_LINK))
#define ALMANAH_IS_LINK_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_LINK))
#define ALMANAH_LINK_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_LINK, AlmanahLinkClass))

typedef struct _AlmanahLinkPrivate	AlmanahLinkPrivate;

typedef struct {
	GObject parent;
	AlmanahLinkPrivate *priv;
} AlmanahLink;

typedef struct {
	GObjectClass parent;

	gchar *type_id;
	gchar *name;
	gchar *description;
	gchar *icon_name;

	gchar *(*format_value) (AlmanahLink *link);
	gboolean (*view) (AlmanahLink *link);
	void (*build_dialog) (AlmanahLink *link, GtkVBox *parent_vbox);
	void (*get_values) (AlmanahLink *link);
} AlmanahLinkClass;

GType almanah_link_get_type (void);
AlmanahLink *almanah_link_new (const gchar *type_id);
gchar *almanah_link_format_value (AlmanahLink *self);
gboolean almanah_link_view (AlmanahLink *self);
void almanah_link_build_dialog (AlmanahLink *self, GtkVBox *parent_vbox);
void almanah_link_get_values (AlmanahLink *self);
void almanah_link_populate_model (GtkListStore *list_store, guint type_id_column, guint name_column, guint icon_name_column);
const gchar *almanah_link_get_type_id (AlmanahLink *self);
const gchar *almanah_link_get_name (AlmanahLink *self);
const gchar *almanah_link_get_description (AlmanahLink *self);
const gchar *almanah_link_get_icon_name (AlmanahLink *self);
gchar *almanah_link_get_value (AlmanahLink *self);
void almanah_link_set_value (AlmanahLink *self, const gchar *value);
gchar *almanah_link_get_value2 (AlmanahLink *self);
void almanah_link_set_value2 (AlmanahLink *self, const gchar *value);

G_END_DECLS

#endif /* !ALMANAH_LINK_H */
