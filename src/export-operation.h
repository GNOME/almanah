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

#ifndef ALMANAH_EXPORT_OPERATION_H
#define ALMANAH_EXPORT_OPERATION_H

#include <glib.h>
#include <glib-object.h>

#include "storage-manager.h"

G_BEGIN_DECLS

typedef guint AlmanahExportOperationType;

typedef void (*AlmanahExportProgressCallback) (const GDate *date, gpointer user_data);

#define ALMANAH_TYPE_EXPORT_OPERATION       (almanah_export_operation_get_type ())

G_DECLARE_FINAL_TYPE (AlmanahExportOperation, almanah_export_operation, ALMANAH, EXPORT_OPERATION, GObject)

AlmanahExportOperation *almanah_export_operation_new (AlmanahExportOperationType type_id, AlmanahStorageManager *source_storage_manager,
                                                      GFile *destination) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

void almanah_export_operation_run (AlmanahExportOperation *self, GCancellable *cancellable,
                                   AlmanahExportProgressCallback progress_callback, gpointer progress_user_data,
                                   GAsyncReadyCallback callback, gpointer user_data);
gboolean almanah_export_operation_finish (AlmanahExportOperation *self, GAsyncResult *async_result, GError **error);

void almanah_export_operation_populate_model (GtkListStore *list_store, guint type_id_column, guint name_column, guint description_column,
                                              guint action_column);

G_END_DECLS

#endif /* !ALMANAH_EXPORT_OPERATION_H */
