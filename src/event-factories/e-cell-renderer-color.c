/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-cell-renderer-color.c
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

#include <config.h>

#include "e-cell-renderer-color.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#define E_CELL_RENDERER_COLOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CELL_RENDERER_COLOR, ECellRendererColorPrivate))

enum {
	PROP_0,
	PROP_COLOR
};

struct _ECellRendererColorPrivate {
	GdkRGBA *color;
};

G_DEFINE_TYPE (
	ECellRendererColor,
	e_cell_renderer_color,
	GTK_TYPE_CELL_RENDERER)

static void
cell_renderer_color_get_size (GtkCellRenderer *cell,
                              GtkWidget *widget,
                              const GdkRectangle *cell_area,
                              gint *x_offset,
                              gint *y_offset,
                              gint *width,
                              gint *height)
{
	gint color_width  = 16;
	gint color_height = 16;
	gint calc_width;
	gint calc_height;
	gfloat xalign;
	gfloat yalign;
	guint xpad;
	guint ypad;

	g_object_get (
		cell, "xalign", &xalign, "yalign", &yalign,
		"xpad", &xpad, "ypad", &ypad, NULL);

	calc_width  = (gint) xpad * 2 + color_width;
	calc_height = (gint) ypad * 2 + color_height;

	if (cell_area && color_width > 0 && color_height > 0) {
		if (x_offset) {
			*x_offset = (((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ?
					(1.0 - xalign) : xalign) *
					(cell_area->width - calc_width));
			*x_offset = MAX (*x_offset, 0);
		}

		if (y_offset) {
			*y_offset =(yalign *
				(cell_area->height - calc_height));
			*y_offset = MAX (*y_offset, 0);
		}
	} else {
		if (x_offset) *x_offset = 0;
		if (y_offset) *y_offset = 0;
	}

	if (width)
		*width = calc_width;

	if (height)
		*height = calc_height;
}

static void
cell_renderer_color_render (GtkCellRenderer *cell,
                            cairo_t *cr,
                            GtkWidget *widget,
                            const GdkRectangle *background_area,
                            const GdkRectangle *cell_area,
                            GtkCellRendererState flags)
{
	ECellRendererColorPrivate *priv;
	GdkRectangle pix_rect;
	GdkRectangle draw_rect;
	guint xpad;
	guint ypad;

	priv = E_CELL_RENDERER_COLOR_GET_PRIVATE (cell);

	if (priv->color == NULL)
		return;

	cell_renderer_color_get_size (
		cell, widget, cell_area,
		&pix_rect.x, &pix_rect.y,
		&pix_rect.width, &pix_rect.height);

	g_object_get (cell, "xpad", &xpad, "ypad", &ypad, NULL);

	pix_rect.x += cell_area->x + xpad;
	pix_rect.y += cell_area->y + ypad;
	pix_rect.width  -= xpad * 2;
	pix_rect.height -= ypad * 2;

	if (!gdk_rectangle_intersect (cell_area, &pix_rect, &draw_rect))
		return;

	gdk_cairo_set_source_rgba (cr, priv->color);
	cairo_rectangle (cr, pix_rect.x, pix_rect.y, draw_rect.width, draw_rect.height);

	cairo_fill (cr);
}

static void
cell_renderer_color_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	ECellRendererColorPrivate *priv;

	priv = E_CELL_RENDERER_COLOR_GET_PRIVATE (object);

	switch (property_id) {
		case PROP_COLOR:
			if (priv->color != NULL)
				gdk_rgba_free (priv->color);
			priv->color = g_value_dup_boxed (value);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cell_renderer_color_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	ECellRendererColorPrivate *priv;

	priv = E_CELL_RENDERER_COLOR_GET_PRIVATE (object);

	switch (property_id) {
		case PROP_COLOR:
			g_value_set_boxed (value, priv->color);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cell_renderer_color_finalize (GObject *object)
{
	ECellRendererColorPrivate *priv;

	priv = E_CELL_RENDERER_COLOR_GET_PRIVATE (object);

	if (priv->color != NULL)
		gdk_rgba_free (priv->color);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_cell_renderer_color_parent_class)->finalize (object);
}

static void
e_cell_renderer_color_class_init (ECellRendererColorClass *class)
{
	GObjectClass *object_class;
	GtkCellRendererClass *cell_class;

	g_type_class_add_private (class, sizeof (ECellRendererColorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = cell_renderer_color_set_property;
	object_class->get_property = cell_renderer_color_get_property;
	object_class->finalize = cell_renderer_color_finalize;

	cell_class = GTK_CELL_RENDERER_CLASS (class);
	cell_class->get_size = cell_renderer_color_get_size;
	cell_class->render = cell_renderer_color_render;

	g_object_class_install_property (
		object_class,
		PROP_COLOR,
		g_param_spec_boxed (
			"color",
			"Color Info",
			"The color to render",
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));
}

static void
e_cell_renderer_color_init (ECellRendererColor *cellcolor)
{
	cellcolor->priv = E_CELL_RENDERER_COLOR_GET_PRIVATE (cellcolor);

	g_object_set (cellcolor, "xpad", 4, NULL);
}

/**
 * e_cell_renderer_color_new:
 *
 * Since: 2.22
 **/
GtkCellRenderer *
e_cell_renderer_color_new (void)
{
	return g_object_new (E_TYPE_CELL_RENDERER_COLOR, NULL);
}
