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

#include <glib.h>
#include <glib/gi18n.h>

#include "note.h"
#include "../link.h"
#include "../interface.h"
#include "../main.h"

static void almanah_note_link_init (AlmanahNoteLink *self);
static gchar *note_format_value (AlmanahLink *link);
static gboolean note_view (AlmanahLink *link);
static void note_build_dialog (AlmanahLink *link, GtkVBox *parent_vbox);
static void note_get_values (AlmanahLink *link);

struct _AlmanahNoteLinkPrivate {
	GtkWidget *text_view;
};

G_DEFINE_TYPE (AlmanahNoteLink, almanah_note_link, ALMANAH_TYPE_LINK)
#define ALMANAH_NOTE_LINK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_NOTE_LINK, AlmanahNoteLinkPrivate))

static void
almanah_note_link_class_init (AlmanahNoteLinkClass *klass)
{
	AlmanahLinkClass *link_class = ALMANAH_LINK_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahNoteLinkPrivate));

	link_class->type_id = "note";
	link_class->name = _("Note");
	link_class->description = _("A note about an important event.");
	link_class->icon_name = "emblem-important";

	link_class->format_value = note_format_value;
	link_class->view = note_view;
	link_class->build_dialog = note_build_dialog;
	link_class->get_values = note_get_values;
}

static void
almanah_note_link_init (AlmanahNoteLink *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_NOTE_LINK, AlmanahNoteLinkPrivate);
}

static gchar *
note_format_value (AlmanahLink *link)
{
	return g_strdup (almanah_link_get_value (link));
}

static gboolean
note_view (AlmanahLink *link)
{
	const gchar *value = almanah_link_get_value (link);

	GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (almanah->main_window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				"%s", value);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return TRUE;
}

static void
note_build_dialog (AlmanahLink *link, GtkVBox *parent_vbox)
{
	GtkWidget *scrolled_window;
	AlmanahNoteLinkPrivate *priv = ALMANAH_NOTE_LINK (link)->priv;

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
					     GTK_SHADOW_IN);
	priv->text_view = gtk_text_view_new ();
	gtk_container_add (GTK_CONTAINER (scrolled_window), priv->text_view);

	gtk_box_pack_start (GTK_BOX (parent_vbox), scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (parent_vbox));
}

static void
note_get_values (AlmanahLink *link)
{
	gchar *value;
	GtkTextBuffer *buffer;
	GtkTextIter start_iter, end_iter;
	AlmanahNoteLinkPrivate *priv = ALMANAH_NOTE_LINK (link)->priv;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->text_view));
	gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);

	value = gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);
	almanah_link_set_value (link, value);
	g_free (value);

	almanah_link_set_value2 (link, NULL);
}
