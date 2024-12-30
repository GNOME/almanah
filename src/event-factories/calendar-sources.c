/*
 * Copyright (C) 2004 Free Software Foundation, Inc.
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
 */

#include <config.h>

#include "calendar-sources.h"

#include <libintl.h>
#include <string.h>
#define HANDLE_LIBICAL_MEMORY
#include <libecal/libecal.h>
#include <libedataserver/libedataserver.h>

#include "e-source-selector.h"

#undef CALENDAR_ENABLE_DEBUG
#include "calendar-debug.h"

#ifndef _
#define _(x) gettext(x)
#endif

#ifndef N_
#define N_(x) x
#endif

typedef struct _CalendarSourceData CalendarSourceData;

struct _CalendarSourceData
{
  ECalClientSourceType client_type;
  CalendarSources *sources;
  guint            changed_signal;

  GSList          *clients; /* ECalClient * */
  ESourceSelector *esource_selector;

  guint            timeout_id;

  guint            loaded : 1;
};

typedef struct
{
  CalendarSourceData  appointment_sources;
  CalendarSourceData  task_sources;

  ESourceRegistry    *esource_registry;
} CalendarSourcesPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (CalendarSources, calendar_sources, G_TYPE_OBJECT)

static void calendar_sources_finalize   (GObject             *object);

static void backend_died_cb (ECalClient *client, CalendarSourceData *source_data);
static void calendar_sources_selection_changed_cb (ESourceSelector *selector, CalendarSourceData *source_data);

enum
{
  APPOINTMENT_SOURCES_CHANGED,
  TASK_SOURCES_CHANGED,
  LAST_SIGNAL
};
static guint signals [LAST_SIGNAL] = { 0, };

static GObjectClass    *parent_class = NULL;
static CalendarSources *calendar_sources_singleton = NULL;


static void
calendar_sources_class_init (CalendarSourcesClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = calendar_sources_finalize;

  signals [APPOINTMENT_SOURCES_CHANGED] =
    g_signal_new ("appointment-sources-changed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (CalendarSourcesClass,
				   appointment_sources_changed),
		  NULL,
		  NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE,
		  0);

  signals [TASK_SOURCES_CHANGED] =
    g_signal_new ("task-sources-changed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (CalendarSourcesClass,
				   task_sources_changed),
		  NULL,
		  NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE,
		  0);
}

static void
calendar_sources_init (CalendarSources *sources)
{
  GError *error = NULL;

  CalendarSourcesPrivate *priv = calendar_sources_get_instance_private (sources);

  priv->appointment_sources.client_type    = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
  priv->appointment_sources.sources        = sources;
  priv->appointment_sources.changed_signal = signals [APPOINTMENT_SOURCES_CHANGED];
  priv->appointment_sources.timeout_id     = 0;

  priv->task_sources.client_type    = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
  priv->task_sources.sources        = sources;
  priv->task_sources.changed_signal = signals [TASK_SOURCES_CHANGED];
  priv->task_sources.timeout_id     = 0;

  priv->esource_registry = e_source_registry_new_sync (NULL, &error);

  if (error) {
    g_warning ("%s: Failed to create ESourceRegistry: %s", G_STRFUNC, error->message);
    g_clear_error (&error);
  }
}

static void
calendar_sources_finalize_source_data (CalendarSources    *sources,
				       CalendarSourceData *source_data)
{
  if (source_data->loaded)
    {
      GSList *l;

      for (l = source_data->clients; l; l = l->next)
        {
          g_signal_handlers_disconnect_by_func (G_OBJECT (l->data),
                                                G_CALLBACK (backend_died_cb),
                                                source_data);
          g_object_unref (l->data);
        }
      g_slist_free (source_data->clients);
      source_data->clients = NULL;

      if (source_data->esource_selector)
        {
          g_signal_handlers_disconnect_by_func (source_data->esource_selector,
                                                G_CALLBACK (calendar_sources_selection_changed_cb),
                                                source_data);
          g_object_unref (source_data->esource_selector);
	}
      source_data->esource_selector = NULL;

      if (source_data->timeout_id != 0)
        {
          g_source_remove (source_data->timeout_id);
          source_data->timeout_id = 0;
        }

      source_data->loaded = FALSE;
    }
}

static void
calendar_sources_finalize (GObject *object)
{
  CalendarSources *sources = CALENDAR_SOURCES (object);
  CalendarSourcesPrivate *priv = calendar_sources_get_instance_private (sources);

  calendar_sources_finalize_source_data (sources, &priv->appointment_sources);
  calendar_sources_finalize_source_data (sources, &priv->task_sources);

  if (priv->esource_registry)
    g_object_unref (priv->esource_registry);
  priv->esource_registry = NULL;

  if (G_OBJECT_CLASS (parent_class)->finalize)
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

CalendarSources *
calendar_sources_get (void)
{
  gpointer singleton_location = &calendar_sources_singleton;

  if (calendar_sources_singleton)
    return g_object_ref (calendar_sources_singleton);

  calendar_sources_singleton = g_object_new (CALENDAR_TYPE_SOURCES, NULL);
  g_object_add_weak_pointer (G_OBJECT (calendar_sources_singleton),
			     singleton_location);

  return calendar_sources_singleton;
}

/* The clients are just created here but not loaded */
static ECalClient *
get_ecal_from_source (ESource             *esource,
		      ECalClientSourceType client_type,
		      GSList              *existing_clients)
{
  EClient *retval;
  GError *error = NULL;

  if (existing_clients)
    {
      GSList *l;

      for (l = existing_clients; l; l = l->next)
	{
	  ECalClient *client = E_CAL_CLIENT (l->data);

	  if (g_str_equal (e_source_get_uid (esource), e_source_get_uid (e_client_get_source (E_CLIENT (client)))))
	    {
	      dprintf ("        load_esource: found existing source ... returning that\n");

	      return g_object_ref (client);
	    }
	}
    }

  /* Basically do not wait for the connected state */
  retval = e_cal_client_connect_sync (esource, client_type, -1, NULL, &error);
  if (!retval)
    {
      g_warning ("Could not load source '%s' from '%s': %s",
		 e_source_get_display_name (esource),
		 e_source_get_uid (esource),
		 error ? error->message : "Unknown error");
      g_clear_error (&error);

      return NULL;
    }

  return E_CAL_CLIENT (retval);
}

/* - Order doesn't matter
 * - Can just compare object pointers since we
 *   re-use client connections
 */
static gboolean
compare_ecal_lists (GSList *a,
		    GSList *b)
{
  GSList *l;

  if (g_slist_length (a) != g_slist_length (b))
    return FALSE;

  for (l = a; l; l = l->next)
    {
      if (!g_slist_find (b, l->data))
	return FALSE;
    }

  return TRUE;
}

static inline void
debug_dump_ecal_list (GSList *ecal_list)
{
#ifdef CALENDAR_ENABLE_DEBUG
  GSList *l;

  dprintf ("Loaded clients:\n");
  for (l = ecal_list; l; l = l->next)
    {
      ECalClient *client = l->data;
      ESource *source = e_client_get_source (E_CLIENT (client));

      dprintf ("  %s %s\n",
	       e_source_get_uid (source),
	       e_source_get_display_name (source));
    }
#endif
}

static void
calendar_sources_load_esources (CalendarSourceData *source_data);

static gboolean
backend_restart (gpointer data)
{
  CalendarSourceData *source_data = data;

  calendar_sources_load_esources (source_data);

  source_data->timeout_id = 0;
    
  return FALSE;
}

static void
backend_died_cb (ECalClient *client, CalendarSourceData *source_data)
{
  ESource *source;

  source_data->clients = g_slist_remove (source_data->clients, client);
  if (g_slist_length (source_data->clients) < 1) 
    {
      g_slist_free (source_data->clients);
      source_data->clients = NULL;
    }
  source = e_client_get_source (E_CLIENT (client));
  g_warning ("The calendar backend for '%s/%s' has crashed.", e_source_get_uid (source), e_source_get_display_name (source));

  if (source_data->timeout_id != 0)
    {
      g_source_remove (source_data->timeout_id);
      source_data->timeout_id = 0;
    }

  source_data->timeout_id = g_timeout_add_seconds (2, backend_restart,
		  				   source_data);
}

static void
calendar_sources_load_esources (CalendarSourceData *source_data)
{
  GSList  *clients = NULL;
  GSList  *sources, *s;
  gboolean emit_signal = FALSE;

  g_return_if_fail (source_data->esource_selector != NULL);

  dprintf ("Sources:\n");
  sources = e_source_selector_get_selection (source_data->esource_selector);
  for (s = sources; s; s = s->next)
    {
       ESource *esource = E_SOURCE (s->data);
       ECalClient *client;
  
       dprintf ("   type = '%s' uid = '%s', name = '%s': \n",
                source_data->client_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS ? "appointment" : "task",
		e_source_get_uid (esource),
		e_source_get_display_name (esource));

       if ((client = get_ecal_from_source (esource, source_data->client_type, source_data->clients)) != NULL)
	  {
	    clients = g_slist_prepend (clients, client);
	  }
    }
  dprintf ("\n");

  e_source_selector_free_selection (sources);

  if (source_data->loaded && 
      !compare_ecal_lists (source_data->clients, clients))
    emit_signal = TRUE;

  for (s = source_data->clients; s; s = s->next)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (s->data),
                                            G_CALLBACK (backend_died_cb),
                                            source_data);

      g_object_unref (s->data);
    }
  g_slist_free (source_data->clients);
  source_data->clients = g_slist_reverse (clients);

  /* connect to backend_died after we disconnected the previous signal
   * handlers. If we do it before, we'll lose some handlers (for clients that
   * were already there before) */
  for (s = source_data->clients; s; s = s->next)
    {
      g_signal_connect (G_OBJECT (s->data), "backend-died",
                        G_CALLBACK (backend_died_cb), source_data);
    }

  if (emit_signal) 
    {
      dprintf ("Emitting %s-sources-changed signal\n",
	       source_data->client_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS ? "appointment" : "task");
      g_signal_emit (source_data->sources, source_data->changed_signal, 0);
    }

  debug_dump_ecal_list (source_data->clients);
}

static void
calendar_sources_selection_changed_cb (ESourceSelector    *source_selector,
				       CalendarSourceData *source_data)
				       
{
  dprintf ("ESourceSelector selection-changed, reloading\n");

  calendar_sources_load_esources (source_data);
}

static void
calendar_sources_load_sources (CalendarSources    *sources,
			       CalendarSourceData *source_data,
			       const char         *sources_extension)

{
  CalendarSourcesPrivate *priv = calendar_sources_get_instance_private (sources);

  g_return_if_fail (priv->esource_registry != NULL);

  dprintf ("---------------------------\n");
  dprintf ("Loading sources:\n");
  dprintf ("  sources_extension: %s\n", sources_extension);

  source_data->esource_selector = E_SOURCE_SELECTOR (g_object_ref_sink (e_source_selector_new (priv->esource_registry, sources_extension)));
  g_signal_connect (source_data->esource_selector, "selection-changed",
		    G_CALLBACK (calendar_sources_selection_changed_cb),
		    source_data);

  calendar_sources_load_esources (source_data);

  source_data->loaded = TRUE;

  dprintf ("---------------------------\n");
}

GSList *
calendar_sources_get_appointment_sources (CalendarSources *sources)
{
  g_return_val_if_fail (CALENDAR_IS_SOURCES (sources), NULL);

  CalendarSourcesPrivate *priv = calendar_sources_get_instance_private (sources);

  if (!priv->appointment_sources.loaded)
    {
      calendar_sources_load_sources (sources,
				     &priv->appointment_sources,
				     E_SOURCE_EXTENSION_CALENDAR);
    }
  
  return priv->appointment_sources.clients;
}

GSList *
calendar_sources_get_task_sources (CalendarSources *sources)
{
  g_return_val_if_fail (CALENDAR_IS_SOURCES (sources), NULL);

  CalendarSourcesPrivate *priv = calendar_sources_get_instance_private (sources);

  if (!priv->task_sources.loaded)
    {
      calendar_sources_load_sources (sources,
				     &priv->task_sources,
				     E_SOURCE_EXTENSION_TASK_LIST);
    }

  return priv->task_sources.clients;
}
