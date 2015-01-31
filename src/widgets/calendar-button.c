/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Álvaro Peña 2011-2012 <alvaropg@gmail.com>
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
#include <config.h>

#include "calendar-button.h"
#include "calendar.h"
#include "calendar-window.h"
#include "interface.h"

/* This enum allows to know the reason why the calendar date has been changed */
enum {
	NONE_EVENT = 1, /* The window is showed */
	FIRST_EVENT,    /* The widget is instatiated the first time */
	TODAY_EVENT,    /* The user clicks on "Today" button */
	DAY_EVENT,      /* The user selects a concret day in the calendar widget */
	MONTH_EVENT     /* The user changes the month, or the year clicking in the calendar widget */
};

enum {
	DAY_SELECTED_SIGNAL,
	SELECT_DATE_CLICKED_SIGNAL,
	LAST_SIGNAL
};

enum {
	PROP_STORAGE_MANAGER = 1
};

static guint calendar_button_signals[LAST_SIGNAL] = { 0 };

struct _AlmanahCalendarButtonPrivate {
	GtkWidget *label;
	GtkWidget *dock;
	guchar user_event;
	AlmanahCalendar *calendar;
	GtkWidget *today_button;
	GtkWidget *select_date_button;
	AlmanahStorageManager *storage_manager;
};

static void almanah_calendar_button_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_calendar_button_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void almanah_calendar_button_finalize (GObject *object);

static void almanah_calendar_button_dock_hiden (GtkWidget *dock, AlmanahCalendarButton *self);

static void almanah_calendar_button_toggled (GtkToggleButton *togglebutton);
static void almanah_calendar_button_day_selected_cb (GtkCalendar *calendar, AlmanahCalendarButton *self);
static void almanah_calendar_button_month_changed_cb (GtkCalendar *calendar, AlmanahCalendarButton *self);
static gboolean almanah_calendar_button_today_press_cb         (GtkWidget *widget, GdkEvent *event, AlmanahCalendarButton *self);
static void     almanah_calendar_button_today_clicked_cb       (GtkButton *button, gpointer user_data);
static void     almanah_calendar_button_select_date_clicked_cb (GtkButton *button, gpointer user_data);
static gboolean almanah_calendar_button_select_date_press_cb   (GtkWidget *widget, GdkEvent *event, AlmanahCalendarButton *self);

static void dock_position_func (AlmanahCalendarButton *self, gint *x, gint *y);

G_DEFINE_TYPE (AlmanahCalendarButton, almanah_calendar_button, GTK_TYPE_TOGGLE_BUTTON)

static void
almanah_calendar_button_class_init (AlmanahCalendarButtonClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkToggleButtonClass *toggle_button_class = GTK_TOGGLE_BUTTON_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahCalendarButtonPrivate));

	gobject_class->get_property = almanah_calendar_button_get_property;
	gobject_class->set_property = almanah_calendar_button_set_property;
	gobject_class->finalize = almanah_calendar_button_finalize;

	toggle_button_class->toggled = almanah_calendar_button_toggled;

	g_object_class_install_property (gobject_class, PROP_STORAGE_MANAGER,
	                                 g_param_spec_object ("storage-manager",
	                                                      "Storage manager", "The storage manager whose entries should be listed.",
	                                                      ALMANAH_TYPE_STORAGE_MANAGER,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * AlmanahCalendarButton::day-selected:
	 * @calendar_button: the object which received the signal.
	 *
	 * Emitted when the user selects a day in the dock window.
	 */
	calendar_button_signals[DAY_SELECTED_SIGNAL] = g_signal_new ("day-selected",
								     G_OBJECT_CLASS_TYPE (gobject_class),
								     G_SIGNAL_RUN_FIRST,
								     G_STRUCT_OFFSET (AlmanahCalendarButtonClass, day_selected),
								     NULL, NULL,
								     NULL,
								     G_TYPE_NONE, 0);

	/**
	 * AlmanahCalendarButton::select-date-clicked:
	 * @calendar_button: the object which received the signal.
	 *
	 * Emitted when the user clicks the "select date" button in the dock window.
	 */
	calendar_button_signals[SELECT_DATE_CLICKED_SIGNAL] = g_signal_new ("select-date-clicked",
									    G_OBJECT_CLASS_TYPE (gobject_class),
									    G_SIGNAL_RUN_FIRST,
									    G_STRUCT_OFFSET (AlmanahCalendarButtonClass, select_date_clicked),
									    NULL, NULL,
									    NULL,
									    G_TYPE_NONE, 0);
}

static void
almanah_calendar_button_init (AlmanahCalendarButton *self)
{
	GtkWidget *arrow;
	GtkBox *main_box;
	GtkBuilder *builder;
	GError *error = NULL;
	const gchar *interface_filename = almanah_get_interface_filename ();
	const gchar *object_names[] = {
		"almanah_calendar_window",
		NULL
	};

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_CALENDAR_BUTTON, AlmanahCalendarButtonPrivate);
	self->priv->user_event = FIRST_EVENT;

	gtk_button_set_focus_on_click (GTK_BUTTON (self), TRUE);

	/* The button elements */
	self->priv->label = gtk_label_new (NULL);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);

	main_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6));
	gtk_box_pack_start (main_box, self->priv->label, TRUE, TRUE, 0);
	gtk_box_pack_start (main_box, arrow, FALSE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (main_box));

	/* Calendar dock window from the UI file */
	builder = gtk_builder_new ();
	if (gtk_builder_add_objects_from_file (builder, interface_filename, (gchar **) object_names, &error) == FALSE) {
		g_warning (_("UI file \"%s\" could not be loaded: %s"), interface_filename, error->message);
		g_error_free (error);
		g_object_unref (builder);

		return;
	}

	gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	self->priv->dock = GTK_WIDGET (gtk_builder_get_object (builder, "almanah_calendar_window"));
	if (self->priv->dock == NULL) {
		g_warning (_("Can't load calendar window object from UI file"));
		g_object_unref (builder);

		return;
	}
	gtk_window_set_type_hint (GTK_WINDOW (self->priv->dock), GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU);

	g_signal_connect (self->priv->dock, "hide", G_CALLBACK (almanah_calendar_button_dock_hiden), self);

	/* The calendar widget */
	self->priv->calendar = ALMANAH_CALENDAR (gtk_builder_get_object (builder, "almanah_cw_calendar"));
	g_object_ref (self->priv->calendar);
	g_signal_connect (self->priv->calendar, "day-selected", G_CALLBACK (almanah_calendar_button_day_selected_cb), self);
	g_signal_connect (self->priv->calendar, "month_changed", G_CALLBACK (almanah_calendar_button_month_changed_cb), self);

	/* Today button */
	self->priv->today_button = GTK_WIDGET (gtk_builder_get_object (builder, "almanah_cw_today_button"));
	g_signal_connect (self->priv->today_button, "clicked", G_CALLBACK (almanah_calendar_button_today_clicked_cb), self);
	g_signal_connect (self->priv->today_button, "button-press-event", G_CALLBACK (almanah_calendar_button_today_press_cb), self);

	/* Select a day button */
	/* @TODO: No the button press event, instead the 'activate' action funcion (if not, the select day window dosn't showed... */
	self->priv->select_date_button = GTK_WIDGET (gtk_builder_get_object (builder, "almanah_cw_select_date_button"));
	g_signal_connect (self->priv->select_date_button, "clicked", G_CALLBACK (almanah_calendar_button_select_date_clicked_cb), self);
	g_signal_connect (self->priv->select_date_button, "button-press-event", G_CALLBACK (almanah_calendar_button_select_date_press_cb), self);

	g_object_unref (builder);
}

static void
almanah_calendar_button_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahCalendarButtonPrivate *priv = ALMANAH_CALENDAR_BUTTON (object)->priv;

	switch (property_id) {
		case PROP_STORAGE_MANAGER:
			g_value_set_object (value, priv->storage_manager);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_calendar_button_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahCalendarButton *self = ALMANAH_CALENDAR_BUTTON (object);

	switch (property_id) {
		case PROP_STORAGE_MANAGER:
			almanah_calendar_button_set_storage_manager (self, g_value_get_object (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_calendar_button_finalize (GObject *object)
{
	AlmanahCalendarButtonPrivate *priv = ALMANAH_CALENDAR_BUTTON (object)->priv;

	g_clear_object (&priv->calendar);
	g_clear_object (&priv->storage_manager);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_calendar_button_parent_class)->finalize (object);
}

/**
 * Calculate the window position
 */
static void
dock_position_func (AlmanahCalendarButton *self, gint *x, gint *y)
{
	GdkScreen *screen;
	GdkRectangle monitor;
	GtkAllocation allocation;
	GtkRequisition dock_req;
	gint new_x, new_y, monitor_num;
	AlmanahCalendarWindow *calendar_window = ALMANAH_CALENDAR_WINDOW (self->priv->dock);

	/* Get the screen and monitor geometry */
	screen = gtk_widget_get_screen (GTK_WIDGET (self));
	monitor_num = gdk_screen_get_monitor_at_window (screen, gtk_widget_get_window (GTK_WIDGET (self)));
	if (monitor_num < 0)
		monitor_num = 0;
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	/* Get the AlmanahCalendarButton position */
	gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);
	gdk_window_get_origin (gtk_widget_get_window (GTK_WIDGET (self)), &new_x, &new_y);
	/* The dock window starting position is over the calendar button widget */
	new_x += allocation.x;
	new_y += allocation.y;

	gtk_widget_get_preferred_size (GTK_WIDGET (calendar_window), &dock_req, NULL);
	if (new_x + dock_req.width > monitor.x + monitor.width) {
		/* Move the required pixels to the left if the dock don't showed complety
		 * in the screen
		 */
		new_x -= (new_x + dock_req.width) - (monitor.x + monitor.width);
	}

	if ((new_y + allocation.height + dock_req.height) <= monitor.y + monitor.height) {
		/*The dock window height isn't bigger than the monitor size */
		new_y += allocation.height;
	} else if (new_y - dock_req.height >= monitor.y) {
		/* If the dock window height can't showed complety in the monitor,
		 * and the dock height isn't to bigg to show on top the calendar button
		 * move it on top of the calendar button
		 */
		new_y -= dock_req.height;
	} else if (monitor.y + monitor.height - (new_y + allocation.height) > new_y) {
		/* in other case, we show under the calendar button if the space is enought */
		new_y += allocation.height;
	} else {
		/* we need to put the dock in somewhere... even the monitor is to small */
		new_y -= dock_req.height;
	}

	/* Put the dock window in the correct screen */
	gtk_window_set_screen (GTK_WINDOW (calendar_window), screen);

	*x = new_x;
	*y = new_y;
}

static void
almanah_calendar_button_dock_hiden (GtkWidget *dock, AlmanahCalendarButton *self)
{
	/* Reset the calendar user event and toggle off the button */
	ALMANAH_CALENDAR_BUTTON (self)->priv->user_event = NONE_EVENT;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self), FALSE);
}

static void
almanah_calendar_button_toggled (GtkToggleButton *togglebutton)
{
	gint x, y;
	AlmanahCalendarButton *self;

	self = ALMANAH_CALENDAR_BUTTON (togglebutton);
	if (gtk_toggle_button_get_active (togglebutton)) {
		/* Show the dock */
		dock_position_func (self, &x, &y);
		gtk_window_move (GTK_WINDOW (self->priv->dock), x, y);
		almanah_calendar_window_popup (ALMANAH_CALENDAR_WINDOW (self->priv->dock));
	}
}

static void
almanah_calendar_button_day_selected_cb (GtkCalendar *calendar, AlmanahCalendarButton *self)
{
	GDate calendar_date;
	gchar calendar_string[100];

	almanah_calendar_get_date (self->priv->calendar, &calendar_date);
	/* Translators: This is a strftime()-format string for the date displayed at the top of the main window. */
	g_date_strftime (calendar_string, sizeof (calendar_string), _("%A, %e %B %Y"), &calendar_date);
	gtk_label_set_text (GTK_LABEL (self->priv->label), calendar_string);
	if (self->priv->user_event < DAY_EVENT) {
		/* Only hide the dock window when the user has clicked in a calendar day */
		self->priv->user_event = DAY_EVENT;
		almanah_calendar_window_popdown (ALMANAH_CALENDAR_WINDOW (self->priv->dock));
	}

	self->priv->user_event = NONE_EVENT;

	/* Emmits the signal at the end */
	g_signal_emit (self, calendar_button_signals[DAY_SELECTED_SIGNAL], 0);
}

static void
almanah_calendar_button_month_changed_cb (GtkCalendar *calendar, AlmanahCalendarButton *self)
{
	if (self->priv->user_event != TODAY_EVENT) {
		/* Save the month changed event just if the user hasn't click the today button
		 * beacuse the dock window should not hide in this case */
		self->priv->user_event = MONTH_EVENT;
	}
}

static gboolean
almanah_calendar_button_today_press_cb (GtkWidget *widget, GdkEvent *event, AlmanahCalendarButton *self)
{
	/* Save this event to not hide the dock window */
	self->priv->user_event = TODAY_EVENT;

	return FALSE;
}

static void
almanah_calendar_button_today_clicked_cb (__attribute__ ((unused)) GtkButton *button, gpointer user_data)
{
	AlmanahCalendarButton *self = ALMANAH_CALENDAR_BUTTON (user_data);

	almanah_calendar_button_select_today (self);
}

static gboolean
almanah_calendar_button_select_date_press_cb (GtkWidget *widget, GdkEvent *event, AlmanahCalendarButton *self)
{
	self->priv->user_event = NONE_EVENT;

	return FALSE;
}

static void
almanah_calendar_button_select_date_clicked_cb (__attribute__ ((unused)) GtkButton *button, gpointer user_data)
{
	AlmanahCalendarButton *self = ALMANAH_CALENDAR_BUTTON (user_data);

	g_signal_emit (self, calendar_button_signals[SELECT_DATE_CLICKED_SIGNAL], 0);
}

GtkWidget *
almanah_calendar_button_new (AlmanahStorageManager *storage_manager)
{
	g_return_val_if_fail (ALMANAH_IS_STORAGE_MANAGER (storage_manager), NULL);
	return GTK_WIDGET (g_object_new (ALMANAH_TYPE_CALENDAR_BUTTON, "storage-manager", storage_manager, NULL));
}

void
almanah_calendar_button_set_storage_manager (AlmanahCalendarButton *self, AlmanahStorageManager *storage_manager)
{
	g_return_if_fail (ALMANAH_IS_CALENDAR_BUTTON (self));
	g_return_if_fail (ALMANAH_IS_STORAGE_MANAGER (storage_manager));

	g_clear_object (&self->priv->storage_manager);
	self->priv->storage_manager = storage_manager;
	g_object_ref (self->priv->storage_manager);

	if (self->priv->calendar != NULL && ALMANAH_IS_CALENDAR (self->priv->calendar)) {
		almanah_calendar_set_storage_manager (self->priv->calendar, self->priv->storage_manager);
	}
}

void
almanah_calendar_button_select_date (AlmanahCalendarButton *self, GDate *date)
{
	g_return_if_fail (ALMANAH_IS_CALENDAR_BUTTON (self));
	g_return_if_fail (date != NULL);

	almanah_calendar_select_date (self->priv->calendar, date);
}

void
almanah_calendar_button_get_date (AlmanahCalendarButton *self, GDate *date)
{
	g_return_if_fail (ALMANAH_IS_CALENDAR_BUTTON (self));
	g_return_if_fail (date != NULL);

	almanah_calendar_get_date (self->priv->calendar, date);
}

void
almanah_calendar_button_popdown (AlmanahCalendarButton *self)
{
	g_return_if_fail (ALMANAH_IS_CALENDAR_BUTTON (self));

	almanah_calendar_window_popdown (ALMANAH_CALENDAR_WINDOW (self->priv->dock));
}

void
almanah_calendar_button_select_today (AlmanahCalendarButton *self)
{
	GDate current_date;

	g_return_if_fail (ALMANAH_IS_CALENDAR_BUTTON (self));

	g_date_set_time_t (&current_date, time (NULL));
	almanah_calendar_button_select_date (self, &current_date);
}
