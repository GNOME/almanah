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

#include "entry-tags-area.h"
#include "tag.h"
#include "tag-entry.h"
#include "entry.h"
#include "storage-manager.h"

enum {
        PROP_ENTRY = 1,
        PROP_STORAGE_MANAGER
};

struct _AlmanahEntryTagsAreaPrivate {
        AlmanahEntry *entry;
        AlmanahStorageManager *storage_manager;
        guint tags_number;
	AlmanahTagEntry *tag_entry;
};

static void almanah_entry_tags_area_get_property      (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_entry_tags_area_set_property      (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void almanah_entry_tags_area_finalize          (GObject *object);
static void almanah_entry_tags_area_load_tags         (AlmanahEntryTagsArea *self);
static void almanah_entry_tags_area_update            (AlmanahEntryTagsArea *self);
static gint almanah_entry_tags_area_draw              (GtkWidget *widget, cairo_t *cr);

/* Signals */
void tag_entry_activate_cb              (GtkEntry *entry, AlmanahEntryTagsArea *self);
void entry_tags_area_remove_foreach_cb  (GtkWidget *tag_widget, AlmanahEntryTagsArea *self);
void storage_manager_entry_tag_added_cb (AlmanahEntry *entry, gchar *tag,  AlmanahEntryTagsArea *self);

G_DEFINE_TYPE (AlmanahEntryTagsArea, almanah_entry_tags_area, GTK_TYPE_GRID)

static void
almanah_entry_tags_area_class_init (AlmanahEntryTagsAreaClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        g_type_class_add_private (klass, sizeof (AlmanahEntryTagsAreaPrivate));

        gobject_class->get_property = almanah_entry_tags_area_get_property;
        gobject_class->set_property = almanah_entry_tags_area_set_property;
        gobject_class->finalize = almanah_entry_tags_area_finalize;

	widget_class->draw = almanah_entry_tags_area_draw;

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
}

static void
almanah_entry_tags_area_init (AlmanahEntryTagsArea *self)
{
        self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_ENTRY_TAGS_AREA, AlmanahEntryTagsAreaPrivate);

        /* There is no tags showed right now. */
        self->priv->tags_number = 0;

	/* The tag entry widget */
	self->priv->tag_entry = g_object_new (ALMANAH_TYPE_TAG_ENTRY, NULL);
	gtk_entry_set_text (GTK_ENTRY (self->priv->tag_entry), "add tag");
	gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->priv->tag_entry));
	g_signal_connect (self->priv->tag_entry, "activate", G_CALLBACK (tag_entry_activate_cb), self);
}

static void
almanah_entry_tags_area_finalize (GObject *object)
{
        AlmanahEntryTagsAreaPrivate *priv = ALMANAH_ENTRY_TAGS_AREA (object)->priv;

        g_clear_object (&priv->entry);
        g_clear_object (&priv->storage_manager);

        G_OBJECT_CLASS (almanah_entry_tags_area_parent_class)->finalize (object);
}

static void
almanah_entry_tags_area_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
        AlmanahEntryTagsAreaPrivate *priv = ALMANAH_ENTRY_TAGS_AREA (object)->priv;

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
        AlmanahEntryTagsAreaPrivate *priv = ALMANAH_ENTRY_TAGS_AREA (object)->priv;

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
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                        break;
        }        
}

static void
almanah_entry_tags_area_load_tags (AlmanahEntryTagsArea *self)
{
	GList *tags;

	tags = almanah_storage_manager_entry_get_tags (self->priv->storage_manager, self->priv->entry);
	while (tags) {
		GtkWidget *tag_widget = almanah_tag_new (tags->data);
		gtk_container_add (GTK_CONTAINER (self), tag_widget);
		self->priv->tags_number++;

		g_free (tags->data);
		tags = g_list_next (tags);
	}

	g_free (tags);
	gtk_widget_show_all (GTK_WIDGET (self));
}

static void
almanah_entry_tags_area_update (AlmanahEntryTagsArea *self)
{
        gtk_container_foreach (GTK_CONTAINER (self), (GtkCallback) entry_tags_area_remove_foreach_cb, self);
}

static gint
almanah_entry_tags_area_draw (GtkWidget *widget, cairo_t *cr)
{
	gint width, height;

	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	return GTK_WIDGET_CLASS (almanah_entry_tags_area_parent_class)->draw (widget, cr);
}

void
tag_entry_activate_cb (GtkEntry *entry, AlmanahEntryTagsArea *self)
{
	gboolean result;
	gchar *tag;

	tag = g_strdup (gtk_entry_get_text (entry));
	gtk_entry_set_text (entry, "");
	if (almanah_storage_manager_entry_add_tag (self->priv->storage_manager, self->priv->entry, tag)) {
		GtkWidget *tag_widget = almanah_tag_new (tag);
		gtk_container_add (GTK_CONTAINER (self), tag_widget);
		self->priv->tags_number++;
		gtk_widget_show (tag_widget);
	}
	g_free (tag);

	/* @TODO: Return the focus to the GtkTextView */
}

void
entry_tags_area_remove_foreach_cb (GtkWidget *tag_widget, AlmanahEntryTagsArea *self)
{
        if (ALMANAH_IS_TAG (tag_widget)) {
                gtk_widget_destroy (tag_widget);
                self->priv->tags_number--;
        }

        /* Show the tags for the entry */
        if (self->priv->tags_number == 0) {
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

void
storage_manager_entry_tag_added_cb (AlmanahEntry *entry, gchar *tag, AlmanahEntryTagsArea *self)
{
	GtkWidget *tag_widget;

	/* TODO: test if the priv->entry == entry */

}

void
almanah_entry_tags_area_set_storage_manager (AlmanahEntryTagsArea *entry_tags_area, AlmanahStorageManager *storage_manager)
{
	GValue storage_value = G_VALUE_INIT;

	g_return_if_fail (ALMANAH_IS_ENTRY_TAGS_AREA (entry_tags_area));
	g_return_if_fail (ALMANAH_IS_STORAGE_MANAGER (storage_manager));

	g_value_init (&storage_value, G_TYPE_OBJECT);
	g_value_set_object (&storage_value, storage_manager);
	g_object_set_property (G_OBJECT (entry_tags_area), "storage-manager", &storage_value);
	g_value_unset (&storage_value);
}
