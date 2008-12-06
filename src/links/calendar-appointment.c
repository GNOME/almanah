/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2008 <philip@tecnocode.co.uk>
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

#include "link.h"
#include "calendar-appointment.h"

static void almanah_calendar_appointment_link_init (AlmanahCalendarAppointmentLink *self);
static void almanah_calendar_appointment_link_finalize (GObject *object);
static const gchar *almanah_calendar_appointment_link_format_value (AlmanahLink *link);
static gboolean almanah_calendar_appointment_link_view (AlmanahLink *link);

struct _AlmanahCalendarAppointmentLinkPrivate {
	gchar *uid;
	gchar *summary;
};

G_DEFINE_TYPE (AlmanahCalendarAppointmentLink, almanah_calendar_appointment_link, ALMANAH_TYPE_LINK)
#define ALMANAH_CALENDAR_APPOINTMENT_LINK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_CALENDAR_APPOINTMENT_LINK, AlmanahCalendarAppointmentLinkPrivate))

static void
almanah_calendar_appointment_link_class_init (AlmanahCalendarAppointmentLinkClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AlmanahLinkClass *link_class = ALMANAH_LINK_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahCalendarAppointmentLinkPrivate));

	gobject_class->finalize = almanah_calendar_appointment_link_finalize;

	link_class->name = _("Calendar Appointment");
	link_class->description = _("An appointment on an Evolution calendar.");
	link_class->icon_name = "appointment-new";

	link_class->format_value = almanah_calendar_appointment_link_format_value;
	link_class->view = almanah_calendar_appointment_link_view;
}

static void
almanah_calendar_appointment_link_init (AlmanahCalendarAppointmentLink *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_CALENDAR_APPOINTMENT_LINK, AlmanahCalendarAppointmentLinkPrivate);
}

static void
almanah_calendar_appointment_link_finalize (GObject *object)
{
	AlmanahCalendarAppointmentLinkPrivate *priv = ALMANAH_CALENDAR_APPOINTMENT_LINK_GET_PRIVATE (object);

	g_free (priv->uid);
	g_free (priv->summary);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_calendar_appointment_link_parent_class)->finalize (object);
}

AlmanahCalendarAppointmentLink *
almanah_calendar_appointment_link_new (const gchar *uid, const gchar *summary)
{
	AlmanahCalendarAppointmentLink *link = g_object_new (ALMANAH_TYPE_CALENDAR_APPOINTMENT_LINK, NULL);
	link->priv->uid = g_strdup (uid);
	link->priv->summary = g_strdup (summary);
	return link;
}

static const gchar *
almanah_calendar_appointment_link_format_value (AlmanahLink *link)
{
	/* TODO */
	return ALMANAH_CALENDAR_APPOINTMENT_LINK (link)->priv->summary;
}

static gboolean
almanah_calendar_appointment_link_view (AlmanahLink *link)
{
	/* TODO */
	return TRUE;
}
