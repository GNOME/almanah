/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Álvaro Peña 2013 <alvaropg@gmail.com>
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

#ifndef ALMANAH_TAG_ACCESSIBLE_H
#define ALMANAH_TAG_ACCESSIBLE_H

#include <gtk/gtk.h>
#include <gtk/gtk-a11y.h>

G_BEGIN_DECLS

#define ALMANAH_TYPE_TAG_ACCESSIBLE         (almanah_tag_accessible_get_type ())
#define ALMANAH_TAG_ACCESSIBLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_TAG_ACCESSIBLE, AlmanahTagAccessible))
#define ALMANAH_TAG_ACCESSIBLE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_TAG_ACCESSIBLE, AlmanahTagAccessibleClass))
#define ALMANAH_IS_TAG_ACCESSIBLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_TAG_ACCESSIBLE))
#define ALMANAH_IS_TAG_ACCESSIBLE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_TAG_ACCESSIBLE))
#define ALMANAH_TAG_ACCESSIBLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_TAG_ACCESSIBLE, AlmanahTagAccessibleClass))

typedef struct _AlmanahTagAccessiblePrivate AlmanahTagAccessiblePrivate;

typedef struct {
	GtkWidgetAccessible parent;

	AlmanahTagAccessiblePrivate *priv;
} AlmanahTagAccessible;

typedef struct {
	GtkWidgetAccessibleClass parent;
} AlmanahTagAccessibleClass;

GType almanah_tag_accessible_get_type  (void) G_GNUC_CONST;

G_END_DECLS

#endif /* !ALMANAH_TAG_ACCESSIBLE_H */
