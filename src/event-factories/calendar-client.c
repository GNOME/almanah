/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2-*- */
/*
 * Copyright (C) 2004 Free Software Foundation, Inc.
 * Copyright (C) 2015 Álvaro Peña <alvaropg@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Mark McLoughlin  <mark@skynet.ie>
 *     William Jon McCann  <mccann@jhu.edu>
 *     Martin Grimme  <martin@pycage.de>
 *     Christian Kellner  <gicmo@xatom.net>
 *
 * Modified by:
 *     Álvaro Peña <alvaropg@gmail.com>
 */

#include <config.h>

#include "calendar-client.h"

#include <libintl.h>
#include <string.h>
#define HANDLE_LIBICAL_MEMORY
#include <libecal/libecal.h>

#include "calendar-sources.h"

#undef CALENDAR_ENABLE_DEBUG
#include "calendar-debug.h"

#ifndef _
#define _(x) gettext(x)
#endif

#ifndef N_
#define N_(x) x
#endif

#define CALENDAR_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CALENDAR_TYPE_CLIENT, CalendarClientPrivate))

typedef struct _CalendarClientQuery  CalendarClientQuery;
typedef struct _CalendarClientSource CalendarClientSource;

struct _CalendarClientQuery
{
  ECalClientView *view;
  GHashTable *events;
};

struct _CalendarClientSource
{
  CalendarClient      *client;
  ECalClient          *cal_client;

  CalendarClientQuery  completed_query;
  CalendarClientQuery  in_progress_query;

  guint                changed_signal_id;

  guint                query_completed : 1;
  guint                query_in_progress : 1;
};

struct _CalendarClientPrivate
{
  CalendarSources     *calendar_sources;

  GSList              *appointment_sources;
  GSList              *task_sources;

  ICalTimezone        *zone;

  GFileMonitor        *tz_monitor;

  guint                day;
  guint                month;
  guint                year;
};

static void calendar_client_class_init   (CalendarClientClass *klass);
static void calendar_client_init         (CalendarClient      *client);
static void calendar_client_finalize     (GObject             *object);
static void calendar_client_set_property (GObject             *object,
                                          guint                prop_id,
                                          const GValue        *value,
                                          GParamSpec          *pspec);
static void calendar_client_get_property (GObject             *object,
                                          guint                prop_id,
                                          GValue              *value,
                                          GParamSpec          *pspec);

static GSList *calendar_client_update_sources_list         (CalendarClient       *client,
                                                            GSList               *sources,
                                                            GSList               *esources,
                                                            guint                 changed_signal_id);
static void    calendar_client_appointment_sources_changed (CalendarClient       *client);
static void    calendar_client_task_sources_changed        (CalendarClient       *client);

static void calendar_client_stop_query  (CalendarClient       *client,
                                         CalendarClientSource *source,
                                         CalendarClientQuery  *query);
static void calendar_client_start_query (CalendarClient       *client,
                                         CalendarClientSource *source,
                                         const char           *query);

static void calendar_client_source_finalize (CalendarClientSource *source);
static void calendar_client_query_finalize  (CalendarClientQuery  *query);

static void
calendar_client_update_appointments (CalendarClient *client);
static void
calendar_client_update_tasks (CalendarClient *client);

enum
  {
    PROP_O,
    PROP_DAY,
    PROP_MONTH,
    PROP_YEAR
  };

enum
  {
    APPOINTMENTS_CHANGED,
    TASKS_CHANGED,
    LAST_SIGNAL
  };

static GObjectClass *parent_class = NULL;
static guint         signals [LAST_SIGNAL] = { 0, };

GType
calendar_client_get_type (void)
{
  static GType client_type = 0;

  if (!client_type)
    {
      static const GTypeInfo client_info =
        {
          sizeof (CalendarClientClass),
          NULL,   /* base_init */
          NULL,   /* base_finalize */
          (GClassInitFunc) calendar_client_class_init,
          NULL,           /* class_finalize */
          NULL,   /* class_data */
          sizeof (CalendarClient),
          0,    /* n_preallocs */
          (GInstanceInitFunc) calendar_client_init,
        };

      client_type = g_type_register_static (G_TYPE_OBJECT,
                                            "CalendarClient",
                                            &client_info, 0);
    }

  return client_type;
}

static void
calendar_client_class_init (CalendarClientClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize     = calendar_client_finalize;
  gobject_class->set_property = calendar_client_set_property;
  gobject_class->get_property = calendar_client_get_property;

  g_type_class_add_private (klass, sizeof (CalendarClientPrivate));

  g_object_class_install_property (gobject_class,
                                   PROP_DAY,
                                   g_param_spec_uint ("day",
                                                      "Day",
                                                      "The currently monitored day between 1 and 31 (0 denotes unset)",
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_MONTH,
                                   g_param_spec_uint ("month",
                                                      "Month",
                                                      "The currently monitored month between 0 and 11",
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_YEAR,
                                   g_param_spec_uint ("year",
                                                      "Year",
                                                      "The currently monitored year",
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READWRITE));

  signals [APPOINTMENTS_CHANGED] =
    g_signal_new ("appointments-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (CalendarClientClass, tasks_changed),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  signals [TASKS_CHANGED] =
    g_signal_new ("tasks-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (CalendarClientClass, tasks_changed),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
}

static ICalTimezone *
calendar_client_config_get_icaltimezone (void)
{
  char         *location;
  ICalTimezone *zone = NULL;

  location = e_cal_util_get_system_timezone_location ();
  if (!location)
    return i_cal_timezone_get_utc_timezone ();

  zone = i_cal_timezone_get_builtin_timezone (location);
  g_free (location);

  return zone;
}

static void
calendar_client_set_timezone (CalendarClient *client)
{
  GSList *l;
  GSList *esources;

  client->priv->zone = calendar_client_config_get_icaltimezone ();

  esources = calendar_sources_get_appointment_sources (client->priv->calendar_sources);
  for (l = esources; l; l = l->next) {
    ECalClient *source = l->data;

    e_cal_client_set_default_timezone (source, client->priv->zone);
  }
}

static void
calendar_client_timezone_changed_cb (G_GNUC_UNUSED GFileMonitor      *monitor,
                                     G_GNUC_UNUSED GFile             *file,
                                     G_GNUC_UNUSED GFile             *other_file,
                                     G_GNUC_UNUSED GFileMonitorEvent *event,
                                     gpointer                         user_data)
{
  calendar_client_set_timezone (CALENDAR_CLIENT (user_data));
}

static void
load_calendars (CalendarClient    *client,
                CalendarEventType  type)
{
  GSList *l, *clients;

  switch (type)
    {
    case CALENDAR_EVENT_APPOINTMENT:
      clients = client->priv->appointment_sources;
      break;
    case CALENDAR_EVENT_TASK:
      clients = client->priv->task_sources;
      break;
    case CALENDAR_EVENT_ALL:
    default:
      g_assert_not_reached ();
    }

  for (l = clients; l != NULL; l = l->next)
    {
      CalendarClientSource *cl_source = l->data;

      if (type == CALENDAR_EVENT_APPOINTMENT)
        calendar_client_update_appointments (cl_source->client);
      else
        calendar_client_update_tasks (cl_source->client);
    }
}

static void
calendar_client_init (CalendarClient *client)
{
  GSList *esources;
  GFile *tz;

  client->priv = CALENDAR_CLIENT_GET_PRIVATE (client);

  client->priv->calendar_sources = calendar_sources_get ();

  esources = calendar_sources_get_appointment_sources (client->priv->calendar_sources);
  client->priv->appointment_sources =
    calendar_client_update_sources_list (client, NULL, esources, signals [APPOINTMENTS_CHANGED]);

  esources = calendar_sources_get_task_sources (client->priv->calendar_sources);
  client->priv->task_sources =
    calendar_client_update_sources_list (client, NULL, esources, signals [TASKS_CHANGED]);

  /* set the timezone before loading the clients */
  calendar_client_set_timezone (client);
  load_calendars (client, CALENDAR_EVENT_APPOINTMENT);
  load_calendars (client, CALENDAR_EVENT_TASK);

  g_signal_connect_swapped (client->priv->calendar_sources,
                            "appointment-sources-changed",
                            G_CALLBACK (calendar_client_appointment_sources_changed),
                            client);
  g_signal_connect_swapped (client->priv->calendar_sources,
                            "task-sources-changed",
                            G_CALLBACK (calendar_client_task_sources_changed),
                            client);

  tz = g_file_new_for_path ("/etc/localtime");
  client->priv->tz_monitor = g_file_monitor_file (tz, G_FILE_MONITOR_NONE, NULL, NULL);
  g_object_unref (tz);
  if (client->priv->tz_monitor == NULL)
    g_warning ("Can't monitor /etc/localtime for changes");
  else
    g_signal_connect (client->priv->tz_monitor, "changed", G_CALLBACK (calendar_client_timezone_changed_cb), client);

  client->priv->day   = G_MAXUINT;
  client->priv->month = G_MAXUINT;
  client->priv->year  = G_MAXUINT;
}

static void
calendar_client_finalize (GObject *object)
{
  CalendarClient *client = CALENDAR_CLIENT (object);
  GSList         *l;

  g_clear_object (&client->priv->tz_monitor);

  for (l = client->priv->appointment_sources; l; l = l->next)
    {
      calendar_client_source_finalize (l->data);
      g_free (l->data);
    }
  g_slist_free (client->priv->appointment_sources);
  client->priv->appointment_sources = NULL;

  for (l = client->priv->task_sources; l; l = l->next)
    {
      calendar_client_source_finalize (l->data);
      g_free (l->data);
    }
  g_slist_free (client->priv->task_sources);
  client->priv->task_sources = NULL;

  if (client->priv->calendar_sources)
    g_object_unref (client->priv->calendar_sources);
  client->priv->calendar_sources = NULL;

  if (G_OBJECT_CLASS (parent_class)->finalize)
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
calendar_client_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  CalendarClient *client = CALENDAR_CLIENT (object);

  switch (prop_id)
    {
    case PROP_DAY:
      calendar_client_select_day (client, g_value_get_uint (value));
      break;
    case PROP_MONTH:
      calendar_client_select_month (client,
                                    g_value_get_uint (value),
                                    client->priv->year);
      break;
    case PROP_YEAR:
      calendar_client_select_month (client,
                                    client->priv->month,
                                    g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
calendar_client_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  CalendarClient *client = CALENDAR_CLIENT (object);

  switch (prop_id)
    {
    case PROP_DAY:
      g_value_set_uint (value, client->priv->day);
      break;
    case PROP_MONTH:
      g_value_set_uint (value, client->priv->month);
      break;
    case PROP_YEAR:
      g_value_set_uint (value, client->priv->year);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

CalendarClient *
calendar_client_new (void)
{
  return g_object_new (CALENDAR_TYPE_CLIENT, NULL);
}

/* @day and @month can happily be out of range as
 * mktime() will normalize them correctly. From mktime(3):
 *
 * "If structure members are outside their legal interval,
 *  they will be normalized (so that, e.g., 40 October is
 *  changed into 9 November)."
 *
 * "What?", you say, "Something useful in libc?"
 */
static inline time_t
make_time_for_day_begin (int day,
                         int month,
                         int year)
{
  struct tm localtime_tm = { 0, };

  localtime_tm.tm_mday  = day;
  localtime_tm.tm_mon   = month;
  localtime_tm.tm_year  = year - 1900;
  localtime_tm.tm_isdst = -1;

  return mktime (&localtime_tm);
}

static inline char *
make_isodate_for_day_begin (int day,
                            int month,
                            int year)
{
  time_t utctime;

  utctime = make_time_for_day_begin (day, month, year);

  return utctime != -1 ? isodate_from_time_t (utctime) : NULL;
}

static time_t
get_time_from_property (ICalComponent      *ical,
                        ICalPropertyKind    prop_kind,
                        ICalTime *       (* get_prop_func) (ICalProperty *prop),
                        ICalTimezone       *default_zone)
{
  ICalProperty  *prop;
  ICalTime      *ical_time;
  ICalParameter *param;
  ICalTimezone  *time_zone = NULL;
  time_t         retval;

  prop = i_cal_component_get_first_property (ical, prop_kind);
  if (!prop)
    return 0;

  ical_time = get_prop_func (prop);

  param = i_cal_property_get_first_parameter (prop, I_CAL_TZID_PARAMETER);
  if (param)
    time_zone = i_cal_timezone_get_builtin_timezone_from_tzid (i_cal_parameter_get_tzid (param));
  else if (i_cal_time_is_utc (ical_time))
    time_zone = i_cal_timezone_get_utc_timezone ();
  else
    time_zone = default_zone;

  retval = i_cal_time_as_timet_with_zone (ical_time, time_zone);

  g_clear_object (&ical_time);
  g_clear_object (&param);
  g_clear_object (&prop);

  return retval;
}

static char *
get_ical_uid (ICalComponent *ical)
{
  return g_strdup (i_cal_component_get_uid (ical));
}

static char *
get_ical_rid (ICalComponent *ical)
{
  return e_cal_util_component_get_recurid_as_string (ical);
}

static char *
get_ical_summary (ICalComponent *ical)
{
  ICalProperty *prop;
  char *retval;

  prop = i_cal_component_get_first_property (ical, I_CAL_SUMMARY_PROPERTY);
  if (!prop)
    return NULL;

  retval = g_strdup (i_cal_property_get_summary (prop));

  g_object_unref (prop);

  return retval;
}

static char *
get_ical_description (ICalComponent *ical)
{
  ICalProperty *prop;
  char *retval;

  prop = i_cal_component_get_first_property (ical, I_CAL_DESCRIPTION_PROPERTY);
  if (!prop)
    return NULL;

  retval = g_strdup (i_cal_property_get_description (prop));

  g_object_unref (prop);

  return retval;
}

static inline time_t
get_ical_start_time (ICalComponent *ical,
                     ICalTimezone  *default_zone)
{
  return get_time_from_property (ical,
                                 I_CAL_DTSTART_PROPERTY,
                                 i_cal_property_get_dtstart,
                                 default_zone);
}

static inline time_t
get_ical_end_time (ICalComponent *ical,
                   ICalTimezone  *default_zone)
{
  return get_time_from_property (ical,
                                 I_CAL_DTEND_PROPERTY,
                                 i_cal_property_get_dtend,
                                 default_zone);
}

static gboolean
get_ical_is_all_day (ICalComponent *ical,
                     time_t         start_time,
                     ICalTimezone  *default_zone)
{
  ICalProperty *prop;
  struct tm    *start_tm;
  time_t        end_time;
  ICalDuration *duration;
  ICalTime     *start_icaltime;
  gboolean      retval;

  start_icaltime = i_cal_component_get_dtstart (ical);
  if (!start_icaltime)
    return FALSE;

  if (i_cal_time_is_date (start_icaltime))
    {
      g_object_unref (start_icaltime);
      return TRUE;
    }

  g_clear_object (&start_icaltime);

  start_tm = gmtime (&start_time);
  if (start_tm->tm_sec  != 0 ||
      start_tm->tm_min  != 0 ||
      start_tm->tm_hour != 0)
    return FALSE;

  if ((end_time = get_ical_end_time (ical, default_zone)))
    return (end_time - start_time) % 86400 == 0;

  prop = i_cal_component_get_first_property (ical, I_CAL_DURATION_PROPERTY);
  if (!prop)
    return FALSE;

  duration = i_cal_property_get_duration (prop);

  retval = i_cal_duration_as_int (duration) % 86400 == 0;

  g_object_unref (duration);
  g_object_unref (prop);

  return retval;
}

static inline time_t
get_ical_due_time (ICalComponent *ical,
                   ICalTimezone  *default_zone)
{
  return get_time_from_property (ical,
                                 I_CAL_DUE_PROPERTY,
                                 i_cal_property_get_due,
                                 default_zone);
}

static guint
get_ical_percent_complete (ICalComponent *ical)
{
  ICalProperty *prop;
  ICalPropertyStatus status;
  int           percent_complete;

  status = i_cal_component_get_status (ical);
  if (status == I_CAL_STATUS_COMPLETED)
    return 100;

  if (e_cal_util_component_has_property (ical, I_CAL_COMPLETED_PROPERTY))
    return 100;

  prop = i_cal_component_get_first_property (ical, I_CAL_PERCENTCOMPLETE_PROPERTY);
  if (!prop)
    return 0;

  percent_complete = i_cal_property_get_percentcomplete (prop);

  g_object_unref (prop);

  return CLAMP (percent_complete, 0, 100);
}

static inline time_t
get_ical_completed_time (ICalComponent *ical,
                         ICalTimezone  *default_zone)
{
  return get_time_from_property (ical,
                                 I_CAL_COMPLETED_PROPERTY,
                                 i_cal_property_get_completed,
                                 default_zone);
}

static int
get_ical_priority (ICalComponent *ical)
{
  ICalProperty *prop;
  int           retval;

  prop = i_cal_component_get_first_property (ical, I_CAL_PRIORITY_PROPERTY);
  if (!prop)
    return -1;

  retval = i_cal_property_get_priority (prop);

  g_object_unref (prop);

  return retval;
}

static char *
color_from_extension (ESourceSelectable *selectable)
{
  g_return_val_if_fail (selectable != NULL, NULL);

  return e_source_selectable_dup_color (selectable);
}

static char *
get_source_color (ECalClient *esource)
{
  ESource *source;

  g_return_val_if_fail (E_IS_CAL_CLIENT (esource), NULL);

  source = e_client_get_source (E_CLIENT (esource));

  if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
    return color_from_extension (e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR));
  if (e_source_has_extension (source, E_SOURCE_EXTENSION_MEMO_LIST))
    return color_from_extension (e_source_get_extension (source, E_SOURCE_EXTENSION_MEMO_LIST));
  if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
    return color_from_extension (e_source_get_extension (source, E_SOURCE_EXTENSION_TASK_LIST));
  return NULL;
}

static inline gboolean
calendar_appointment_equal (CalendarAppointment *a,
                            CalendarAppointment *b)
{
  GSList *la, *lb;

  if (g_slist_length (a->occurrences) != g_slist_length (b->occurrences))
    return FALSE;

  for (la = a->occurrences, lb = b->occurrences; la && lb; la = la->next, lb = lb->next)
    {
      CalendarOccurrence *oa = la->data;
      CalendarOccurrence *ob = lb->data;

      if (oa->start_time != ob->start_time ||
          oa->end_time   != ob->end_time)
        return FALSE;
    }

  return
    g_strcmp0 (a->uid,          b->uid)          == 0 &&
    g_strcmp0 (a->source_uid,   b->source_uid)   == 0 &&
    g_strcmp0 (a->summary,      b->summary)      == 0 &&
    g_strcmp0 (a->description,  b->description)  == 0 &&
    g_strcmp0 (a->color_string, b->color_string) == 0 &&
    a->start_time == b->start_time                    &&
    a->end_time   == b->end_time                      &&
    a->is_all_day == b->is_all_day;
}

static void
calendar_appointment_copy (CalendarAppointment *appointment,
                           CalendarAppointment *appointment_copy)
{
  GSList *l;

  g_assert (appointment != NULL);
  g_assert (appointment_copy != NULL);

  appointment_copy->occurrences = g_slist_copy (appointment->occurrences);
  for (l = appointment_copy->occurrences; l; l = l->next)
    {
      CalendarOccurrence *occurrence = l->data;
      CalendarOccurrence *occurrence_copy;

      occurrence_copy             = g_new0 (CalendarOccurrence, 1);
      occurrence_copy->start_time = occurrence->start_time;
      occurrence_copy->end_time   = occurrence->end_time;

      l->data = occurrence_copy;
    }

  appointment_copy->uid          = g_strdup (appointment->uid);
  appointment_copy->source_uid   = g_strdup (appointment->source_uid);
  appointment_copy->summary      = g_strdup (appointment->summary);
  appointment_copy->description  = g_strdup (appointment->description);
  appointment_copy->color_string = g_strdup (appointment->color_string);
  appointment_copy->start_time   = appointment->start_time;
  appointment_copy->end_time     = appointment->end_time;
  appointment_copy->is_all_day   = appointment->is_all_day;
}

static void
calendar_appointment_finalize (CalendarAppointment *appointment)
{
  GSList *l;

  for (l = appointment->occurrences; l; l = l->next)
    g_free (l->data);
  g_slist_free (appointment->occurrences);
  appointment->occurrences = NULL;

  g_free (appointment->uid);
  appointment->uid = NULL;

  g_free (appointment->rid);
  appointment->rid = NULL;

  g_free (appointment->source_uid);
  appointment->source_uid = NULL;

  g_free (appointment->summary);
  appointment->summary = NULL;

  g_free (appointment->description);
  appointment->description = NULL;

  g_free (appointment->color_string);
  appointment->color_string = NULL;

  appointment->start_time = 0;
  appointment->is_all_day = FALSE;
}

static void
calendar_appointment_init (CalendarAppointment  *appointment,
                           ICalComponent        *ical,
                           CalendarClientSource *source,
                           ICalTimezone         *default_zone)
{
  appointment->uid          = get_ical_uid (ical);
  appointment->rid          = get_ical_rid (ical);
  appointment->source_uid   = e_source_dup_uid (e_client_get_source (E_CLIENT (source->cal_client)));
  appointment->summary      = get_ical_summary (ical);
  appointment->description  = get_ical_description (ical);
  appointment->color_string = get_source_color (source->cal_client);
  appointment->start_time   = get_ical_start_time (ical, default_zone);
  appointment->end_time     = get_ical_end_time (ical, default_zone);
  appointment->is_all_day   = get_ical_is_all_day (ical,
                                                   appointment->start_time,
                                                   default_zone);
}

static ICalTimezone *
resolve_timezone_id (const char *tzid,
                     gpointer user_data,
                     GCancellable *cancellable,
                     GError **error)
{
  ICalTimezone *retval;
  ECalClient *source = user_data;

  retval = i_cal_timezone_get_builtin_timezone_from_tzid (tzid);
  if (!retval)
    {
      retval = e_cal_client_tzlookup_cb (tzid, source, cancellable, error);
    }

  return retval;
}

static gboolean
calendar_appointment_collect_occurrence (ICalComponent  *component,
                                         ICalTime       *occurrence_start,
                                         ICalTime       *occurrence_end,
                                         gpointer        data,
                                         GCancellable   *cancellable,
                                         GError        **error)
{
  CalendarOccurrence *occurrence;
  GSList **collect_loc = data;

  occurrence             = g_new0 (CalendarOccurrence, 1);
  occurrence->start_time = i_cal_time_as_timet (occurrence_start);
  occurrence->end_time   = i_cal_time_as_timet (occurrence_end);

  *collect_loc = g_slist_prepend (*collect_loc, occurrence);

  return TRUE;
}

static void
calendar_appointment_generate_ocurrences (CalendarAppointment *appointment,
                                          ICalComponent       *ical,
                                          ECalClient          *source,
                                          time_t               start,
                                          time_t               end,
                                          ICalTimezone        *default_zone)
{
  ICalTime *interval_start, *interval_end;

  g_assert (appointment->occurrences == NULL);

  interval_start = i_cal_time_new_from_timet_with_zone (start, FALSE, NULL);
  interval_end = i_cal_time_new_from_timet_with_zone (end, FALSE, NULL);

  e_cal_recur_generate_instances_sync (ical,
                                       interval_start,
                                       interval_end,
                                       calendar_appointment_collect_occurrence,
                                       &appointment->occurrences,
                                       resolve_timezone_id,
                                       source,
                                       default_zone,
                                       NULL,
                                       NULL);

  g_clear_object (&interval_start);
  g_clear_object (&interval_end);

  appointment->occurrences = g_slist_reverse (appointment->occurrences);
}

static inline gboolean
calendar_task_equal (CalendarTask *a,
                     CalendarTask *b)
{
  return
    g_strcmp0 (a->uid,          b->uid)          == 0 &&
    g_strcmp0 (a->summary,      b->summary)      == 0 &&
    g_strcmp0 (a->description,  b->description)  == 0 &&
    g_strcmp0 (a->color_string, b->color_string) == 0 &&
    a->start_time       == b->start_time              &&
    a->due_time         == b->due_time                &&
    a->percent_complete == b->percent_complete        &&
    a->completed_time   == b->completed_time          &&
    a->priority         == b->priority;
}

static void
calendar_task_copy (CalendarTask *task,
                    CalendarTask *task_copy)
{
  g_assert (task != NULL);
  g_assert (task_copy != NULL);

  task_copy->uid              = g_strdup (task->uid);
  task_copy->summary          = g_strdup (task->summary);
  task_copy->description      = g_strdup (task->description);
  task_copy->color_string     = g_strdup (task->color_string);
  task_copy->start_time       = task->start_time;
  task_copy->due_time         = task->due_time;
  task_copy->percent_complete = task->percent_complete;
  task_copy->completed_time   = task->completed_time;
  task_copy->priority         = task->priority;
}

static void
calendar_task_finalize (CalendarTask *task)
{
  g_free (task->uid);
  task->uid = NULL;

  g_free (task->summary);
  task->summary = NULL;

  g_free (task->description);
  task->description = NULL;

  g_free (task->color_string);
  task->color_string = NULL;

  task->percent_complete = 0;
}

static void
calendar_task_init (CalendarTask         *task,
                    ICalComponent        *ical,
                    CalendarClientSource *source,
                    ICalTimezone         *default_zone)
{
  task->uid              = get_ical_uid (ical);
  task->summary          = get_ical_summary (ical);
  task->description      = get_ical_description (ical);
  task->color_string     = get_source_color (source->cal_client);
  task->start_time       = get_ical_start_time (ical, default_zone);
  task->due_time         = get_ical_due_time (ical, default_zone);
  task->percent_complete = get_ical_percent_complete (ical);
  task->completed_time   = get_ical_completed_time (ical, default_zone);
  task->priority         = get_ical_priority (ical);
}

void
calendar_event_free (CalendarEvent *event)
{
  switch (event->type)
    {
    case CALENDAR_EVENT_APPOINTMENT:
      calendar_appointment_finalize (CALENDAR_APPOINTMENT (event));
      break;
    case CALENDAR_EVENT_TASK:
      calendar_task_finalize (CALENDAR_TASK (event));
      break;
    case CALENDAR_EVENT_ALL:
    default:
      g_assert_not_reached ();
      break;
    }

  g_free (event);
}

static CalendarEvent *
calendar_event_new (ICalComponent        *ical,
                    CalendarClientSource *source,
                    ICalTimezone         *default_zone)
{
  CalendarEvent *event;

  event = g_new0 (CalendarEvent, 1);

  switch (i_cal_component_isa (ical))
    {
    case I_CAL_VEVENT_COMPONENT:
      event->type = CALENDAR_EVENT_APPOINTMENT;
      calendar_appointment_init (CALENDAR_APPOINTMENT (event),
                                 ical,
                                 source,
                                 default_zone);
      break;
    case I_CAL_VTODO_COMPONENT:
      event->type = CALENDAR_EVENT_TASK;
      calendar_task_init (CALENDAR_TASK (event),
                          ical,
                          source,
                          default_zone);
      break;
    case I_CAL_NO_COMPONENT:
    case I_CAL_ANY_COMPONENT:
    case I_CAL_XROOT_COMPONENT:
    case I_CAL_XATTACH_COMPONENT:
    case I_CAL_VJOURNAL_COMPONENT:
    case I_CAL_VCALENDAR_COMPONENT:
    case I_CAL_VAGENDA_COMPONENT:
    case I_CAL_VFREEBUSY_COMPONENT:
    case I_CAL_VALARM_COMPONENT:
    case I_CAL_XAUDIOALARM_COMPONENT:
    case I_CAL_XDISPLAYALARM_COMPONENT:
    case I_CAL_XEMAILALARM_COMPONENT:
    case I_CAL_XPROCEDUREALARM_COMPONENT:
    case I_CAL_VTIMEZONE_COMPONENT:
    case I_CAL_XSTANDARD_COMPONENT:
    case I_CAL_XDAYLIGHT_COMPONENT:
    case I_CAL_X_COMPONENT:
    case I_CAL_VSCHEDULE_COMPONENT:
    case I_CAL_VQUERY_COMPONENT:
    case I_CAL_VREPLY_COMPONENT:
    case I_CAL_VCAR_COMPONENT:
    case I_CAL_VCOMMAND_COMPONENT:
    case I_CAL_XLICINVALID_COMPONENT:
    case I_CAL_XLICMIMEPART_COMPONENT:
    default:
      g_warning ("Unknown calendar component type: %d\n",
                 i_cal_component_isa (ical));
      g_free (event);
      return NULL;
    }

  return event;
}

static CalendarEvent *
calendar_event_copy (CalendarEvent *event)
{
  CalendarEvent *retval;

  if (!event)
    return NULL;

  retval = g_new0 (CalendarEvent, 1);

  retval->type = event->type;

  switch (event->type)
    {
    case CALENDAR_EVENT_APPOINTMENT:
      calendar_appointment_copy (CALENDAR_APPOINTMENT (event),
                                 CALENDAR_APPOINTMENT (retval));
      break;
    case CALENDAR_EVENT_TASK:
      calendar_task_copy (CALENDAR_TASK (event),
                          CALENDAR_TASK (retval));
      break;
    case CALENDAR_EVENT_ALL:
    default:
      g_assert_not_reached ();
      break;
    }

  return retval;
}

static char *
calendar_event_get_uid (CalendarEvent *event)
{
  switch (event->type)
    {
    case CALENDAR_EVENT_APPOINTMENT:
      return g_strdup_printf ("%s%s", CALENDAR_APPOINTMENT (event)->uid, CALENDAR_APPOINTMENT (event)->rid ? CALENDAR_APPOINTMENT (event)->rid : "");
      break;
    case CALENDAR_EVENT_TASK:
      return g_strdup (CALENDAR_TASK (event)->uid);
      break;
    case CALENDAR_EVENT_ALL:
    default:
      g_assert_not_reached ();
      break;
    }

  return NULL;
}

static gboolean
calendar_event_equal (CalendarEvent *a,
                      CalendarEvent *b)
{
  if (!a && !b)
    return TRUE;

  if ((a && !b) || (!a && b))
    return FALSE;

  if (a->type != b->type)
    return FALSE;

  switch (a->type)
    {
    case CALENDAR_EVENT_APPOINTMENT:
      return calendar_appointment_equal (CALENDAR_APPOINTMENT (a),
                                         CALENDAR_APPOINTMENT (b));
    case CALENDAR_EVENT_TASK:
      return calendar_task_equal (CALENDAR_TASK (a),
                                  CALENDAR_TASK (b));
    case CALENDAR_EVENT_ALL:
    default:
      break;
    }

  g_assert_not_reached ();

  return FALSE;
}

static void
calendar_event_generate_ocurrences (CalendarEvent *event,
                                    ICalComponent *ical,
                                    ECalClient    *source,
                                    time_t         start,
                                    time_t         end,
                                    ICalTimezone  *default_zone)
{
  if (event->type != CALENDAR_EVENT_APPOINTMENT)
    return;

  calendar_appointment_generate_ocurrences (CALENDAR_APPOINTMENT (event),
                                            ical,
                                            source,
                                            start,
                                            end,
                                            default_zone);
}

static inline void
calendar_event_debug_dump (CalendarEvent *event)
{
#ifdef CALENDAR_ENABLE_DEBUG
  switch (event->type)
    {
    case CALENDAR_EVENT_APPOINTMENT:
      {
        char   *start_str;
        char   *end_str;
        GSList *l;

        start_str = CALENDAR_APPOINTMENT (event)->start_time ?
          isodate_from_time_t (CALENDAR_APPOINTMENT (event)->start_time) :
          g_strdup ("(undefined)");
        end_str = CALENDAR_APPOINTMENT (event)->end_time ?
          isodate_from_time_t (CALENDAR_APPOINTMENT (event)->end_time) :
          g_strdup ("(undefined)");

        dprintf ("Appointment: uid '%s', summary '%s', description '%s', "
                 "start_time '%s', end_time '%s', is_all_day %s\n",
                 CALENDAR_APPOINTMENT (event)->uid,
                 CALENDAR_APPOINTMENT (event)->summary,
                 CALENDAR_APPOINTMENT (event)->description,
                 start_str,
                 end_str,
                 CALENDAR_APPOINTMENT (event)->is_all_day ? "(true)" : "(false)");

        g_free (start_str);
        g_free (end_str);

        dprintf ("  Occurrences:\n");
        for (l = CALENDAR_APPOINTMENT (event)->occurrences; l; l = l->next)
          {
            CalendarOccurrence *occurrence = l->data;

            start_str = occurrence->start_time ?
              isodate_from_time_t (occurrence->start_time) :
              g_strdup ("(undefined)");

            end_str = occurrence->end_time ?
              isodate_from_time_t (occurrence->end_time) :
              g_strdup ("(undefined)");

            dprintf ("    start_time '%s', end_time '%s'\n",
                     start_str, end_str);

            g_free (start_str);
            g_free (end_str);
          }
      }
      break;
    case CALENDAR_EVENT_TASK:
      {
        char *start_str;
        char *due_str;
        char *completed_str;

        start_str = CALENDAR_TASK (event)->start_time ?
          isodate_from_time_t (CALENDAR_TASK (event)->start_time) :
          g_strdup ("(undefined)");
        due_str = CALENDAR_TASK (event)->due_time ?
          isodate_from_time_t (CALENDAR_TASK (event)->due_time) :
          g_strdup ("(undefined)");
        completed_str = CALENDAR_TASK (event)->completed_time ?
          isodate_from_time_t (CALENDAR_TASK (event)->completed_time) :
          g_strdup ("(undefined)");

        dprintf ("Task: uid '%s', summary '%s', description '%s', "
                 "start_time '%s', due_time '%s', percent_complete %d, completed_time '%s'\n",
                 CALENDAR_TASK (event)->uid,
                 CALENDAR_TASK (event)->summary,
                 CALENDAR_TASK (event)->description,
                 start_str,
                 due_str,
                 CALENDAR_TASK (event)->percent_complete,
                 completed_str);

        g_free (completed_str);
      }
      break;
    default:
      g_assert_not_reached ();
      break;
    }
#endif
}

static inline CalendarClientQuery *
goddamn_this_is_crack (CalendarClientSource *source,
                       ECalClientView       *view,
                       gboolean             *emit_signal)
{
  g_assert (view != NULL);

  if (source->completed_query.view == view)
    {
      if (emit_signal)
        *emit_signal = TRUE;
      return &source->completed_query;
    }
  else if (source->in_progress_query.view == view)
    {
      if (emit_signal)
        *emit_signal = FALSE;
      return &source->in_progress_query;
    }

  g_assert_not_reached ();

  return NULL;
}

static void
calendar_client_handle_query_completed (CalendarClientSource *source,
                                        const GError *error,
                                        ECalClientView *view)
{
  CalendarClientQuery *query;

  query = goddamn_this_is_crack (source, view, NULL);

  dprintf ("Query %p completed: %s\n", query, error ? error->message : "Success");

  if (error)
    {
      g_warning ("Calendar query failed: %s\n",
                 error->message);
      calendar_client_stop_query (source->client, source, query);
      return;
    }

  g_assert (source->query_in_progress != FALSE);
  g_assert (query == &source->in_progress_query);

  calendar_client_query_finalize (&source->completed_query);

  source->completed_query = source->in_progress_query;
  source->query_completed = TRUE;

  source->query_in_progress        = FALSE;
  source->in_progress_query.view   = NULL;
  source->in_progress_query.events = NULL;

  g_signal_emit (source->client, source->changed_signal_id, 0);
}

static void
calendar_client_handle_query_result (CalendarClientSource *source,
                                     const GSList         *objects,
                                     ECalClientView       *view)
{
  CalendarClientQuery *query;
  CalendarClient      *client;
  gboolean             emit_signal;
  gboolean             events_changed;
  const GSList        *l;
  time_t               month_begin;
  time_t               month_end;

  client = source->client;

  query = goddamn_this_is_crack (source, view, &emit_signal);

  dprintf ("Query %p result: %d objects:\n",
           query, g_list_length (objects));

  month_begin = make_time_for_day_begin (1,
                                         client->priv->month,
                                         client->priv->year);

  month_end = make_time_for_day_begin (1,
                                       client->priv->month + 1,
                                       client->priv->year);

  events_changed = FALSE;
  for (l = objects; l; l = l->next)
    {
      CalendarEvent *event;
      CalendarEvent *old_event;
      ICalComponent *ical = l->data;
      char          *uid;

      event = calendar_event_new (ical, source, client->priv->zone);
      if (!event)
        continue;

      calendar_event_generate_ocurrences (event,
                                          ical,
                                          source->cal_client,
                                          month_begin,
                                          month_end,
                                          client->priv->zone);

      uid = calendar_event_get_uid (event);

      old_event = g_hash_table_lookup (query->events, uid);

      if (!calendar_event_equal (event, old_event))
        {
          dprintf ("Event %s: ", old_event ? "modified" : "added");

          calendar_event_debug_dump (event);

          g_hash_table_replace (query->events, uid, event);

          events_changed = TRUE;
        }
      else
        {
          g_free (uid);
        }
    }

  if (emit_signal && events_changed)
    {
      g_signal_emit (source->client, source->changed_signal_id, 0);
    }
}

static gboolean
check_object_remove (gpointer key,
                     gpointer value,
                     gpointer data)
{
  char             *uid = data;
  size_t           len;

  len = strlen (uid);

  if (len <= strlen (key) && strncmp (uid, key, len) == 0)
    {
      dprintf ("Event removed: ");

      calendar_event_debug_dump (value);

      return TRUE;
    }

  return FALSE;
}

static void
calendar_client_handle_objects_removed (CalendarClientSource *source,
                                        const GSList         *ids,
                                        ECalClientView       *view)
{
  CalendarClientQuery *query;
  gboolean             emit_signal;
  gboolean             events_changed;
  const GSList        *l;

  query = goddamn_this_is_crack (source, view, &emit_signal);

  events_changed = FALSE;
  for (l = ids; l; l = l->next)
    {
      CalendarEvent   *event;
      ECalComponentId *id = l->data;
      char            *uid = g_strdup_printf ("%s%s", e_cal_component_id_get_uid (id),
                                                      e_cal_component_id_get_rid (id) ? e_cal_component_id_get_rid (id) : "");

      if (!e_cal_component_id_get_rid (id) || !(e_cal_component_id_get_rid (id)[0]))
        {
          unsigned int size = g_hash_table_size (query->events);

          g_hash_table_foreach_remove (query->events, check_object_remove, (gpointer) e_cal_component_id_get_uid (id));

          if (size != g_hash_table_size (query->events))
            events_changed = TRUE;
        }
      else if ((event = g_hash_table_lookup (query->events, uid)))
        {
          dprintf ("Event removed: ");

          calendar_event_debug_dump (event);

          g_assert (g_hash_table_remove (query->events, uid));

          events_changed = TRUE;
        }
      g_free (uid);
    }

  if (emit_signal && events_changed)
    {
      g_signal_emit (source->client, source->changed_signal_id, 0);
    }
}

static void
calendar_client_query_finalize (CalendarClientQuery *query)
{
  if (query->view)
    g_object_unref (query->view);
  query->view = NULL;

  if (query->events)
    g_hash_table_destroy (query->events);
  query->events = NULL;
}

static void
calendar_client_stop_query (CalendarClient       *client,
                            CalendarClientSource *source,
                            CalendarClientQuery  *query)
{
  if (query == &source->in_progress_query)
    {
      dprintf ("Stopping in progress query %p\n", query);

      g_assert (source->query_in_progress != FALSE);

      source->query_in_progress = FALSE;
    }
  else if (query == &source->completed_query)
    {
      dprintf ("Stopping completed query %p\n", query);

      g_assert (source->query_completed != FALSE);

      source->query_completed = FALSE;
    }
  else
    g_assert_not_reached ();

  calendar_client_query_finalize (query);
}

static void
calendar_client_start_query (CalendarClient       *client,
                             CalendarClientSource *source,
                             const char           *query)
{
  ECalClientView *view = NULL;
  GError   *error = NULL;

  if (!e_cal_client_get_view_sync (source->cal_client, query, &view, NULL, &error))
    {
      g_warning ("Error preparing the query: '%s': %s\n",
                 query, error->message);
      g_error_free (error);
      return;
    }

  g_assert (view != NULL);

  if (source->query_in_progress)
    calendar_client_stop_query (client, source, &source->in_progress_query);

  dprintf ("Starting query %p: '%s'\n", &source->in_progress_query, query);

  source->query_in_progress        = TRUE;
  source->in_progress_query.view   = view;
  source->in_progress_query.events =
    g_hash_table_new_full (g_str_hash,
                           g_str_equal,
                           g_free,
                           (GDestroyNotify) calendar_event_free);

  g_signal_connect_swapped (view, "objects-added",
                            G_CALLBACK (calendar_client_handle_query_result),
                            source);
  g_signal_connect_swapped (view, "objects-modified",
                            G_CALLBACK (calendar_client_handle_query_result),
                            source);
  g_signal_connect_swapped (view, "objects-removed",
                            G_CALLBACK (calendar_client_handle_objects_removed),
                            source);
  g_signal_connect_swapped (view, "complete",
                            G_CALLBACK (calendar_client_handle_query_completed),
                            source);

  e_cal_client_view_start (view, NULL);
}

static void
calendar_client_update_appointments (CalendarClient *client)
{
  GSList *l;
  char   *query;
  char   *month_begin;
  char   *month_end;

  if (client->priv->month == G_MAXUINT ||
      client->priv->year  == G_MAXUINT)
    return;

  month_begin = make_isodate_for_day_begin (1,
                                            client->priv->month,
                                            client->priv->year);

  month_end = make_isodate_for_day_begin (1,
                                          client->priv->month + 1,
                                          client->priv->year);

  query = g_strdup_printf ("occur-in-time-range? (make-time \"%s\") "
                           "(make-time \"%s\")",
                           month_begin, month_end);

  for (l = client->priv->appointment_sources; l; l = l->next)
    {
      CalendarClientSource *cs = l->data;

      calendar_client_start_query (client, cs, query);
    }

  g_free (month_begin);
  g_free (month_end);
  g_free (query);
}

/* FIXME:
 * perhaps we should use evo's "hide_completed_tasks" pref?
 */
static void
calendar_client_update_tasks (CalendarClient *client)
{
  GSList *l;
  char   *query;

#ifdef FIX_BROKEN_TASKS_QUERY
  /* FIXME: this doesn't work for tasks without a start or
   *        due date
   *        Look at filter_task() to see the behaviour we
   *        want.
   */

  char   *day_begin;
  char   *day_end;

  if (client->priv->day   == G_MAXUINT ||
      client->priv->month == G_MAXUINT ||
      client->priv->year  == G_MAXUINT)
    return;

  day_begin = make_isodate_for_day_begin (client->priv->day,
                                          client->priv->month,
                                          client->priv->year);

  day_end = make_isodate_for_day_begin (client->priv->day + 1,
                                        client->priv->month,
                                        client->priv->year);
  if (!day_begin || !day_end)
    {
      g_warning ("Cannot run query with invalid date: %dd %dy %dm\n",
                 client->priv->day,
                 client->priv->month,
                 client->priv->year);
      g_free (day_begin);
      g_free (day_end);
      return;
    }

  query = g_strdup_printf ("(and (occur-in-time-range? (make-time \"%s\") "
                           "(make-time \"%s\")) "
                           "(or (not is-completed?) "
                           "(and (is-completed?) "
                           "(not (completed-before? (make-time \"%s\"))))))",
                           day_begin, day_end, day_begin);
#else
  query = g_strdup ("#t");
#endif /* FIX_BROKEN_TASKS_QUERY */

  for (l = client->priv->task_sources; l; l = l->next)
    {
      CalendarClientSource *cs = l->data;

      calendar_client_start_query (client, cs, query);
    }

#ifdef FIX_BROKEN_TASKS_QUERY
  g_free (day_begin);
  g_free (day_end);
#endif
  g_free (query);
}

static void
calendar_client_source_finalize (CalendarClientSource *source)
{
  source->client = NULL;

  if (source->cal_client)
    g_object_unref (source->cal_client);
  source->cal_client = NULL;

  calendar_client_query_finalize (&source->completed_query);
  calendar_client_query_finalize (&source->in_progress_query);

  source->query_completed   = FALSE;
  source->query_in_progress = FALSE;
}

static int
compare_calendar_sources (CalendarClientSource *s1,
                          CalendarClientSource *s2)
{
  return (s1->cal_client == s2->cal_client) ? 0 : 1;
}

static GSList *
calendar_client_update_sources_list (CalendarClient *client,
                                     GSList         *sources,
                                     GSList         *esources,
                                     guint           changed_signal_id)
{
  GSList *retval, *l;

  retval = NULL;

  for (l = esources; l; l = l->next)
    {
      CalendarClientSource  dummy_source;
      CalendarClientSource *new_source;
      GSList               *s;
      ECalClient           *esource = l->data;

      dummy_source.cal_client = esource;

      dprintf ("update_sources_list: adding client %s: ",
               e_source_get_uid (e_client_get_source (E_CLIENT (esource))));

      if ((s = g_slist_find_custom (sources,
                                    &dummy_source,
                                    (GCompareFunc) compare_calendar_sources)))
        {
          dprintf ("already on list\n");
          new_source = s->data;
          sources = g_slist_delete_link (sources, s);
        }
      else
        {
          dprintf ("added\n");
          new_source                    = g_new0 (CalendarClientSource, 1);
          new_source->client            = client;
          new_source->cal_client        = g_object_ref (esource);
          new_source->changed_signal_id = changed_signal_id;
        }

      retval = g_slist_prepend (retval, new_source);
    }

  for (l = sources; l; l = l->next)
    {
      CalendarClientSource *source = l->data;

      dprintf ("Removing client %s from list\n",
               e_source_get_uid (e_client_get_source (E_CLIENT (source->cal_client))));

      calendar_client_source_finalize (source);
      g_free (source);
    }
  g_slist_free (sources);

  return retval;
}

static void
calendar_client_appointment_sources_changed (CalendarClient  *client)
{
  GSList *esources;

  dprintf ("appointment_sources_changed: updating ...\n");

  esources = calendar_sources_get_appointment_sources (client->priv->calendar_sources);

  client->priv->appointment_sources =
    calendar_client_update_sources_list (client,
                                         client->priv->appointment_sources,
                                         esources,
                                         signals [APPOINTMENTS_CHANGED]);

  load_calendars (client, CALENDAR_EVENT_APPOINTMENT);
  calendar_client_update_appointments (client);
}

static void
calendar_client_task_sources_changed (CalendarClient  *client)
{
  GSList *esources;

  dprintf ("task_sources_changed: updating ...\n");

  esources = calendar_sources_get_task_sources (client->priv->calendar_sources);

  client->priv->task_sources =
    calendar_client_update_sources_list (client,
                                         client->priv->task_sources,
                                         esources,
                                         signals [TASKS_CHANGED]);

  load_calendars (client, CALENDAR_EVENT_TASK);
  calendar_client_update_tasks (client);
}

void
calendar_client_get_date (CalendarClient *client,
                          guint          *year,
                          guint          *month,
                          guint          *day)
{
  g_return_if_fail (CALENDAR_IS_CLIENT (client));

  if (year)
    *year = client->priv->year;

  if (month)
    *month = client->priv->month;

  if (day)
    *day = client->priv->day;
}

void
calendar_client_select_month (CalendarClient *client,
                              guint           month,
                              guint           year)
{
  g_return_if_fail (CALENDAR_IS_CLIENT (client));
  g_return_if_fail (month <= 11);

  if (client->priv->year != year || client->priv->month != month)
    {
      client->priv->month = month;
      client->priv->year  = year;

      calendar_client_update_appointments (client);
      calendar_client_update_tasks (client);

      g_object_freeze_notify (G_OBJECT (client));
      g_object_notify (G_OBJECT (client), "month");
      g_object_notify (G_OBJECT (client), "year");
      g_object_thaw_notify (G_OBJECT (client));
    }
}

void
calendar_client_select_day (CalendarClient *client,
                            guint           day)
{
  g_return_if_fail (CALENDAR_IS_CLIENT (client));
  g_return_if_fail (day <= 31);

  if (client->priv->day != day)
    {
      client->priv->day = day;

      /* don't need to update appointments unless
       * the selected month changes
       */
#ifdef FIX_BROKEN_TASKS_QUERY
      calendar_client_update_tasks (client);
#endif

      g_object_notify (G_OBJECT (client), "day");
    }
}

typedef struct
{
  CalendarClient *client;
  GSList         *events;
  time_t          start_time;
  time_t          end_time;
} FilterData;

typedef void (* CalendarEventFilterFunc) (const char    *uid,
                                          CalendarEvent *event,
                                          FilterData    *filter_data);

static void
filter_appointment (const char    *uid,
                    CalendarEvent *event,
                    FilterData    *filter_data)
{
  GSList *occurrences, *l;

  if (event->type != CALENDAR_EVENT_APPOINTMENT)
    return;

  occurrences = CALENDAR_APPOINTMENT (event)->occurrences;
  CALENDAR_APPOINTMENT (event)->occurrences = NULL;

  for (l = occurrences; l; l = l->next)
    {
      CalendarOccurrence *occurrence = l->data;
      time_t start_time = occurrence->start_time;
      time_t end_time   = occurrence->end_time;

      if ((start_time >= filter_data->start_time &&
           start_time < filter_data->end_time) ||
          (start_time <= filter_data->start_time &&
           (end_time - 1) > filter_data->start_time))
        {
          CalendarEvent *new_event;

          new_event = calendar_event_copy (event);

          CALENDAR_APPOINTMENT (new_event)->start_time = occurrence->start_time;
          CALENDAR_APPOINTMENT (new_event)->end_time   = occurrence->end_time;

          filter_data->events = g_slist_prepend (filter_data->events, new_event);
        }
    }

  CALENDAR_APPOINTMENT (event)->occurrences = occurrences;
}

static void
filter_task (const char    *uid,
             CalendarEvent *event,
             FilterData    *filter_data)
{
#ifdef FIX_BROKEN_TASKS_QUERY
  CalendarTask *task;
#endif

  if (event->type != CALENDAR_EVENT_TASK)
    return;

#ifdef FIX_BROKEN_TASKS_QUERY
  task = CALENDAR_TASK (event);

  if (task->start_time && task->start_time > filter_data->start_time)
    return;

  if (task->completed_time &&
      (task->completed_time < filter_data->start_time ||
       task->completed_time > filter_data->end_time))
    return;
#endif /* FIX_BROKEN_TASKS_QUERY */

  filter_data->events = g_slist_prepend (filter_data->events,
                                         calendar_event_copy (event));
}

static GSList *
calendar_client_filter_events (CalendarClient          *client,
                               GSList                  *sources,
                               CalendarEventFilterFunc  filter_func,
                               time_t                   start_time,
                               time_t                   end_time)
{
  FilterData  filter_data;
  GSList     *l;
  GSList     *retval;

  if (!sources)
    return NULL;

  filter_data.client     = client;
  filter_data.events     = NULL;
  filter_data.start_time = start_time;
  filter_data.end_time   = end_time;

  retval = NULL;
  for (l = sources; l; l = l->next)
    {
      CalendarClientSource *source = l->data;

      if (source->query_completed)
        {
          filter_data.events = NULL;
          g_hash_table_foreach (source->completed_query.events,
                                (GHFunc) filter_func,
                                &filter_data);

          filter_data.events = g_slist_reverse (filter_data.events);

          retval = g_slist_concat (retval, filter_data.events);
        }
    }

  return retval;
}

GSList *
calendar_client_get_events (CalendarClient    *client,
                            CalendarEventType  event_mask)
{
  GSList *appointments;
  GSList *tasks;
  time_t  day_begin;
  time_t  day_end;

  g_return_val_if_fail (CALENDAR_IS_CLIENT (client), NULL);
  g_return_val_if_fail (client->priv->day   != G_MAXUINT &&
                        client->priv->month != G_MAXUINT &&
                        client->priv->year  != G_MAXUINT, NULL);

  day_begin = make_time_for_day_begin (client->priv->day,
                                       client->priv->month,
                                       client->priv->year);
  day_end   = make_time_for_day_begin (client->priv->day + 1,
                                       client->priv->month,
                                       client->priv->year);

  appointments = NULL;
  if (event_mask & CALENDAR_EVENT_APPOINTMENT)
    {
      appointments = calendar_client_filter_events (client,
                                                    client->priv->appointment_sources,
                                                    filter_appointment,
                                                    day_begin,
                                                    day_end);
    }

  tasks = NULL;
  if (event_mask & CALENDAR_EVENT_TASK)
    {
      tasks = calendar_client_filter_events (client,
                                             client->priv->task_sources,
                                             filter_task,
                                             day_begin,
                                             day_end);
    }

  return g_slist_concat (appointments, tasks);
}

static inline int
day_from_time_t (time_t t)
{
  struct tm *tm = localtime (&t);

  g_assert (tm == NULL || (tm->tm_mday >=1 && tm->tm_mday <= 31));

  return tm ? tm->tm_mday : 0;
}

void
calendar_client_foreach_appointment_day (CalendarClient  *client,
                                         CalendarDayIter  iter_func,
                                         gpointer         user_data)
{
  GSList   *appointments, *l;
  gboolean  marked_days [32] = { FALSE, };
  time_t    month_begin;
  time_t    month_end;
  int       i;

  g_return_if_fail (CALENDAR_IS_CLIENT (client));
  g_return_if_fail (iter_func != NULL);
  g_return_if_fail (client->priv->month != G_MAXUINT &&
                    client->priv->year  != G_MAXUINT);

  month_begin = make_time_for_day_begin (1,
                                         client->priv->month,
                                         client->priv->year);
  month_end   = make_time_for_day_begin (1,
                                         client->priv->month + 1,
                                         client->priv->year);

  appointments = calendar_client_filter_events (client,
                                                client->priv->appointment_sources,
                                                filter_appointment,
                                                month_begin,
                                                month_end);
  for (l = appointments; l; l = l->next)
    {
      CalendarAppointment *appointment = l->data;

      if (appointment->start_time)
        {
          time_t day_time = appointment->start_time;

          if (day_time >= month_begin)
            marked_days [day_from_time_t (day_time)] = TRUE;

          if (appointment->end_time)
            {
              int day_offset;
              int duration = appointment->end_time - appointment->start_time;
              /* mark the days for the appointment, no need to add an extra one when duration is a multiple of 86400 */
              for (day_offset = 1; day_offset <= duration / 86400 && duration != day_offset * 86400; day_offset++)
                {
                  time_t day_tm = appointment->start_time + day_offset * 86400;

                  if (day_tm > month_end)
                    break;
                  if (day_tm >= month_begin)
                    marked_days [day_from_time_t (day_tm)] = TRUE;
                }
            }
        }
      calendar_event_free (CALENDAR_EVENT (appointment));
    }

  g_slist_free (appointments);

  for (i = 1; i < 32; i++)
    {
      if (marked_days [i])
        iter_func (client, i, user_data);
    }
}

void
calendar_client_set_task_completed (CalendarClient *client,
                                    char           *task_uid,
                                    gboolean        task_completed,
                                    guint           percent_complete)
{
  GSList             *l;
  ECalClient         *esource;
  ICalComponent      *ical;
  ICalProperty       *prop;
  ICalPropertyStatus  status;

  g_return_if_fail (CALENDAR_IS_CLIENT (client));
  g_return_if_fail (task_uid != NULL);
  g_return_if_fail (task_completed == FALSE || percent_complete == 100);

  ical = NULL;
  esource = NULL;
  for (l = client->priv->task_sources; l; l = l->next)
    {
      CalendarClientSource *source = l->data;

      esource = source->cal_client;
      e_cal_client_get_object_sync (esource, task_uid, NULL, &ical, NULL, NULL);
      if (ical)
        break;
    }

  if (!ical)
    {
      g_warning ("Cannot locate task with uid = '%s'\n", task_uid);
      return;
    }

  g_assert (esource != NULL);

  /* Completed time */
  prop = i_cal_component_get_first_property (ical,
                                             I_CAL_COMPLETED_PROPERTY);
  if (task_completed)
    {
      ICalTime *completed_time;

      completed_time = i_cal_time_new_current_with_zone (client->priv->zone);
      if (!prop)
        {
          i_cal_component_take_property (ical,
                                         i_cal_property_new_completed (completed_time));
        }
      else
        {
          i_cal_property_set_completed (prop, completed_time);
        }

      g_clear_object (&completed_time);
    }
  else if (prop)
    {
      i_cal_component_remove_property (ical, prop);
    }

  g_clear_object (&prop);

  /* Percent complete */
  prop = i_cal_component_get_first_property (ical,
                                             I_CAL_PERCENTCOMPLETE_PROPERTY);
  if (!prop)
    {
      i_cal_component_take_property (ical,
                                     i_cal_property_new_percentcomplete (percent_complete));
    }
  else
    {
      i_cal_property_set_percentcomplete (prop, percent_complete);
      g_object_unref (prop);
    }

  /* Status */
  status = task_completed ? I_CAL_STATUS_COMPLETED : I_CAL_STATUS_NEEDSACTION;
  prop = i_cal_component_get_first_property (ical, I_CAL_STATUS_PROPERTY);
  if (prop)
    {
      i_cal_property_set_status (prop, status);
      g_object_unref (prop);
    }
  else
    {
      i_cal_component_take_property (ical,
                                     i_cal_property_new_status (status));
    }

  e_cal_client_modify_object_sync (esource, ical, E_CAL_OBJ_MOD_ALL, E_CAL_OPERATION_FLAG_NONE, NULL, NULL);
}
