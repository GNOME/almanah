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

#include <gtk/gtk.h>

#include "font-style-menu-action.h"

G_DEFINE_TYPE (AlmanahFontStyleMenuAction, almanah_font_style_menu_action, GTK_TYPE_ACTION)

static void
almanah_font_style_menu_action_class_init (AlmanahFontStyleMenuActionClass *klass)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS (klass);

	/* This class only has been created to allow a menu into a toolitem
	 * in the data/almanah.ui file (see the unique toolbar) in the way
	 * described in the GtkUIManager documentation ("Note that toolitem
	 * elements may contain a menu element, but only if their associated
	 * action specifies a GtkMenuToolButton as proxy")
	 *
	 * See: https://bugzilla.gnome.org/show_bug.cgi?id=590335
	 */
	action_class->toolbar_item_type = GTK_TYPE_MENU_TOOL_BUTTON;
}

static void
almanah_font_style_menu_action_init (AlmanahFontStyleMenuAction *self)
{
}

GtkAction *
almanah_font_style_menu_action_new (const gchar *name, const gchar *label, const gchar *tooltip, const gchar *stock_id)
{
	g_return_val_if_fail (name != NULL, NULL);

	return g_object_new (ALMANAH_TYPE_FONT_STYLE_MENU_ACTION,
			     "name", name,
			     "label", label,
			     "tooltip", tooltip,
			     "stock-id", stock_id,
			     NULL);
}
