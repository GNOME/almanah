/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-cell-renderer-color.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _E_CELL_RENDERER_COLOR_H_
#define _E_CELL_RENDERER_COLOR_H_

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_CELL_RENDERER_COLOR \
	(e_cell_renderer_color_get_type ())

G_DECLARE_FINAL_TYPE (ECellRendererColor, e_cell_renderer_color, E, CELL_RENDERER_COLOR, GtkCellRenderer)

G_BEGIN_DECLS

GtkCellRenderer *e_cell_renderer_color_new	(void);

G_END_DECLS

#endif /* _E_CELL_RENDERER_COLOR_H_ */
