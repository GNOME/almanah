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

G_DECLARE_FINAL_TYPE (AlmanahTagAccessible, almanah_tag_accessible, ALMANAH, TAG_ACCESSIBLE, GtkWidgetAccessible)

G_END_DECLS

#endif /* !ALMANAH_TAG_ACCESSIBLE_H */
