/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2009 <philip@tecnocode.co.uk>
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

#ifndef ALMANAH_DATE_ENTRY_DIALOG_H
#define ALMANAH_DATE_ENTRY_DIALOG_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ALMANAH_TYPE_DATE_ENTRY_DIALOG		(almanah_date_entry_dialog_get_type ())
#define ALMANAH_DATE_ENTRY_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_DATE_ENTRY_DIALOG, AlmanahDateEntryDialog))
#define ALMANAH_DATE_ENTRY_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_DATE_ENTRY_DIALOG, AlmanahDateEntryDialogClass))
#define ALMANAH_IS_DATE_ENTRY_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_DATE_ENTRY_DIALOG))
#define ALMANAH_IS_DATE_ENTRY_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_DATE_ENTRY_DIALOG))
#define ALMANAH_DATE_ENTRY_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_DATE_ENTRY_DIALOG, AlmanahDateEntryDialogClass))

typedef struct {
	GtkDialog parent;
} AlmanahDateEntryDialog;

typedef struct {
	GtkDialogClass parent;
} AlmanahDateEntryDialogClass;

GType almanah_date_entry_dialog_get_type (void);

AlmanahDateEntryDialog *almanah_date_entry_dialog_new (void);
gboolean almanah_date_entry_dialog_run (AlmanahDateEntryDialog *self);

void almanah_date_entry_dialog_get_date (AlmanahDateEntryDialog *self, GDate *date);
void almanah_date_entry_dialog_set_date (AlmanahDateEntryDialog *self, GDate *date);

G_END_DECLS

#endif /* !ALMANAH_DATE_ENTRY_DIALOG_H */
