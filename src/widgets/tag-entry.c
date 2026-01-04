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

#include "storage-manager.h"
#include "tag-entry.h"

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

static void almanah_tag_entry_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_tag_entry_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void almanah_tag_entry_finalize (GObject *object);
static void almanah_tag_entry_update_tags (AlmanahTagEntry *tag_entry);
static gboolean almanah_tag_entry_match_selected (GtkEntryCompletion *widget, GtkTreeModel *model, GtkTreeIter *iter, AlmanahTagEntry *self);

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahTagEntry, almanah_tag_entry, GTK_TYPE_ENTRY)

static void
almanah_tag_entry_class_init (AlmanahTagEntryClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = almanah_tag_entry_get_property;
	gobject_class->set_property = almanah_tag_entry_set_property;
	gobject_class->finalize = almanah_tag_entry_finalize;

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

	AlmanahTagEntryPrivate *priv = almanah_tag_entry_get_instance_private (self);

	gtk_entry_set_placeholder_text (GTK_ENTRY (self), _ ("add tag"));

	priv->tags_store = gtk_list_store_new (1, G_TYPE_STRING);
	completion = gtk_entry_completion_new ();
	gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (priv->tags_store));
	gtk_entry_completion_set_text_column (completion, 0);
	gtk_entry_set_completion (GTK_ENTRY (self), completion);
	g_signal_connect (completion, "match-selected", G_CALLBACK (almanah_tag_entry_match_selected), self);

	gtk_accessible_update_property (GTK_ACCESSIBLE (self),
	                                GTK_ACCESSIBLE_PROPERTY_LABEL,
	                                _ ("Tag entry"),
	                                -1);
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
	g_autoptr (GPtrArray) tags = NULL;
	GtkTreeIter iter;
	AlmanahTagEntryPrivate *priv = almanah_tag_entry_get_instance_private (tag_entry);

	gtk_list_store_clear (priv->tags_store);
	tags = almanah_storage_manager_get_tags (priv->storage_manager);

	if (tags == NULL) {
		return;
	}

	for (guint i = 0; i < tags->len; i++) {
		gchar *tag = g_ptr_array_index (tags, i);
		gtk_list_store_append (priv->tags_store, &iter);
		gtk_list_store_set (priv->tags_store, &iter, 0, tag, -1);
	}
}

static gboolean
almanah_tag_entry_match_selected (GtkEntryCompletion *widget, GtkTreeModel *model, GtkTreeIter *iter, AlmanahTagEntry *self)
{
	gchar *tag;

	gtk_tree_model_get (model, iter, 0, &tag, -1);
	gtk_editable_set_text (GTK_EDITABLE (self), tag);
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
