/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2010 <philip@tecnocode.co.uk>
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

#include <glib.h>
#include <glib/gi18n.h>

#include "calendar.h"
#include "storage-manager.h"

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void dispose (GObject *object);
static void almanah_calendar_finalize (GObject *object);
static void almanah_calendar_month_changed (GtkCalendar *calendar);
static gchar *almanah_calendar_detail_func (GtkCalendar *calendar, guint year, guint month, guint day, gpointer user_data);
static void entry_added_cb (AlmanahStorageManager *storage_manager, AlmanahEntry *entry, AlmanahCalendar *calendar);
static void entry_removed_cb (AlmanahStorageManager *storage_manager, GDate *date, AlmanahCalendar *calendar);

typedef struct {
	AlmanahStorageManager *storage_manager;
	gulong entry_added_signal;
	gulong entry_removed_signal;

	gboolean *important_days;
} AlmanahCalendarPrivate;

struct _AlmanahCalendar {
	GtkCalendar parent;
};

enum {
	PROP_STORAGE_MANAGER = 1,
};

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahCalendar, almanah_calendar, GTK_TYPE_CALENDAR)

static void
almanah_calendar_class_init (AlmanahCalendarClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkCalendarClass *calendar_class = GTK_CALENDAR_CLASS (klass);

	gobject_class->get_property = get_property;
	gobject_class->set_property = set_property;
	gobject_class->dispose = dispose;
	gobject_class->finalize = almanah_calendar_finalize;

	calendar_class->month_changed = almanah_calendar_month_changed;

	g_object_class_install_property (gobject_class, PROP_STORAGE_MANAGER,
	                                 g_param_spec_object ("storage-manager",
	                                                      "Storage manager", "The storage manager whose entries should be listed.",
	                                                      ALMANAH_TYPE_STORAGE_MANAGER,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_calendar_init (AlmanahCalendar *self)
{
	gtk_calendar_set_detail_func (GTK_CALENDAR (self), almanah_calendar_detail_func, NULL, NULL);
}

static void
dispose (GObject *object)
{
	AlmanahCalendarPrivate *priv = almanah_calendar_get_instance_private (ALMANAH_CALENDAR (object));

	if (priv->storage_manager != NULL)
		g_object_unref (priv->storage_manager);
	priv->storage_manager = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_calendar_parent_class)->dispose (object);
}

static void
almanah_calendar_finalize (GObject *object)
{
	AlmanahCalendarPrivate *priv = almanah_calendar_get_instance_private (ALMANAH_CALENDAR (object));

	g_free (priv->important_days);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_calendar_parent_class)->finalize (object);
}

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahCalendarPrivate *priv = almanah_calendar_get_instance_private (ALMANAH_CALENDAR (object));

	switch (property_id) {
		case PROP_STORAGE_MANAGER:
			g_value_set_object (value, priv->storage_manager);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahCalendar *self = ALMANAH_CALENDAR (object);

	switch (property_id) {
		case PROP_STORAGE_MANAGER:
			almanah_calendar_set_storage_manager (self, g_value_get_object (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_calendar_month_changed (GtkCalendar *calendar)
{
	AlmanahCalendarPrivate *priv = almanah_calendar_get_instance_private (ALMANAH_CALENDAR (calendar));
	guint i, year, month, num_days;
	gboolean *days;

	/* Mark the days on the calendar which have diary entries */
	gtk_calendar_get_date (calendar, &year, &month, NULL);
	month++;
	days = almanah_storage_manager_get_month_marked_days (priv->storage_manager, year, month, &num_days);

	gtk_calendar_clear_marks (calendar);
	for (i = 0; i < num_days; i++) {
		if (days[i] == TRUE)
			gtk_calendar_mark_day (calendar, i + 1);
		else
			gtk_calendar_unmark_day (calendar, i + 1);
	}

	g_free (days);

	/* Cache the days which are important, so that the detail function isn't hideously slow */
	g_free (priv->important_days);
	priv->important_days = almanah_storage_manager_get_month_important_days (priv->storage_manager, year, month, &num_days);
}

static gchar *
almanah_calendar_detail_func (GtkCalendar *calendar, guint year, guint month, guint day, gpointer user_data)
{
	AlmanahCalendarPrivate *priv = almanah_calendar_get_instance_private (ALMANAH_CALENDAR (calendar));
	guint calendar_year, calendar_month;

	gtk_calendar_get_date (calendar, &calendar_year, &calendar_month, NULL);

	/* Check we actually have the data available for the requested month */
	if (priv->important_days == NULL || year != calendar_year || month != calendar_month)
		return NULL;

	/* Display markup if the day is important; don't otherwise */
	if (priv->important_days[day - 1] == TRUE) {
		/* Translators: This is the detail string for important days as displayed in the calendar. */
		return g_strdup (_("Important!"));
	}
	return NULL;
}

static void
entry_added_cb (AlmanahStorageManager *storage_manager, AlmanahEntry *entry, AlmanahCalendar *calendar)
{
	GDate date;
	guint month;

	almanah_entry_get_date (entry, &date);
	gtk_calendar_get_date (GTK_CALENDAR (calendar), NULL, &month, NULL);

	if (g_date_get_month (&date) == month + 1) {
		/* Mark the entry on the calendar, since it's guaranteed to be non-empty */
		gtk_calendar_mark_day (GTK_CALENDAR (calendar), g_date_get_day (&date));
	}
}

static void
entry_removed_cb (AlmanahStorageManager *storage_manager, GDate *date, AlmanahCalendar *calendar)
{
	guint month;

	gtk_calendar_get_date (GTK_CALENDAR (calendar), NULL, &month, NULL);

	if (g_date_get_month (date) == month + 1) {
		/* Unmark the entry on the calendar */
		gtk_calendar_unmark_day (GTK_CALENDAR (calendar), g_date_get_day (date));
	}
}

GtkWidget *
almanah_calendar_new (AlmanahStorageManager *storage_manager)
{
	g_return_val_if_fail (ALMANAH_IS_STORAGE_MANAGER (storage_manager), NULL);
	return g_object_new (ALMANAH_TYPE_CALENDAR, "storage-manager", storage_manager, NULL);
}

AlmanahStorageManager *
almanah_calendar_get_storage_manager (AlmanahCalendar *self)
{
	g_return_val_if_fail (ALMANAH_IS_CALENDAR (self), NULL);

	AlmanahCalendarPrivate *priv = almanah_calendar_get_instance_private (self);

	return priv->storage_manager;
}

void
almanah_calendar_set_storage_manager (AlmanahCalendar *self, AlmanahStorageManager *storage_manager)
{
	AlmanahCalendarPrivate *priv = almanah_calendar_get_instance_private (self);

	g_return_if_fail (ALMANAH_IS_CALENDAR (self));
	g_return_if_fail (storage_manager == NULL || ALMANAH_IS_STORAGE_MANAGER (storage_manager));

	if (priv->storage_manager != NULL) {
		g_signal_handler_disconnect (priv->storage_manager, priv->entry_added_signal);
		g_signal_handler_disconnect (priv->storage_manager, priv->entry_removed_signal);

		g_object_unref (priv->storage_manager);
	}

	priv->storage_manager = storage_manager;

	if (priv->storage_manager != NULL) {
		g_object_ref (priv->storage_manager);

		/* Connect to signals from the storage manager so we can mark/unmark days as appropriate */
		priv->entry_added_signal = g_signal_connect (priv->storage_manager, "entry-added", (GCallback) entry_added_cb, self);
		priv->entry_removed_signal = g_signal_connect (priv->storage_manager, "entry-removed", (GCallback) entry_removed_cb, self);
	}
}

void
almanah_calendar_select_date (AlmanahCalendar *self, GDate *date)
{
	g_return_if_fail (ALMANAH_IS_CALENDAR (self));
	g_return_if_fail (date != NULL);

	gtk_calendar_select_month (GTK_CALENDAR (self), g_date_get_month (date) - 1, g_date_get_year (date));
	gtk_calendar_select_day (GTK_CALENDAR (self), g_date_get_day (date));
}

void
almanah_calendar_select_today (AlmanahCalendar *self)
{
	GDate current_date;

	g_return_if_fail (ALMANAH_IS_CALENDAR (self));

	g_date_set_time_t (&current_date, time (NULL));
	almanah_calendar_select_date (self, &current_date);
}

void
almanah_calendar_get_date (AlmanahCalendar *self, GDate *date)
{
	guint year, month, day;

	g_return_if_fail (ALMANAH_IS_CALENDAR (self));
	g_return_if_fail (date != NULL);

	gtk_calendar_get_date (GTK_CALENDAR (self), &year, &month, &day);
	g_date_set_dmy (date, day, month + 1, year);
}
