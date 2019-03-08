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

#include <glib/gi18n.h>
#include <pango/pangocairo.h>
#include <math.h>

#include "tag.h"
#include "tag-accessible.h"

#define PADDING_TOP    1
#define PADDING_BOTTOM 1
#define PADDING_LEFT   10
#define PADDING_RIGHT  5
#define SHADOW_RIGHT 1
#define SHADOW_BOTTOM 2
#define CLOSE_BUTTON 5
#define CLOSE_BUTTON_SPACING 5

enum {
	PROP_TAG = 1
};

typedef struct {
	gchar *tag;
	PangoLayout *layout;

	/* Tag colors */
	GdkRGBA text_color;
	GdkRGBA strock_color;
	GdkRGBA fill_a_color;
	GdkRGBA fill_b_color;

	/* Some coordinates */
	gint close_x;
	gint close_y;

	/* The close button state */
	gboolean close_highlighted;
	gboolean close_pressed;
} AlmanahTagPrivate;

struct _AlmanahTag {
	GtkDrawingArea parent;
};

enum {
	SIGNAL_REMOVE,
	LAST_SIGNAL
};

static guint tag_signals[LAST_SIGNAL] = { 0, };

static void almanah_tag_finalize             (GObject *object);
static void almanah_tag_get_property         (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_tag_set_property         (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
void        almanah_tag_ensure_layout        (AlmanahTag *self);
void        almanah_tag_get_preferred_width  (GtkWidget *widget, gint *minimum_width, gint *natural_width);
void        almanah_tag_get_preferred_height (GtkWidget *widget, gint *minimum_height, gint *natural_height);
gboolean    almanah_tag_motion_notify_event  (GtkWidget *widget, GdkEventMotion *event);
gboolean    almanah_tag_button_press_event   (GtkWidget *widget, GdkEventButton *event);
gboolean    almanah_tag_button_release_event (GtkWidget *widget, GdkEventButton *event);
gboolean    almanah_tag_draw                 (GtkWidget *widget, cairo_t *cr, gpointer data);
gboolean    almanah_tag_query_tooltip        (GtkWidget *widget, gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip);

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahTag, almanah_tag, GTK_TYPE_DRAWING_AREA)

static void
almanah_tag_class_init (AlmanahTagClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->get_property = almanah_tag_get_property;
	gobject_class->set_property = almanah_tag_set_property;
	gobject_class->finalize = almanah_tag_finalize;

	widget_class->get_preferred_width = almanah_tag_get_preferred_width;
	widget_class->get_preferred_height = almanah_tag_get_preferred_height;
	widget_class->motion_notify_event = almanah_tag_motion_notify_event;
	widget_class->button_release_event = almanah_tag_button_release_event;
	widget_class->button_press_event = almanah_tag_button_press_event;
	widget_class->query_tooltip = almanah_tag_query_tooltip;

	g_object_class_install_property (gobject_class, PROP_TAG,
					 g_param_spec_string ("tag",
							      "Tag", "The tag name.",
							      NULL, G_PARAM_READWRITE));

	tag_signals[SIGNAL_REMOVE] = g_signal_new ("remove",
						   G_TYPE_FROM_CLASS (klass),
						   G_SIGNAL_RUN_LAST,
						   0, NULL, NULL,
						   g_cclosure_marshal_VOID__VOID,
						   G_TYPE_NONE, 0);

	gtk_widget_class_set_accessible_type (widget_class, ALMANAH_TYPE_TAG_ACCESSIBLE);
}

static void
almanah_tag_init (AlmanahTag *self)
{
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (self);

	g_signal_connect (G_OBJECT (self), "draw", G_CALLBACK (almanah_tag_draw), NULL);

	gtk_widget_add_events (GTK_WIDGET  (self),
			       GDK_POINTER_MOTION_MASK
			       | GDK_BUTTON_PRESS_MASK
			       | GDK_BUTTON_RELEASE_MASK);

	gdk_rgba_parse (&priv->text_color, "#936835");
	gdk_rgba_parse (&priv->strock_color, "#ECB447");
	gdk_rgba_parse (&priv->fill_a_color, "#FFDB73");
	gdk_rgba_parse (&priv->fill_b_color, "#FCBC4E");

	gtk_widget_set_has_tooltip (GTK_WIDGET (self), TRUE);

	priv->close_highlighted = FALSE;
	priv->close_pressed = FALSE;

	gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);
}

static void
almanah_tag_finalize (GObject *object)
{
	AlmanahTag *tag = ALMANAH_TAG (object);
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (tag);

	g_free (priv->tag);
	g_clear_object (&priv->layout);

	G_OBJECT_CLASS (almanah_tag_parent_class)->finalize (object);
}

static void
almanah_tag_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahTag *tag = ALMANAH_TAG (object);
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (tag);

	switch (property_id) {
		case PROP_TAG:
			g_value_set_string (value, priv->tag);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_tag_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahTag *tag = ALMANAH_TAG (object);
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (tag);

	switch (property_id) {
		case PROP_TAG:
			if (priv->tag)
				g_free (priv->tag);
			priv->tag = g_strdup (g_value_get_string (value));
			if (PANGO_IS_LAYOUT (priv->layout)) {
				pango_layout_set_text (priv->layout, priv->tag, -1);
				gtk_widget_queue_resize (GTK_WIDGET (object));
			}
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

void
almanah_tag_ensure_layout (AlmanahTag *self)
{
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (self);

	if (!priv->layout) {
		GtkStyleContext *style_context;
		PangoFontDescription *font_desc;

		priv->layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), priv->tag);
		style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
		gtk_style_context_get (style_context, GTK_STATE_FLAG_NORMAL, "font", &font_desc, NULL);
		pango_font_description_set_size (font_desc, (pango_font_description_get_size (font_desc) * 0.8));
		pango_font_description_set_weight (font_desc, PANGO_WEIGHT_BOLD);
		pango_layout_set_font_description (priv->layout, font_desc);
	}
}

void
almanah_tag_get_preferred_height (GtkWidget *widget, gint *minimum_height, gint *natural_height)
{
	AlmanahTag *tag = ALMANAH_TAG (widget);
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (tag);
	gint height;

	almanah_tag_ensure_layout (tag);

	pango_layout_get_size (priv->layout, NULL, &height);
	*minimum_height = (height / PANGO_SCALE) + PADDING_TOP + PADDING_BOTTOM + SHADOW_BOTTOM;
	*natural_height = *minimum_height;
}

void
almanah_tag_get_preferred_width (GtkWidget *widget, gint *minimum_width, gint *natural_width)
{
	AlmanahTag *tag = ALMANAH_TAG (widget);
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (tag);
	gint width;

	almanah_tag_ensure_layout (tag);

	pango_layout_get_size (priv->layout, &width, NULL);
	*minimum_width = (width / PANGO_SCALE) + CLOSE_BUTTON_SPACING + CLOSE_BUTTON + PADDING_LEFT + PADDING_RIGHT + SHADOW_RIGHT;
	*natural_width = *minimum_width;
}

gboolean
almanah_tag_motion_notify_event  (GtkWidget *widget, GdkEventMotion *event)
{
	AlmanahTag *tag = ALMANAH_TAG (widget);
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (tag);
	gint close_x = priv->close_x;
	gint close_y = priv->close_y;
	gboolean close_highlighted;

	/* Close button */
	if (event->x >= close_x && event->x <= close_x + CLOSE_BUTTON
	    && event->y >= close_y && event->y <= close_y + CLOSE_BUTTON) {
		close_highlighted = TRUE;
	} else {
		close_highlighted = FALSE;
	}

	if (priv->close_highlighted != close_highlighted) {
		priv->close_highlighted = close_highlighted;
		gtk_widget_queue_draw (widget);
	}

	return FALSE;
}

gboolean
almanah_tag_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	AlmanahTag *tag = ALMANAH_TAG (widget);
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (tag);
	gint close_x = priv->close_x;
	gint close_y = priv->close_y;

	if (event->x >= close_x && event->x <= close_x + CLOSE_BUTTON
	    && event->y >= close_y && event->y <= close_y + CLOSE_BUTTON
	    && event->button == 1) {
		priv->close_pressed = TRUE;
	} else {
		priv->close_pressed = FALSE;
	}

	return FALSE;
}

gboolean
almanah_tag_button_release_event (GtkWidget *widget, GdkEventButton *event)
{
	AlmanahTag *tag = ALMANAH_TAG (widget);
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (tag);
	gint close_x = priv->close_x;
	gint close_y = priv->close_y;

	if (event->x >= close_x && event->x <= close_x + CLOSE_BUTTON
	    && event->y >= close_y && event->y <= close_y + CLOSE_BUTTON
	    && priv->close_pressed
	    && event->button == 1) {
		g_signal_emit (tag, tag_signals[SIGNAL_REMOVE], 0);
	}
	priv->close_pressed = FALSE;

	return FALSE;
}

gboolean
almanah_tag_draw (GtkWidget *widget, cairo_t *cr, gpointer data)
{
	AlmanahTag *tag = ALMANAH_TAG (widget);
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (tag);
	gint y_origin, allocated_height, width, height, middle_height, middle_padding_left, text_height, text_width;
	cairo_pattern_t *fill_pattrn;

	almanah_tag_ensure_layout (tag);

	/* Get the tag dimensions */
	gtk_widget_get_preferred_width (widget, &width, NULL);
	gtk_widget_get_preferred_height (widget, &height, NULL);
	width = width - SHADOW_RIGHT;
	height = height - SHADOW_BOTTOM;

	/* Some coordinates */
	middle_height = height / 2;
	middle_padding_left = PADDING_LEFT / 2;

	/* The tag must be vertical centered */
	allocated_height = gtk_widget_get_allocated_height (widget);
	y_origin = (allocated_height / 2) - middle_height;

	/* Tag border */
	cairo_set_line_width (cr, 1);
	cairo_move_to (cr, width - 2, y_origin);
	cairo_line_to (cr, middle_padding_left, y_origin);
	cairo_line_to (cr, 0, y_origin + middle_height);
	cairo_line_to (cr, middle_padding_left, y_origin + height);
	cairo_line_to (cr, width - 2, y_origin + height);
	cairo_line_to (cr, width, y_origin + height - 2);
	cairo_line_to (cr, width, y_origin + 2);
	cairo_close_path (cr);
	/* gradient background */
	fill_pattrn = cairo_pattern_create_linear (1, y_origin + 1, 2, y_origin + height - 1);
	cairo_pattern_add_color_stop_rgb (fill_pattrn, 0,
					  priv->fill_a_color.red,
					  priv->fill_a_color.green,
					  priv->fill_a_color.blue);
	cairo_pattern_add_color_stop_rgb (fill_pattrn, 1,
					  priv->fill_b_color.red,
					  priv->fill_b_color.green,
					  priv->fill_b_color.blue);
	cairo_set_source (cr, fill_pattrn);
	cairo_fill_preserve (cr);
	/* paint the border */
	gdk_cairo_set_source_rgba (cr, &priv->strock_color);
	cairo_stroke (cr);
	/* a little white line (biselado ?) */
	cairo_set_line_width (cr, 0.5);
	cairo_move_to (cr, middle_padding_left, y_origin + 1);
	cairo_line_to (cr, width - 1, y_origin + 1);
	cairo_set_source_rgba (cr, 1, 1, 1, 0.6);
	cairo_stroke (cr);

	/* Little circle */
	cairo_set_line_width (cr, 1);
	cairo_arc (cr, middle_padding_left, y_origin + middle_height, 2, 0, 2 * M_PI);
	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_fill_preserve (cr);
	gdk_cairo_set_source_rgba (cr, &priv->strock_color);
	cairo_stroke (cr);

	/* Tag text */
	pango_layout_get_size (priv->layout, &text_width, &text_height);
	text_height = text_height / PANGO_SCALE;
	text_width = text_width / PANGO_SCALE;
	/* A white text shadow */
	cairo_set_source_rgba (cr, 1, 1, 1, 0.5);
	cairo_move_to (cr, PADDING_LEFT, y_origin + middle_height - (text_height /2));
	pango_cairo_show_layout (cr, priv->layout);
	/* The text */
	gdk_cairo_set_source_rgba (cr, &priv->text_color);
	cairo_move_to (cr, PADDING_LEFT, y_origin + middle_height - (text_height /2) - 1);
	pango_cairo_show_layout (cr, priv->layout);

	/* Remove button, it's a "x" */
	priv->close_x = PADDING_LEFT + text_width + CLOSE_BUTTON_SPACING;
	priv->close_y = y_origin + middle_height - (CLOSE_BUTTON / 2);

	/* First the shadow */
	cairo_set_source_rgba (cr, 1, 1, 1, 0.5);
	/* Line from left up */
	cairo_move_to (cr, priv->close_x, priv->close_y + 1);
	/* To right down */
	cairo_line_to (cr, priv->close_x + CLOSE_BUTTON, y_origin + middle_height + (CLOSE_BUTTON / 2) + 1);
	cairo_stroke (cr);
	/* From right left */
	cairo_move_to (cr, priv->close_x + CLOSE_BUTTON, priv->close_y + 1);
	/* To left down */
	cairo_line_to (cr, priv->close_x, y_origin + middle_height + (CLOSE_BUTTON / 2) + 1);
	cairo_stroke (cr);

	if (priv->close_highlighted) {
		gdk_cairo_set_source_rgba (cr, &priv->fill_a_color);
	} else {
		gdk_cairo_set_source_rgba (cr, &priv->text_color);
	}
	/* Line from left up */
	cairo_move_to (cr, priv->close_x, priv->close_y);
	/* To right down */
	cairo_line_to (cr, priv->close_x + CLOSE_BUTTON, y_origin + middle_height + (CLOSE_BUTTON / 2));
	cairo_stroke (cr);
	/* From right left */
	cairo_move_to (cr, priv->close_x + CLOSE_BUTTON, priv->close_y);
	/* To left down */
	cairo_line_to (cr, priv->close_x, y_origin + middle_height + (CLOSE_BUTTON / 2));
	cairo_stroke (cr);

	/* Focus */
	if (gtk_widget_has_focus (widget))
		gtk_render_focus (gtk_widget_get_style_context (widget),
				  cr,
				  0, 0,
				  gtk_widget_get_allocated_width (widget),
				  gtk_widget_get_allocated_height (widget));

	return FALSE;
}

gboolean
almanah_tag_query_tooltip (GtkWidget *widget, gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip)
{
	AlmanahTag *tag = ALMANAH_TAG (widget);
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (tag);
	gint close_x = priv->close_x;
	gint close_y = priv->close_y;

	/* @TODO: Maybe remove all this code and test if priv->close_highlighted == TRUE, or just return it */
	if (x >= close_x && x <= close_x + CLOSE_BUTTON
	    && y >= close_y && y <= close_y + CLOSE_BUTTON) {
		/* Looks like gtk_widget_set_tooltip_text don't works here, even in the init... ? */
		gtk_tooltip_set_text (tooltip, _("Remove tag"));
		return TRUE;
	} else {
		return FALSE;
	}
}

GtkWidget *
almanah_tag_new (const gchar *tag)
{
	return GTK_WIDGET (g_object_new (ALMANAH_TYPE_TAG,
					 "tag", tag,
					 NULL));
}

const gchar *
almanah_tag_get_tag (AlmanahTag *tag_widget)
{
	g_return_val_if_fail (ALMANAH_IS_TAG (tag_widget), NULL);

	AlmanahTag *tag = ALMANAH_TAG (tag_widget);
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (tag);

	return priv->tag;
}

void
almanah_tag_remove (AlmanahTag *tag_widget)
{
	g_return_if_fail (ALMANAH_IS_TAG (tag_widget));

	g_signal_emit (tag_widget, tag_signals[SIGNAL_REMOVE], 0);
}
