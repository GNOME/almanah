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

typedef struct {
	GtkWidget *dock;
	guchar user_event;
	AlmanahCalendar *calendar;
	GtkWidget *today_button;
	GtkWidget *select_date_button;
	AlmanahStorageManager *storage_manager;
} AlmanahCalendarButtonPrivate;

static void almanah_calendar_button_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_calendar_button_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void almanah_calendar_button_finalize (GObject *object);

static void almanah_calendar_button_dock_closed (GtkWidget *dock, AlmanahCalendarButton *self);

static void almanah_calendar_button_toggled (GtkToggleButton *togglebutton);
static void almanah_calendar_button_day_selected_cb (GtkCalendar *calendar, AlmanahCalendarButton *self);
static void almanah_calendar_button_month_changed_cb (GtkCalendar *calendar, AlmanahCalendarButton *self);
static gboolean almanah_calendar_button_today_press_cb         (GtkWidget *widget, GdkEvent *event, AlmanahCalendarButton *self);
static void     almanah_calendar_button_today_clicked_cb       (GtkButton *button, gpointer user_data);
static void     almanah_calendar_button_select_date_clicked_cb (GtkButton *button, gpointer user_data);
static gboolean almanah_calendar_button_select_date_press_cb   (GtkWidget *widget, GdkEvent *event, AlmanahCalendarButton *self);

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahCalendarButton, almanah_calendar_button, GTK_TYPE_TOGGLE_BUTTON)

static void
almanah_calendar_button_class_init (AlmanahCalendarButtonClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkToggleButtonClass *toggle_button_class = GTK_TOGGLE_BUTTON_CLASS (klass);

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
	GtkBuilder *builder;
	GError *error = NULL;
	const gchar *object_names[] = {
		"almanah_calendar_window",
		NULL
	};

	AlmanahCalendarButtonPrivate *priv = almanah_calendar_button_get_instance_private (self);
	priv->user_event = FIRST_EVENT;

	gtk_widget_set_focus_on_click (GTK_WIDGET (self), TRUE);

	/* Calendar dock window from the UI file */
	builder = gtk_builder_new ();
	if (gtk_builder_add_objects_from_resource (builder, "/org/gnome/Almanah/ui/almanah.ui", (gchar **) object_names, &error) == 0) {
		g_warning (_("UI data could not be loaded: %s"), error->message);
		g_error_free (error);
		g_object_unref (builder);

		return;
	}

	gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	priv->dock = GTK_WIDGET (gtk_builder_get_object (builder, "almanah_calendar_window"));
	if (priv->dock == NULL) {
		g_warning (_("Can't load calendar window object from UI file"));
		g_object_unref (builder);

		return;
	}
	gtk_popover_set_relative_to (GTK_POPOVER (priv->dock), GTK_WIDGET (self));

	g_signal_connect (priv->dock, "hide", G_CALLBACK (almanah_calendar_button_dock_closed), self);

	/* The calendar widget */
	priv->calendar = ALMANAH_CALENDAR (gtk_builder_get_object (builder, "almanah_cw_calendar"));
	g_object_ref (priv->calendar);
	g_signal_connect (priv->calendar, "day-selected", G_CALLBACK (almanah_calendar_button_day_selected_cb), self);
	g_signal_connect (priv->calendar, "month_changed", G_CALLBACK (almanah_calendar_button_month_changed_cb), self);

	/* Today button */
	priv->today_button = GTK_WIDGET (gtk_builder_get_object (builder, "almanah_cw_today_button"));
	g_signal_connect (priv->today_button, "clicked", G_CALLBACK (almanah_calendar_button_today_clicked_cb), self);
	g_signal_connect (priv->today_button, "button-press-event", G_CALLBACK (almanah_calendar_button_today_press_cb), self);

	/* Select a day button */
	/* @TODO: No the button press event, instead the 'activate' action funcion (if not, the select day window dosn't showed... */
	priv->select_date_button = GTK_WIDGET (gtk_builder_get_object (builder, "almanah_cw_select_date_button"));
	g_signal_connect (priv->select_date_button, "clicked", G_CALLBACK (almanah_calendar_button_select_date_clicked_cb), self);
	g_signal_connect (priv->select_date_button, "button-press-event", G_CALLBACK (almanah_calendar_button_select_date_press_cb), self);

	g_object_unref (builder);
}

static void
almanah_calendar_button_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahCalendarButtonPrivate *priv = almanah_calendar_button_get_instance_private (ALMANAH_CALENDAR_BUTTON (object));

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
	AlmanahCalendarButtonPrivate *priv = almanah_calendar_button_get_instance_private (ALMANAH_CALENDAR_BUTTON (object));

	g_clear_object (&priv->calendar);
	g_clear_object (&priv->storage_manager);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_calendar_button_parent_class)->finalize (object);
}

static void
almanah_calendar_button_dock_closed (GtkWidget *dock, AlmanahCalendarButton *self)
{
	AlmanahCalendarButtonPrivate *priv = almanah_calendar_button_get_instance_private (ALMANAH_CALENDAR_BUTTON (self));

	/* Reset the calendar user event and toggle off the button */
	priv->user_event = NONE_EVENT;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self), FALSE);
}

static void
almanah_calendar_button_toggled (GtkToggleButton *togglebutton)
{
	AlmanahCalendarButton *self = ALMANAH_CALENDAR_BUTTON (togglebutton);
	AlmanahCalendarButtonPrivate *priv = almanah_calendar_button_get_instance_private (self);

	if (gtk_toggle_button_get_active (togglebutton)) {
		/* Show the dock */
		gtk_widget_show_all (GTK_WIDGET (priv->dock));
	}
}

static void
almanah_calendar_button_day_selected_cb (GtkCalendar *calendar, AlmanahCalendarButton *self)
{
	AlmanahCalendarButtonPrivate *priv = almanah_calendar_button_get_instance_private (self);

	if (priv->user_event < DAY_EVENT) {
		/* Only hide the dock window when the user has clicked in a calendar day */
		priv->user_event = DAY_EVENT;
		gtk_widget_hide (GTK_WIDGET (priv->dock));
	}

	priv->user_event = NONE_EVENT;

	/* Emmits the signal at the end */
	g_signal_emit (self, calendar_button_signals[DAY_SELECTED_SIGNAL], 0);
}

static void
almanah_calendar_button_month_changed_cb (GtkCalendar *calendar, AlmanahCalendarButton *self)
{
	AlmanahCalendarButtonPrivate *priv = almanah_calendar_button_get_instance_private (self);

	if (priv->user_event != TODAY_EVENT) {
		/* Save the month changed event just if the user hasn't click the today button
		 * beacuse the dock window should not hide in this case */
		priv->user_event = MONTH_EVENT;
	}
}

static gboolean
almanah_calendar_button_today_press_cb (GtkWidget *widget, GdkEvent *event, AlmanahCalendarButton *self)
{
	AlmanahCalendarButtonPrivate *priv = almanah_calendar_button_get_instance_private (self);

	/* Save this event to not hide the dock window */
	priv->user_event = TODAY_EVENT;

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
	AlmanahCalendarButtonPrivate *priv = almanah_calendar_button_get_instance_private (self);

	priv->user_event = NONE_EVENT;

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

	AlmanahCalendarButtonPrivate *priv = almanah_calendar_button_get_instance_private (self);

	g_clear_object (&priv->storage_manager);
	priv->storage_manager = storage_manager;
	g_object_ref (priv->storage_manager);

	if (priv->calendar != NULL && ALMANAH_IS_CALENDAR (priv->calendar)) {
		almanah_calendar_set_storage_manager (priv->calendar, priv->storage_manager);
	}
}

void
almanah_calendar_button_select_date (AlmanahCalendarButton *self, GDate *date)
{
	g_return_if_fail (ALMANAH_IS_CALENDAR_BUTTON (self));
	g_return_if_fail (date != NULL);

	AlmanahCalendarButtonPrivate *priv = almanah_calendar_button_get_instance_private (self);

	almanah_calendar_select_date (priv->calendar, date);
}

void
almanah_calendar_button_get_date (AlmanahCalendarButton *self, GDate *date)
{
	g_return_if_fail (ALMANAH_IS_CALENDAR_BUTTON (self));
	g_return_if_fail (date != NULL);

	AlmanahCalendarButtonPrivate *priv = almanah_calendar_button_get_instance_private (self);

	almanah_calendar_get_date (priv->calendar, date);
}

void
almanah_calendar_button_popdown (AlmanahCalendarButton *self)
{
	g_return_if_fail (ALMANAH_IS_CALENDAR_BUTTON (self));

	AlmanahCalendarButtonPrivate *priv = almanah_calendar_button_get_instance_private (self);

	gtk_widget_hide (GTK_WIDGET (priv->dock));
}

void
almanah_calendar_button_select_today (AlmanahCalendarButton *self)
{
	GDate current_date;

	g_return_if_fail (ALMANAH_IS_CALENDAR_BUTTON (self));

	g_date_set_time_t (&current_date, time (NULL));
	almanah_calendar_button_select_date (self, &current_date);
}
