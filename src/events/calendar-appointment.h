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

#ifndef ALMANAH_CALENDAR_APPOINTMENT_EVENT_H
#define ALMANAH_CALENDAR_APPOINTMENT_EVENT_H

#include <glib.h>
#include <glib-object.h>

#include "event.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_CALENDAR_APPOINTMENT_EVENT     (almanah_calendar_appointment_event_get_type ())

G_DECLARE_FINAL_TYPE (AlmanahCalendarAppointmentEvent, almanah_calendar_appointment_event, ALMANAH, CALENDAR_APPOINTMENT_EVENT, AlmanahEvent)

AlmanahCalendarAppointmentEvent *almanah_calendar_appointment_event_new (const gchar *summary, time_t start_time);

G_END_DECLS

#endif /* !ALMANAH_CALENDAR_APPOINTMENT_EVENT_H */
