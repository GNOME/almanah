/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2011 <philip@tecnocode.co.uk>
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

#ifndef ALMANAH_APPLICATION_H
#define ALMANAH_APPLICATION_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "event-manager.h"
#include "storage-manager.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_APPLICATION		(almanah_application_get_type ())
#define ALMANAH_APPLICATION(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_APPLICATION, AlmanahApplication))
#define ALMANAH_APPLICATION_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_APPLICATION, AlmanahApplicationClass))
#define ALMANAH_IS_APPLICATION(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_APPLICATION))
#define ALMANAH_IS_APPLICATION_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_APPLICATION))
#define ALMANAH_APPLICATION_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_APPLICATION, AlmanahApplicationClass))

typedef struct _AlmanahApplicationPrivate	AlmanahApplicationPrivate;

typedef struct {
	GtkApplication parent;
	AlmanahApplicationPrivate *priv;
} AlmanahApplication;

typedef struct {
	GtkApplicationClass parent;
} AlmanahApplicationClass;

GType almanah_application_get_type (void) G_GNUC_CONST;

AlmanahApplication *almanah_application_new (void) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

gboolean almanah_application_get_debug (AlmanahApplication *self) G_GNUC_PURE;

AlmanahEventManager *almanah_application_dup_event_manager (AlmanahApplication *self) G_GNUC_WARN_UNUSED_RESULT;
AlmanahStorageManager *almanah_application_dup_storage_manager (AlmanahApplication *self) G_GNUC_WARN_UNUSED_RESULT;
GSettings *almanah_application_dup_settings (AlmanahApplication *self) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* !ALMANAH_APPLICATION_H */
