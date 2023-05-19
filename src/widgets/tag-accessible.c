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

#include "tag-accessible.h"
#include "tag.h"

struct _AlmanahTagAccessible {
	GtkWidgetAccessible parent;
};

static void  almanah_tag_accessible_initialize                    (AtkObject *obj, gpointer data);

static const gchar* almanah_tag_accessible_get_name               (AtkObject *accessible);

static void  almanah_tag_accessible_atk_action_iface_init         (AtkActionIface *iface);
gboolean     almanah_tag_accessible_atk_action_do_action          (AtkAction *action, gint i);
gint         almanah_tag_accessible_atk_action_get_n_actions      (AtkAction *action);
const gchar* almanah_tag_accessible_atk_action_get_description    (AtkAction *action, gint i);
const gchar* almanah_tag_accessible_atk_action_get_name           (AtkAction *action, gint i);
const gchar* almanah_tag_accessible_atk_action_get_keybinding     (AtkAction *action, gint i);
gboolean     almanah_tag_accessible_atk_action_set_description    (AtkAction *action, gint i, const gchar *desc);
const gchar* almanah_tag_accessible_atk_action_get_localized_name (AtkAction *action, gint i);

G_DEFINE_TYPE_WITH_CODE (AlmanahTagAccessible, almanah_tag_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, almanah_tag_accessible_atk_action_iface_init))

static void
almanah_tag_accessible_class_init (AlmanahTagAccessibleClass *klass)
{
        AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	class->get_name = almanah_tag_accessible_get_name;
        class->initialize = almanah_tag_accessible_initialize;
}

static void
almanah_tag_accessible_init (AlmanahTagAccessible *self)
{
}

static void
almanah_tag_accessible_initialize (AtkObject *obj, gpointer data)
{
        ATK_OBJECT_CLASS (almanah_tag_accessible_parent_class)->initialize (obj, data);

        obj->role = ATK_ROLE_DRAWING_AREA;
}

/* Code adapted from gtklabelaccessible in GTK+ project */
static const gchar*
almanah_tag_accessible_get_name (AtkObject *accessible)
{
	const gchar *name;

	g_return_val_if_fail (ALMANAH_IS_TAG_ACCESSIBLE (accessible), NULL);

	name = ATK_OBJECT_CLASS (almanah_tag_accessible_parent_class)->get_name (accessible);
	if (name != NULL)
		return name;
	else {
		/* Get the text on the tag */
		GtkWidget *widget;

		widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
		g_return_val_if_fail (widget != NULL, NULL);
		g_return_val_if_fail (ALMANAH_IS_TAG (widget), NULL);

		return almanah_tag_get_tag (ALMANAH_TAG (widget));
	}
}

static void
almanah_tag_accessible_atk_action_iface_init (AtkActionIface *iface)
{
	iface->do_action = almanah_tag_accessible_atk_action_do_action;
	iface->get_n_actions = almanah_tag_accessible_atk_action_get_n_actions;
	iface->get_description = almanah_tag_accessible_atk_action_get_description;
	iface->get_name = almanah_tag_accessible_atk_action_get_name;
	iface->get_keybinding = almanah_tag_accessible_atk_action_get_keybinding;
	iface->set_description = almanah_tag_accessible_atk_action_set_description;
	iface->get_localized_name = almanah_tag_accessible_atk_action_get_localized_name;
}

gboolean
almanah_tag_accessible_atk_action_do_action (AtkAction *action, gint i)
{
        GtkWidget *widget;

        widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
        g_return_val_if_fail (widget != NULL, FALSE);

	if (i == 0) {
		almanah_tag_remove (ALMANAH_TAG (widget));
		return TRUE;
	} else
		return FALSE;
}

gint
almanah_tag_accessible_atk_action_get_n_actions (AtkAction *action)
{
	return 1;
}

const gchar*
almanah_tag_accessible_atk_action_get_description (AtkAction *action, gint i)
{
	if (i == 0)
		return "Remove the tag from the entry";
	else
		return NULL;
}

const gchar*
almanah_tag_accessible_atk_action_get_name (AtkAction *action, gint i)
{
	if (i == 0)
		return "remove";
	else
		return NULL;
}

const gchar*
almanah_tag_accessible_atk_action_get_keybinding (AtkAction *action, gint i)
{
	if (i == 0)
		return "R;;";
	else
		return NULL;
}

gboolean
almanah_tag_accessible_atk_action_set_description (AtkAction *action, gint i, const gchar *desc)
{
	return FALSE;
}

const gchar*
almanah_tag_accessible_atk_action_get_localized_name (AtkAction *action, gint i)
{
	if (i == 0)
		return _("Remove the tag from the entry");
	else
		return NULL;
}
