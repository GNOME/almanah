/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Álvaro Peña 2012 <alvaropg@gmail.com>
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

#include <config.h>
#include <glib/gi18n.h>

#include "calendar-window.h"
#include "widgets/calendar.h"
#include "interface.h"

static void show (GtkWidget *self);
static gboolean button_press_event (GtkWidget *self, GdkEventButton *event);
static gboolean key_press_event (GtkWidget *self, GdkEventKey *event);

struct _AlmanahCalendarWindowPrivate {
	GdkDevice *grab_pointer;
};

G_DEFINE_TYPE (AlmanahCalendarWindow, almanah_calendar_window, GTK_TYPE_WINDOW)
#define ALMANAH_CALENDAR_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_CALENDAR_WINDOW, AlmanahCalendarWindowPrivate))

static void
almanah_calendar_window_class_init (AlmanahCalendarWindowClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahCalendarWindowPrivate));

	widget_class->show = show;
	widget_class->button_press_event = button_press_event;
	widget_class->key_press_event = key_press_event;
}

static void
almanah_calendar_window_init (AlmanahCalendarWindow *self)
{
	gchar *css_path;
	GtkCssProvider *style_provider;
	GError *error = NULL;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_CALENDAR_WINDOW, AlmanahCalendarWindowPrivate);

	css_path = g_build_filename (almanah_get_css_path (), "calendar-window.css", NULL);
	style_provider = gtk_css_provider_new ();
	if (!gtk_css_provider_load_from_path (style_provider, css_path, NULL)) {
		/* Error loading the CSS */
		g_warning (_("Couldn't load the CSS file '%s' for calendar window. The interface might not be styled correctly"), css_path);
		g_error_free (error);
	} else {
		GtkStyleContext *style_context;

		style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
		gtk_style_context_add_provider (style_context, GTK_STYLE_PROVIDER (style_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}

	g_free (css_path);
	g_object_unref (style_provider);
}

static void
show (GtkWidget *self)
{
	GdkDevice *device, *keyboard, *pointer;
	GdkWindow *window;

	GTK_WIDGET_CLASS (almanah_calendar_window_parent_class)->show (self);

	window = gtk_widget_get_window (self);

	device = gtk_get_current_event_device ();
	switch (gdk_device_get_source (device)) {
		case GDK_SOURCE_KEYBOARD:
			keyboard = device;
			pointer = gdk_device_get_associated_device (device);
			break;
		case GDK_SOURCE_MOUSE:
		case GDK_SOURCE_PEN:
		case GDK_SOURCE_ERASER:
		case GDK_SOURCE_CURSOR:
			pointer = device;
			keyboard = gdk_device_get_associated_device (device);
			break;
		default:
			g_warning (_("Unknown input device"));
			return;
	}

	gtk_grab_add (self);

	gdk_device_grab (keyboard, window,
			 GDK_OWNERSHIP_WINDOW, TRUE,
			 GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
			 NULL, GDK_CURRENT_TIME);

	gdk_device_grab (pointer, window,
			 GDK_OWNERSHIP_WINDOW, TRUE,
			 GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			 GDK_POINTER_MOTION_MASK,
			 NULL, GDK_CURRENT_TIME);

	ALMANAH_CALENDAR_WINDOW_GET_PRIVATE(ALMANAH_CALENDAR_WINDOW (self))->grab_pointer = pointer;
}

static gboolean
button_press_event (GtkWidget *self, GdkEventButton *event)
{
	GtkAllocation allocation;
	gint window_x, window_y;

	if (event->type != GDK_BUTTON_PRESS) {
		return TRUE;
	}

	/* Take the dock window position and dimensions */
	gdk_window_get_position (gtk_widget_get_window (self), &window_x, &window_y);
	gtk_widget_get_allocation (self, &allocation);

	/* Hide the dock when the user clicks out of the dock window */
	if (event->x_root < window_x || event->x_root > window_x + allocation.width ||
	    event->y_root < window_y || event->y_root > window_y + allocation.height) {
		almanah_calendar_window_popdown (ALMANAH_CALENDAR_WINDOW (self));
	}

	return FALSE;
}

static gboolean
key_press_event (GtkWidget *self, GdkEventKey *event)
{
	if (event->keyval != GDK_KEY_Escape) {
		return FALSE;
	}

	almanah_calendar_window_popdown (ALMANAH_CALENDAR_WINDOW (self));

	return TRUE;
}

GtkWidget *
almanah_calendar_window_new (void)
{
	return GTK_WIDGET (g_object_new (ALMANAH_TYPE_CALENDAR_WINDOW, NULL));
}

void
almanah_calendar_window_popup (AlmanahCalendarWindow *self)
{
	g_return_if_fail (ALMANAH_IS_CALENDAR_WINDOW (self));

	gtk_widget_show_all (GTK_WIDGET (self));
}

void
almanah_calendar_window_popdown (AlmanahCalendarWindow *self)
{
	GdkDevice *pointer, *keyboard;

	g_return_if_fail (ALMANAH_IS_CALENDAR_WINDOW (self));

	pointer = self->priv->grab_pointer;
	gdk_device_ungrab (pointer, GDK_CURRENT_TIME);
	keyboard = gdk_device_get_associated_device (pointer);
	if (keyboard)
		gdk_device_ungrab (keyboard, GDK_CURRENT_TIME);

	gtk_grab_remove (GTK_WIDGET (self));

	gtk_widget_hide (GTK_WIDGET (self));
}
