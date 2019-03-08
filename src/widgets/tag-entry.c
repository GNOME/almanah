/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Álvaro Peña 2013 <alvaropg@gmail.com>
 *
 * Almanah is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Almanah is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Almanah.	If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "tag-entry.h"
#include "storage-manager.h"

enum {
	PROP_STORAGE_MANAGER = 1
};

typedef struct {
	GtkListStore *tags_store;
	AlmanahStorageManager *storage_manager;
} AlmanahTagEntryPrivate;

struct _AlmanahTagEntry {
	GtkEntry parent;
};

static void almanah_tag_entry_get_property	  (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_tag_entry_set_property	  (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void almanah_tag_entry_finalize		  (GObject *object);
static void almanah_tag_entry_update_tags	  (AlmanahTagEntry *tag_entry);
static void almanah_tag_entry_get_preferred_width (GtkWidget *widget, gint *minimum, gint *natural);
gboolean    almanah_tag_entry_focus_out_event	  (GtkWidget *self, GdkEventFocus *event);
gboolean    almanah_tag_entry_focus_in_event	  (GtkWidget *self, GdkEventFocus *event);
gboolean    almanah_tag_entry_match_selected	  (GtkEntryCompletion *widget, GtkTreeModel *model, GtkTreeIter *iter, AlmanahTagEntry *self);

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahTagEntry, almanah_tag_entry, GTK_TYPE_ENTRY)

static void
almanah_tag_entry_class_init (AlmanahTagEntryClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->get_property = almanah_tag_entry_get_property;
	gobject_class->set_property = almanah_tag_entry_set_property;
	gobject_class->finalize = almanah_tag_entry_finalize;

	gtkwidget_class->focus_out_event = almanah_tag_entry_focus_out_event;
	gtkwidget_class->focus_in_event = almanah_tag_entry_focus_in_event;
	gtkwidget_class->get_preferred_width = almanah_tag_entry_get_preferred_width;

	g_object_class_install_property (gobject_class, PROP_STORAGE_MANAGER,
					 g_param_spec_object ("storage-manager",
							      "Storage manager", "The storage manager whose entries should be listed.",
							      ALMANAH_TYPE_STORAGE_MANAGER,
							      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_tag_entry_init (AlmanahTagEntry *self)
{
	GtkEntryCompletion *completion;
	AtkObject *self_atk_object;

	AlmanahTagEntryPrivate *priv = almanah_tag_entry_get_instance_private (self);

	priv->tags_store = gtk_list_store_new (1, G_TYPE_STRING);
	completion = gtk_entry_completion_new ();
	gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (priv->tags_store));
	gtk_entry_completion_set_text_column (completion, 0);
	gtk_entry_set_completion (GTK_ENTRY (self), completion);
	g_signal_connect (completion, "match-selected", G_CALLBACK (almanah_tag_entry_match_selected), self);

	self_atk_object = gtk_widget_get_accessible (GTK_WIDGET (self));
	atk_object_set_name (self_atk_object, _("Tag entry"));
}

static void
almanah_tag_entry_finalize (GObject *object)
{
	AlmanahTagEntryPrivate *priv = almanah_tag_entry_get_instance_private (ALMANAH_TAG_ENTRY (object));

	g_clear_object (&priv->storage_manager);

	G_OBJECT_CLASS (almanah_tag_entry_parent_class)->finalize (object);
}

static void
almanah_tag_entry_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahTagEntryPrivate *priv = almanah_tag_entry_get_instance_private (ALMANAH_TAG_ENTRY (object));

	switch (property_id) {
	case PROP_STORAGE_MANAGER:
		g_value_set_object (value, priv->storage_manager);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
almanah_tag_entry_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahTagEntryPrivate *priv = almanah_tag_entry_get_instance_private (ALMANAH_TAG_ENTRY (object));

	switch (property_id) {
	case PROP_STORAGE_MANAGER:
		g_clear_object (&priv->storage_manager);
		priv->storage_manager = ALMANAH_STORAGE_MANAGER (g_value_get_object (value));
		g_object_ref (priv->storage_manager);
		almanah_tag_entry_update_tags (ALMANAH_TAG_ENTRY (object));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
almanah_tag_entry_update_tags (AlmanahTagEntry *tag_entry)
{
	GList *tags;
	GtkTreeIter iter;
	AlmanahTagEntryPrivate *priv = almanah_tag_entry_get_instance_private (tag_entry);

	gtk_list_store_clear (priv->tags_store);
	tags = almanah_storage_manager_get_tags (priv->storage_manager);
	while (tags) {
		gtk_list_store_append (priv->tags_store, &iter);
		gtk_list_store_set (priv->tags_store, &iter, 0, tags->data, -1);

		tags = g_list_next (tags);
	}

	if (tags)
		g_list_free (tags);
}

static void
almanah_tag_entry_get_preferred_width (GtkWidget *widget, gint *minimum, gint *natural)
{
	gint m_width, n_width;

	GTK_WIDGET_CLASS (almanah_tag_entry_parent_class)->get_preferred_width (widget, &m_width, &n_width);

	/* Just a bad hack... @TODO: set the width to a maximun number of characters, using the pango layout */
	*minimum = m_width - 100;
	*natural = n_width - 50;
}

gboolean
almanah_tag_entry_focus_out_event (GtkWidget *self, GdkEventFocus *event)
{
	gtk_entry_set_text (GTK_ENTRY (self), _("add tag"));

	return FALSE;
}

gboolean
almanah_tag_entry_focus_in_event (GtkWidget *self, GdkEventFocus *event)
{
	gtk_entry_set_text (GTK_ENTRY (self), "");

	return FALSE;
}

gboolean
almanah_tag_entry_match_selected (GtkEntryCompletion *widget, GtkTreeModel *model, GtkTreeIter *iter, AlmanahTagEntry *self)
{
	gchar *tag;

	gtk_tree_model_get (model, iter, 0, &tag, -1);
	gtk_entry_set_text (GTK_ENTRY (self), tag);
	g_signal_emit_by_name (GTK_ENTRY (self), "activate");

	return TRUE;
}

/* @TODO: Remove? use g_object_set */
void
almanah_tag_entry_set_storage_manager (AlmanahTagEntry *tag_entry, AlmanahStorageManager *storage_manager)
{
	GValue storage_value = G_VALUE_INIT;

	g_return_if_fail (ALMANAH_IS_TAG_ENTRY (tag_entry));
	g_return_if_fail (ALMANAH_IS_STORAGE_MANAGER (storage_manager));

	g_value_init (&storage_value, G_TYPE_OBJECT);
	g_value_set_object (&storage_value, storage_manager);
	g_object_set_property (G_OBJECT (tag_entry), "storage-manager", &storage_value);
	g_value_unset (&storage_value);
}
