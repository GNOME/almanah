/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2008-2009 <philip@tecnocode.co.uk>
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
#include "../definition.h"
#include "../interface.h"
#include "../main.h"

static gboolean note_view (AlmanahDefinition *definition);
static void note_build_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox);
static void note_close_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox);
static void note_parse_text (AlmanahDefinition *definition, const gchar *text);
static gchar *note_get_blurb (AlmanahDefinition *definition);

struct _AlmanahNoteDefinitionPrivate {
	GtkWidget *text_view;
};

G_DEFINE_TYPE (AlmanahNoteDefinition, almanah_note_definition, ALMANAH_TYPE_DEFINITION)
#define ALMANAH_NOTE_DEFINITION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_NOTE_DEFINITION, AlmanahNoteDefinitionPrivate))

static void
almanah_note_definition_class_init (AlmanahNoteDefinitionClass *klass)
{
	AlmanahDefinitionClass *definition_class = ALMANAH_DEFINITION_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahNoteDefinitionPrivate));

	definition_class->type_id = ALMANAH_DEFINITION_NOTE;
	definition_class->name = _("Note");
	definition_class->description = _("A note about an important event.");
	definition_class->icon_name = "emblem-important";

	definition_class->view = note_view;
	definition_class->build_dialog = note_build_dialog;
	definition_class->close_dialog = note_close_dialog;
	definition_class->parse_text = note_parse_text;
	definition_class->get_blurb = note_get_blurb;
}

static void
almanah_note_definition_init (AlmanahNoteDefinition *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_NOTE_DEFINITION, AlmanahNoteDefinitionPrivate);
}

static gboolean
note_view (AlmanahDefinition *definition)
{
	const gchar *value = almanah_definition_get_value (definition);

	GtkWidget *dialog = gtk_message_dialog_new (NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				"%s", value);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return TRUE;
}

static void
note_build_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox)
{
	GtkWidget *scrolled_window;
	AlmanahNoteDefinitionPrivate *priv = ALMANAH_NOTE_DEFINITION (definition)->priv;
	const gchar *value = almanah_definition_get_value (definition);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
					     GTK_SHADOW_IN);
	priv->text_view = gtk_text_view_new ();
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (priv->text_view), GTK_WRAP_WORD_CHAR);
	gtk_container_add (GTK_CONTAINER (scrolled_window), priv->text_view);

	/* Initialise the dialogue with the definition's current values */
	if (value != NULL) {
		GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->text_view));
		gtk_text_buffer_set_text (buffer, value, -1);
	}

	gtk_box_pack_start (GTK_BOX (parent_vbox), scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (parent_vbox));
}

static void
note_close_dialog (AlmanahDefinition *definition, GtkVBox *parent_vbox)
{
	AlmanahNoteDefinitionPrivate *priv = ALMANAH_NOTE_DEFINITION (definition)->priv;
	gchar *value;
	GtkTextBuffer *buffer;
	GtkTextIter start_iter, end_iter;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->text_view));
	gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);

	value = gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);
	almanah_definition_set_value (definition, value);
	g_free (value);

	almanah_definition_set_value2 (definition, NULL);
}

static void
note_parse_text (AlmanahDefinition *definition, const gchar *text)
{
	/* TODO */
}

static gchar *
note_get_blurb (AlmanahDefinition *definition)
{
	return g_strdup (almanah_definition_get_value (definition));
}
