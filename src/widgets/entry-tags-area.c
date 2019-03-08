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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Almanah.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "entry-tags-area.h"
#include "entry.h"
#include "tag.h"
#include "tag-entry.h"
#include "storage-manager.h"

enum {
	PROP_ENTRY = 1,
	PROP_STORAGE_MANAGER,
	PROP_BACK_WIDGET
};

typedef struct {
	AlmanahEntry *entry;
	AlmanahStorageManager *storage_manager;
	GtkWidget *back_widget;
	guint tags_number;
	AlmanahTagEntry *tag_entry;
} AlmanahEntryTagsAreaPrivate;

struct _AlmanahEntryTagsArea {
	GtkFlowBox parent;
};

static void almanah_entry_tags_area_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_entry_tags_area_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void almanah_entry_tags_area_finalize     (GObject *object);
static void almanah_entry_tags_area_load_tags    (AlmanahEntryTagsArea *self);
static void almanah_entry_tags_area_update       (AlmanahEntryTagsArea *self);
static void almanah_entry_tags_area_show         (GtkWidget *widget);
static void almanah_entry_tags_area_add_tag      (AlmanahEntryTagsArea *self, const gchar *tag);

/* Signals */
void        tag_entry_activate_cb              (GtkEntry *entry, AlmanahEntryTagsArea *self);
void        entry_tags_area_remove_foreach_cb  (GtkWidget *tag_widget, AlmanahEntryTagsArea *self);
static void tag_remove                         (AlmanahTag *tag_widget, AlmanahEntryTagsArea *self);

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahEntryTagsArea, almanah_entry_tags_area, GTK_TYPE_FLOW_BOX)

static void
almanah_entry_tags_area_class_init (AlmanahEntryTagsAreaClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->get_property = almanah_entry_tags_area_get_property;
	gobject_class->set_property = almanah_entry_tags_area_set_property;
	gobject_class->finalize = almanah_entry_tags_area_finalize;

	widget_class->show = almanah_entry_tags_area_show;

	g_object_class_install_property (gobject_class, PROP_ENTRY,
					 g_param_spec_object ("entry",
							      "Entry", "The entry from which show the tag list",
							      ALMANAH_TYPE_ENTRY,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_STORAGE_MANAGER,
					 g_param_spec_object ("storage-manager",
							      "Storage manager", "The storage manager whose entries should be listed.",
							      ALMANAH_TYPE_STORAGE_MANAGER,
							      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_BACK_WIDGET,
					 g_param_spec_object ("back-widget",
							      "Back Widget", "The widget to grab the focus after a tag was added to an entry.",
							      GTK_TYPE_WIDGET,
							      G_PARAM_READWRITE));
}

static void
almanah_entry_tags_area_init (AlmanahEntryTagsArea *self)
{
	AlmanahEntryTagsAreaPrivate *priv = almanah_entry_tags_area_get_instance_private (self);

	/* There is no tags showed right now. */
	priv->tags_number = 0;

	/* The tag entry widget */
	priv->tag_entry = g_object_new (ALMANAH_TYPE_TAG_ENTRY, NULL);
	gtk_entry_set_text (GTK_ENTRY (priv->tag_entry), _("add tag"));
	gtk_widget_set_tooltip_text (GTK_WIDGET (priv->tag_entry), _("Write the tag and press enter to save it"));
	gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (priv->tag_entry));
	g_signal_connect (priv->tag_entry, "activate", G_CALLBACK (tag_entry_activate_cb), self);
}

static void
almanah_entry_tags_area_finalize (GObject *object)
{
	AlmanahEntryTagsAreaPrivate *priv = almanah_entry_tags_area_get_instance_private (ALMANAH_ENTRY_TAGS_AREA (object));

	g_clear_object (&priv->entry);
	g_clear_object (&priv->storage_manager);
	g_clear_object (&priv->back_widget);

	G_OBJECT_CLASS (almanah_entry_tags_area_parent_class)->finalize (object);
}

static void
almanah_entry_tags_area_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahEntryTagsAreaPrivate *priv = almanah_entry_tags_area_get_instance_private (ALMANAH_ENTRY_TAGS_AREA (object));

	switch (property_id) {
		case PROP_ENTRY:
			g_value_set_object (value, priv->entry);
			break;
		case PROP_STORAGE_MANAGER:
			g_value_set_object (value, priv->storage_manager);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_entry_tags_area_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahEntryTagsAreaPrivate *priv = almanah_entry_tags_area_get_instance_private (ALMANAH_ENTRY_TAGS_AREA (object));

	switch (property_id) {
		case PROP_ENTRY:
			g_clear_object (&priv->entry);
			priv->entry = ALMANAH_ENTRY (g_value_get_object (value));
			g_object_ref (priv->entry);
			almanah_entry_tags_area_update (ALMANAH_ENTRY_TAGS_AREA (object));
			break;
		case PROP_STORAGE_MANAGER:
			g_return_if_fail (ALMANAH_IS_STORAGE_MANAGER (g_value_get_object (value)));
			g_clear_object (&priv->storage_manager);
			priv->storage_manager = ALMANAH_STORAGE_MANAGER (g_value_get_object (value));
			g_object_ref (priv->storage_manager);
			almanah_tag_entry_set_storage_manager (priv->tag_entry, priv->storage_manager);
			break;
		case PROP_BACK_WIDGET:
			g_return_if_fail (GTK_IS_WIDGET (g_value_get_object (value)) && gtk_widget_get_can_focus (g_value_get_object (value)));
			g_clear_object (&priv->back_widget);
			priv->back_widget = GTK_WIDGET (g_value_get_object (value));
			g_object_ref (priv->back_widget);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_entry_tags_area_load_tags (AlmanahEntryTagsArea *self)
{
	AlmanahEntryTagsAreaPrivate *priv = almanah_entry_tags_area_get_instance_private (self);
	GList *tags;

	tags = almanah_storage_manager_entry_get_tags (priv->storage_manager, priv->entry);
	while (tags) {
		almanah_entry_tags_area_add_tag (self, (const gchar*) tags->data);

		g_free (tags->data);
		tags = g_list_next (tags);
	}

	g_free (tags);
	gtk_widget_show_all (GTK_WIDGET (self));
}

static void
almanah_entry_tags_area_update (AlmanahEntryTagsArea *self)
{
	/* Update the tags area removing all tag widgets first */
	gtk_container_foreach (GTK_CONTAINER (self), (GtkCallback) entry_tags_area_remove_foreach_cb, self);
}

static void
almanah_entry_tags_area_show (GtkWidget *widget)
{
	AlmanahEntryTagsArea *self = ALMANAH_ENTRY_TAGS_AREA (widget);
	AlmanahEntryTagsAreaPrivate *priv = almanah_entry_tags_area_get_instance_private (self);

	gtk_widget_show (GTK_WIDGET (priv->tag_entry));
	GTK_WIDGET_CLASS (almanah_entry_tags_area_parent_class)->show (widget);
}

static void
almanah_entry_tags_area_add_tag (AlmanahEntryTagsArea *self, const gchar *tag)
{
	AlmanahEntryTagsAreaPrivate *priv = almanah_entry_tags_area_get_instance_private (self);
	GtkWidget *tag_widget;

	tag_widget = almanah_tag_new (tag);
	gtk_container_add (GTK_CONTAINER (self), tag_widget);
	gtk_widget_show (tag_widget);
	g_signal_connect (tag_widget, "remove", G_CALLBACK (tag_remove), self);

	priv->tags_number++;
}

void
tag_entry_activate_cb (GtkEntry *entry, AlmanahEntryTagsArea *self)
{
	AlmanahEntryTagsAreaPrivate *priv = almanah_entry_tags_area_get_instance_private (self);
	gchar *tag;

	tag = g_strdup (gtk_entry_get_text (entry));
	gtk_entry_set_text (entry, "");
	if (almanah_storage_manager_entry_add_tag (priv->storage_manager, priv->entry, tag)) {
		/* @TODO: turn this async waiting for the storage signal "tag-added" */
		almanah_entry_tags_area_add_tag (self, (const gchar*) tag);
		gtk_widget_grab_focus (priv->back_widget);
	} else {
		g_debug ("Can't add the tag");
	}
	g_free (tag);
}

void
entry_tags_area_remove_foreach_cb (GtkWidget *tag_widget, AlmanahEntryTagsArea *self)
{
	AlmanahEntryTagsAreaPrivate *priv = almanah_entry_tags_area_get_instance_private (self);

	/* Remove all the tag widget */
	if (ALMANAH_IS_TAG (tag_widget)) {
		gtk_widget_destroy (tag_widget);
		priv->tags_number--;
	}

	/* Show the tags for the entry when no remains tag widgets */
	if (priv->tags_number == 0) {
		almanah_entry_tags_area_load_tags (self);
	}
}

void
almanah_entry_tags_area_set_entry (AlmanahEntryTagsArea *entry_tags_area, AlmanahEntry *entry)
{
	GValue entry_value = G_VALUE_INIT;

	g_return_if_fail (ALMANAH_IS_ENTRY_TAGS_AREA (entry_tags_area));
	g_return_if_fail (ALMANAH_IS_ENTRY (entry));

	g_value_init (&entry_value, G_TYPE_OBJECT);
	g_value_set_object (&entry_value, entry);
	g_object_set_property (G_OBJECT (entry_tags_area), "entry", &entry_value);
	g_value_unset (&entry_value);
}

static void
tag_remove (AlmanahTag *tag_widget, AlmanahEntryTagsArea *self)
{
	AlmanahEntryTagsAreaPrivate *priv = almanah_entry_tags_area_get_instance_private (self);

	if (almanah_storage_manager_entry_remove_tag (priv->storage_manager, priv->entry, almanah_tag_get_tag (tag_widget))) {
		gtk_widget_destroy (GTK_WIDGET (tag_widget));
		priv->tags_number--;
	} else {
		g_debug ("Can't remove the tag");
	}
}

/**
 * @TODO: Document
 */
void
almanah_entry_tags_area_set_storage_manager (AlmanahEntryTagsArea *entry_tags_area, AlmanahStorageManager *storage_manager)
{
	GValue storage_value = G_VALUE_INIT;

	g_return_if_fail (ALMANAH_IS_ENTRY_TAGS_AREA (entry_tags_area));

	g_value_init (&storage_value, G_TYPE_OBJECT);
	g_value_set_object (&storage_value, storage_manager);
	g_object_set_property (G_OBJECT (entry_tags_area), "storage-manager", &storage_value);
	g_value_unset (&storage_value);
}

/**
 * @TODO: Document
 */
void
almanah_entry_tags_area_set_back_widget (AlmanahEntryTagsArea *entry_tags_area, GtkWidget *back_widget)
{
	GValue back_w_value = G_VALUE_INIT;

	g_return_if_fail (ALMANAH_IS_ENTRY_TAGS_AREA (entry_tags_area));

	g_value_init (&back_w_value, G_TYPE_OBJECT);
	g_value_set_object (&back_w_value, back_widget);
	g_object_set_property (G_OBJECT (entry_tags_area), "back-widget", &back_w_value);
	g_value_unset (&back_w_value);
}
