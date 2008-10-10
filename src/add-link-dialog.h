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

#ifndef ALMANAH_ADD_LINK_DIALOG_H
#define ALMANAH_ADD_LINK_DIALOG_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "link.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_ADD_LINK_DIALOG		(almanah_add_link_dialog_get_type ())
#define ALMANAH_ADD_LINK_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ALMANAH_TYPE_ADD_LINK_DIALOG, AlmanahAddLinkDialog))
#define ALMANAH_ADD_LINK_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), ALMANAH_TYPE_ADD_LINK_DIALOG, AlmanahAddLinkDialogClass))
#define ALMANAH_IS_ADD_LINK_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ALMANAH_TYPE_ADD_LINK_DIALOG))
#define ALMANAH_IS_ADD_LINK_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ALMANAH_TYPE_ADD_LINK_DIALOG))
#define ALMANAH_ADD_LINK_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ALMANAH_TYPE_ADD_LINK_DIALOG, AlmanahAddLinkDialogClass))

typedef struct _AlmanahAddLinkDialogPrivate	AlmanahAddLinkDialogPrivate;

typedef struct {
	GtkDialog parent;
	AlmanahAddLinkDialogPrivate *priv;
} AlmanahAddLinkDialog;

typedef struct {
	GtkDialogClass parent;
} AlmanahAddLinkDialogClass;

GType almanah_add_link_dialog_get_type (void);
AlmanahAddLinkDialog *almanah_add_link_dialog_new (void);
AlmanahLink *almanah_add_link_dialog_get_link (AlmanahAddLinkDialog *self);

G_END_DECLS

#endif /* !ALMANAH_ADD_LINK_DIALOG_H */
