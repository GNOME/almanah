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

#ifndef ALMANAH_IMPORT_OPERATION_H
#define ALMANAH_IMPORT_OPERATION_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* TODO: These must be kept in synchrony with the rows in almanah_ird_view_store in almanah.ui */
typedef enum {
	ALMANAH_IMPORT_STATUS_IMPORTED = 0,
	ALMANAH_IMPORT_STATUS_MERGED = 1,
	ALMANAH_IMPORT_STATUS_FAILED = 2
} AlmanahImportStatus;

typedef guint AlmanahImportOperationType;

typedef void (*AlmanahImportProgressCallback) (const GDate *date, AlmanahImportStatus status, const gchar *message, gpointer user_data);

#define ALMANAH_TYPE_IMPORT_OPERATION		(almanah_import_operation_get_type ())
#define ALMANAH_IMPORT_OPERATION(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_IMPORT_OPERATION, AlmanahImportOperation))
#define ALMANAH_IMPORT_OPERATION_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_IMPORT_OPERATION, AlmanahImportOperationClass))
#define ALMANAH_IS_IMPORT_OPERATION(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_IMPORT_OPERATION))
#define ALMANAH_IS_IMPORT_OPERATION_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_IMPORT_OPERATION))
#define ALMANAH_IMPORT_OPERATION_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_IMPORT_OPERATION, AlmanahImportOperationClass))

typedef struct _AlmanahImportOperationPrivate	AlmanahImportOperationPrivate;

typedef struct {
	GObject parent;
	AlmanahImportOperationPrivate *priv;
} AlmanahImportOperation;

typedef struct {
	GObjectClass parent;
} AlmanahImportOperationClass;

GType almanah_import_operation_get_type (void) G_GNUC_CONST;

AlmanahImportOperation *almanah_import_operation_new (AlmanahImportOperationType type_id, GFile *source) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

void almanah_import_operation_run (AlmanahImportOperation *self, GCancellable *cancellable,
                                   AlmanahImportProgressCallback progress_callback, gpointer progress_user_data,
                                   GAsyncReadyCallback callback, gpointer user_data);
gboolean almanah_import_operation_finish (AlmanahImportOperation *self, GAsyncResult *async_result, GError **error);

void almanah_import_operation_populate_model (GtkListStore *list_store, guint type_id_column, guint name_column, guint description_column,
                                              guint action_column, guint file_chooser_title_column);

G_END_DECLS

#endif /* !ALMANAH_IMPORT_OPERATION_H */
