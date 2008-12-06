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

#ifndef ALMANAH_CALENDAR_APPOINTMENT_LINK_H
#define ALMANAH_CALENDAR_APPOINTMENT_LINK_H

#include <glib.h>
#include <glib-object.h>

#include "link.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_CALENDAR_APPOINTMENT_LINK		(almanah_calendar_appointment_link_get_type ())
#define ALMANAH_CALENDAR_APPOINTMENT_LINK(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_CALENDAR_APPOINTMENT_LINK, AlmanahCalendarAppointmentLink))
#define ALMANAH_CALENDAR_APPOINTMENT_LINK_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_CALENDAR_APPOINTMENT_LINK, AlmanahCalendarAppointmentLinkClass))
#define ALMANAH_IS_CALENDAR_APPOINTMENT_LINK(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_CALENDAR_APPOINTMENT_LINK))
#define ALMANAH_IS_CALENDAR_APPOINTMENT_LINK_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_CALENDAR_APPOINTMENT_LINK))
#define ALMANAH_CALENDAR_APPOINTMENT_LINK_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_CALENDAR_APPOINTMENT_LINK, AlmanahCalendarAppointmentLinkClass))

typedef struct _AlmanahCalendarAppointmentLinkPrivate	AlmanahCalendarAppointmentLinkPrivate;

typedef struct {
	AlmanahLink parent;
	AlmanahCalendarAppointmentLinkPrivate *priv;
} AlmanahCalendarAppointmentLink;

typedef struct {
	AlmanahLinkClass parent;
} AlmanahCalendarAppointmentLinkClass;

GType almanah_calendar_appointment_link_get_type (void);
AlmanahCalendarAppointmentLink *almanah_calendar_appointment_link_new (const gchar *summary, GTime start_time);

G_END_DECLS

#endif /* !ALMANAH_CALENDAR_APPOINTMENT_LINK_H */
