/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2009–2010 <philip@tecnocode.co.uk>
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

#ifndef ALMANAH_IMPORT_EXPORT_DIALOG_H
#define ALMANAH_IMPORT_EXPORT_DIALOG_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "import-operation.h"
#include "storage-manager.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_IMPORT_EXPORT_DIALOG		(almanah_import_export_dialog_get_type ())
#define ALMANAH_IMPORT_EXPORT_DIALOG(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_IMPORT_EXPORT_DIALOG, AlmanahImportExportDialog))
#define ALMANAH_IMPORT_EXPORT_DIALOG_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_IMPORT_EXPORT_DIALOG, AlmanahImportExportDialogClass))
#define ALMANAH_IS_IMPORT_EXPORT_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_IMPORT_EXPORT_DIALOG))
#define ALMANAH_IS_IMPORT_EXPORT_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_IMPORT_EXPORT_DIALOG))
#define ALMANAH_IMPORT_EXPORT_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_IMPORT_EXPORT_DIALOG, AlmanahImportExportDialogClass))

typedef struct {
	GtkDialog parent;
} AlmanahImportExportDialog;

typedef struct {
	GtkDialogClass parent;
} AlmanahImportExportDialogClass;

GType almanah_import_export_dialog_get_type (void) G_GNUC_CONST;

AlmanahImportExportDialog *almanah_import_export_dialog_new (AlmanahStorageManager *storage_manager, gboolean import) G_GNUC_WARN_UNUSED_RESULT;

#define ALMANAH_TYPE_IMPORT_RESULTS_DIALOG		(almanah_import_results_dialog_get_type ())
#define ALMANAH_IMPORT_RESULTS_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_IMPORT_RESULTS_DIALOG, AlmanahImportResultsDialog))
#define ALMANAH_IMPORT_RESULTS_DIALOG_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_IMPORT_RESULTS_DIALOG, AlmanahImportResultsDialogClass))
#define ALMANAH_IS_IMPORT_RESULTS_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_IMPORT_RESULTS_DIALOG))
#define ALMANAH_IS_IMPORT_RESULTS_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_IMPORT_RESULTS_DIALOG))
#define ALMANAH_IMPORT_RESULTS_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_IMPORT_RESULTS_DIALOG, AlmanahImportResultsDialogClass))

typedef struct _AlmanahImportResultsDialogPrivate	AlmanahImportResultsDialogPrivate;

typedef struct {
	GtkDialog parent;
	AlmanahImportResultsDialogPrivate *priv;
} AlmanahImportResultsDialog;

typedef struct {
	GtkDialogClass parent;
} AlmanahImportResultsDialogClass;

GType almanah_import_results_dialog_get_type (void) G_GNUC_CONST;

AlmanahImportResultsDialog *almanah_import_results_dialog_new (void) G_GNUC_WARN_UNUSED_RESULT;
void almanah_import_results_dialog_add_result (AlmanahImportResultsDialog *self, const GDate *date, AlmanahImportStatus status, const gchar *message);

G_END_DECLS

#endif /* !ALMANAH_IMPORT_EXPORT_DIALOG_H */
