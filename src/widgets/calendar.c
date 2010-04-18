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
#include "main.h"

static void almanah_calendar_finalize (GObject *object);
static void almanah_calendar_month_changed (GtkCalendar *calendar);
static gchar *almanah_calendar_detail_func (GtkCalendar *calendar, guint year, guint month, guint day, gpointer user_data);

struct _AlmanahCalendarPrivate {
	gboolean *important_days;
};

G_DEFINE_TYPE (AlmanahCalendar, almanah_calendar, GTK_TYPE_CALENDAR)

static void
almanah_calendar_class_init (AlmanahCalendarClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkCalendarClass *calendar_class = GTK_CALENDAR_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahCalendarPrivate));

	gobject_class->finalize = almanah_calendar_finalize;

	calendar_class->month_changed = almanah_calendar_month_changed;
}

static void
almanah_calendar_init (AlmanahCalendar *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_CALENDAR, AlmanahCalendarPrivate);
	gtk_calendar_set_detail_func (GTK_CALENDAR (self), almanah_calendar_detail_func, NULL, NULL);
}

static void
almanah_calendar_finalize (GObject *object)
{
	AlmanahCalendarPrivate *priv = ALMANAH_CALENDAR (object)->priv;

	g_free (priv->important_days);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_calendar_parent_class)->finalize (object);
}

static void
almanah_calendar_month_changed (GtkCalendar *calendar)
{
	AlmanahCalendarPrivate *priv = ALMANAH_CALENDAR (calendar)->priv;
	guint i, year, month, num_days;
	gboolean *days;

	/* Mark the days on the calendar which have diary entries */
	gtk_calendar_get_date (calendar, &year, &month, NULL);
	month++;
	days = almanah_storage_manager_get_month_marked_days (almanah->storage_manager, year, month, &num_days);

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
	priv->important_days = almanah_storage_manager_get_month_important_days (almanah->storage_manager, year, month, &num_days);
}

static gchar *
almanah_calendar_detail_func (GtkCalendar *calendar, guint year, guint month, guint day, gpointer user_data)
{
	AlmanahCalendarPrivate *priv = ALMANAH_CALENDAR (calendar)->priv;
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

GtkWidget *
almanah_calendar_new (void)
{
	return g_object_new (ALMANAH_TYPE_CALENDAR, NULL);
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
