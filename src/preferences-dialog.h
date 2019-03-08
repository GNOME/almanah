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

#ifndef ALMANAH_PREFERENCES_DIALOG_H
#define ALMANAH_PREFERENCES_DIALOG_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define ALMANAH_TYPE_PREFERENCES_DIALOG		(almanah_preferences_dialog_get_type ())
#define ALMANAH_PREFERENCES_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_PREFERENCES_DIALOG, AlmanahPreferencesDialog))
#define ALMANAH_PREFERENCES_DIALOG_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_PREFERENCES_DIALOG, AlmanahPreferencesDialogClass))
#define ALMANAH_IS_PREFERENCES_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_PREFERENCES_DIALOG))
#define ALMANAH_IS_PREFERENCES_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_PREFERENCES_DIALOG))
#define ALMANAH_PREFERENCES_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_PREFERENCES_DIALOG, AlmanahPreferencesDialogClass))

typedef struct {
	GtkDialog parent;
} AlmanahPreferencesDialog;

typedef struct {
	GtkDialogClass parent;
} AlmanahPreferencesDialogClass;

GType almanah_preferences_dialog_get_type (void);
AlmanahPreferencesDialog *almanah_preferences_dialog_new (GSettings *settings) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

G_END_DECLS

#endif /* !ALMANAH_PREFERENCES_DIALOG_H */
