/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Álvaro Peña 2011 <alvaropg@gmail.com>
 *
 * Almanah is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Almanah is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Almanah.	If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ALMANAH_FONT_STYLE_MENU_ACTION_H
#define ALMANAH_FONT_STYLE_MENU_ACTION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ALMANAH_TYPE_FONT_STYLE_MENU_ACTION	    (almanah_font_style_menu_action_get_type ())
#define ALMANAH_FONT_STYLE_MENU_ACTION(o)	    (G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_FONT_STYLE_MENU_ACTION, AlmanahFontStyleMenuAction))
#define ALMANAH_FONT_STYLE_MENU_ACTION_CLASS(k)	    (G_TYPE_CHECK_CLASS_CAST ((k), ALMANAH_TYPE_FONT_STYLE_MENU_ACTION, AlmanahFontStyleMenuActionClass))
#define ALMANAH_IS_FONT_STYLE_MENU_ACTION(o)	    (G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_FONT_STYLE_MENU_ACTION))
#define ALMANAH_IS_FONT_STYLE_MENU_ACTION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_FONT_STYLE_MENU_ACTION))
#define ALMANAH_FONT_STYLE_MENU_ACTION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_FONT_STYLE_MENU_ACTION, AlmanahFontStyleMenuAction))

typedef struct {
	GtkAction parent;
} AlmanahFontStyleMenuAction;

typedef struct {
	GtkActionClass parent_class;
} AlmanahFontStyleMenuActionClass;

GType almanah_font_style_menu_action_get_type (void) G_GNUC_CONST;
GtkAction *almanah_font_style_menu_action_new (const gchar *name, const gchar *label, const gchar *tooltip, const gchar *stock_id) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

G_END_DECLS

#endif /* !ALMANAH_FONT_STYLE_MENU_ACTION_H__ */
