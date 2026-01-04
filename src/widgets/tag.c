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

#include "gtk/gtk.h"
#include "tag.h"

enum {
	PROP_TAG = 1
};

typedef struct {
	gchar *tag;
	GtkLabel *label;
} AlmanahTagPrivate;

struct _AlmanahTag {
	GtkBox parent;
};

enum {
	SIGNAL_REMOVE,
	LAST_SIGNAL
};

static guint tag_signals[LAST_SIGNAL] = {
	0,
};

static void almanah_tag_finalize (GObject *object);
static void almanah_tag_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_tag_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void almanah_tag_button_clicked_cb (GtkButton *self, gpointer user_data);

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahTag, almanah_tag, GTK_TYPE_BOX)

static void
almanah_tag_class_init (AlmanahTagClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = almanah_tag_get_property;
	gobject_class->set_property = almanah_tag_set_property;
	gobject_class->finalize = almanah_tag_finalize;

	g_object_class_install_property (gobject_class, PROP_TAG,
	                                 g_param_spec_string ("tag",
	                                                      "Tag", "The tag name.",
	                                                      NULL, G_PARAM_READWRITE));

	tag_signals[SIGNAL_REMOVE] = g_signal_new ("remove",
	                                           G_TYPE_FROM_CLASS (klass),
	                                           G_SIGNAL_RUN_LAST,
	                                           0, NULL, NULL,
	                                           g_cclosure_marshal_VOID__VOID,
	                                           G_TYPE_NONE, 0);
}

static void
almanah_tag_init (AlmanahTag *self)
{
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (self);

	// GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	gtk_widget_set_hexpand (GTK_WIDGET (self), FALSE);
	gtk_widget_add_css_class (GTK_WIDGET (self), "tag");

	GtkWidget *label = gtk_label_new (priv->tag);
	GtkWidget *close_btn = gtk_button_new_from_icon_name ("window-close-symbolic");

	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_hexpand (label, TRUE);

	gtk_widget_add_css_class (close_btn, "flat");
	gtk_widget_add_css_class (close_btn, "circular");
	gtk_widget_set_tooltip_text (close_btn, _ ("Remove tag"));

	gtk_box_append (GTK_BOX (self), label);
	gtk_box_append (GTK_BOX (self), close_btn);

	g_signal_connect_swapped (close_btn, "clicked", G_CALLBACK (almanah_tag_button_clicked_cb), self);

	// gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);
}

static void
almanah_tag_finalize (GObject *object)
{
	AlmanahTag *tag = ALMANAH_TAG (object);
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (tag);

	g_free (priv->tag);

	G_OBJECT_CLASS (almanah_tag_parent_class)->finalize (object);
}

static void
almanah_tag_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahTag *tag = ALMANAH_TAG (object);
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (tag);

	switch (property_id) {
		case PROP_TAG:
			g_value_set_string (value, priv->tag);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_tag_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahTag *tag = ALMANAH_TAG (object);
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (tag);

	switch (property_id) {
		case PROP_TAG:
			if (priv->tag) {
				g_free (priv->tag);
			}
			priv->tag = g_strdup (g_value_get_string (value));
			gtk_label_set_text (priv->label, priv->tag);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_tag_button_clicked_cb (GtkButton *self, gpointer user_data)
{
	AlmanahTag *tag = ALMANAH_TAG (user_data);

	g_signal_emit (tag, tag_signals[SIGNAL_REMOVE], 0);
}

GtkWidget *
almanah_tag_new (const gchar *tag)
{
	return GTK_WIDGET (g_object_new (ALMANAH_TYPE_TAG,
	                                 "tag", tag,
	                                 NULL));
}

const gchar *
almanah_tag_get_tag (AlmanahTag *tag_widget)
{
	g_return_val_if_fail (ALMANAH_IS_TAG (tag_widget), NULL);

	AlmanahTag *tag = ALMANAH_TAG (tag_widget);
	AlmanahTagPrivate *priv = almanah_tag_get_instance_private (tag);

	return priv->tag;
}

void
almanah_tag_remove (AlmanahTag *tag_widget)
{
	g_return_if_fail (ALMANAH_IS_TAG (tag_widget));

	g_signal_emit (tag_widget, tag_signals[SIGNAL_REMOVE], 0);
}
