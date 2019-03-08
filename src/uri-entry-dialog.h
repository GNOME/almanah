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

#ifndef ALMANAH_URI_ENTRY_DIALOG_H
#define ALMANAH_URI_ENTRY_DIALOG_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ALMANAH_TYPE_URI_ENTRY_DIALOG		(almanah_uri_entry_dialog_get_type ())
#define ALMANAH_URI_ENTRY_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_URI_ENTRY_DIALOG, AlmanahUriEntryDialog))
#define ALMANAH_URI_ENTRY_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_URI_ENTRY_DIALOG, AlmanahUriEntryDialogClass))
#define ALMANAH_IS_URI_ENTRY_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_URI_ENTRY_DIALOG))
#define ALMANAH_IS_URI_ENTRY_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_URI_ENTRY_DIALOG))
#define ALMANAH_URI_ENTRY_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_URI_ENTRY_DIALOG, AlmanahUriEntryDialogClass))

typedef struct {
	GtkDialog parent;
} AlmanahUriEntryDialog;

typedef struct {
	GtkDialogClass parent;
} AlmanahUriEntryDialogClass;

GType almanah_uri_entry_dialog_get_type (void) G_GNUC_CONST;

AlmanahUriEntryDialog *almanah_uri_entry_dialog_new (void) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;
gboolean almanah_uri_entry_dialog_run (AlmanahUriEntryDialog *self);

const gchar *almanah_uri_entry_dialog_get_uri (AlmanahUriEntryDialog *self) G_GNUC_PURE;
void almanah_uri_entry_dialog_set_uri (AlmanahUriEntryDialog *self, const gchar *uri);

G_END_DECLS

#endif /* !ALMANAH_URI_ENTRY_DIALOG_H */
