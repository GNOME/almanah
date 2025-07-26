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

#include <config.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <atk/atk.h>

#include "preferences-dialog.h"
#include "secret-keys-store.h"
#include "interface.h"
#include "main-window.h"

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void almanah_preferences_dialog_dispose (GObject *object);
static void pd_key_combo_changed_cb (GtkComboBox *combo_box, AlmanahPreferencesDialog *preferences_dialog);
static void pd_new_key_button_clicked_cb (GtkButton *button, AlmanahPreferencesDialog *preferences_dialog);

struct _AlmanahPreferencesDialog {
	GtkDialog parent;

	GSettings *settings;
	GtkComboBox *key_combo;
	AlmanahSecretKeysStore *key_store;
#ifdef ENABLE_SPELL_CHECKING
	guint spell_checking_enabled_id;
#endif /* ENABLE_SPELL_CHECKING */
	GtkCheckButton *spell_checking_enabled_check_button;
};

enum {
	PROP_SETTINGS = 1,
};

G_DEFINE_TYPE (AlmanahPreferencesDialog, almanah_preferences_dialog, GTK_TYPE_DIALOG)

static void
almanah_preferences_dialog_class_init (AlmanahPreferencesDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->get_property = get_property;
	gobject_class->set_property = set_property;
	gobject_class->dispose = almanah_preferences_dialog_dispose;

	g_object_class_install_property (gobject_class, PROP_SETTINGS,
	                                 g_param_spec_object ("settings",
	                                                      "Settings", "Settings instance to modify.",
	                                                      G_TYPE_SETTINGS,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Almanah/ui/preferences-dialog.ui");

	gtk_widget_class_bind_template_child (widget_class, AlmanahPreferencesDialog, key_combo);
	gtk_widget_class_bind_template_child (widget_class, AlmanahPreferencesDialog, spell_checking_enabled_check_button);

	gtk_widget_class_bind_template_callback (widget_class, pd_new_key_button_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, pd_key_combo_changed_cb);
}

static void
almanah_preferences_dialog_init (AlmanahPreferencesDialog *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));
	gtk_window_set_modal (GTK_WINDOW (self), FALSE);
	gtk_window_set_title (GTK_WINDOW (self), _("Preferences"));
	gtk_widget_set_size_request (GTK_WIDGET (self), 400, -1);
	gtk_window_set_resizable (GTK_WINDOW (self), TRUE); /* needs to be resizeable so long keys can be made visible in the list */
}

static void
almanah_preferences_dialog_dispose (GObject *object)
{
	AlmanahPreferencesDialog *self = ALMANAH_PREFERENCES_DIALOG (object);

	g_clear_object (&self->key_store);

	if (self->settings != NULL)
		g_object_unref (self->settings);
	self->settings = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_preferences_dialog_parent_class)->dispose (object);
}

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahPreferencesDialog *self = ALMANAH_PREFERENCES_DIALOG (object);

	switch (property_id) {
		case PROP_SETTINGS:
			g_value_set_object (value, self->settings);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahPreferencesDialog *self = ALMANAH_PREFERENCES_DIALOG (object);

	switch (property_id) {
		case PROP_SETTINGS:
			self->settings = g_value_dup_object (value);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

AlmanahPreferencesDialog *
almanah_preferences_dialog_new (GSettings *settings)
{
	gchar *key;
	AlmanahPreferencesDialog *preferences_dialog;

	g_return_val_if_fail (G_IS_SETTINGS (settings), NULL);

	preferences_dialog = g_object_new (ALMANAH_TYPE_PREFERENCES_DIALOG, NULL);

	preferences_dialog->settings = g_object_ref (settings);

	preferences_dialog->key_store = almanah_secret_keys_store_new ();
	gtk_combo_box_set_model (GTK_COMBO_BOX (preferences_dialog->key_combo), GTK_TREE_MODEL (preferences_dialog->key_store));
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	gtk_combo_box_set_id_column (preferences_dialog->key_combo, SECRET_KEYS_STORE_COLUMN_ID);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (preferences_dialog->key_combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (preferences_dialog->key_combo), renderer,
	                               "text", SECRET_KEYS_STORE_COLUMN_LABEL, NULL);

	/* Set the selected key combo value */
	key = g_settings_get_string (preferences_dialog->settings, "encryption-key");
	gtk_combo_box_set_active_id (preferences_dialog->key_combo, key);
	g_free (key);

#ifdef ENABLE_SPELL_CHECKING
	g_settings_bind (preferences_dialog->settings, "spell-checking-enabled", preferences_dialog->spell_checking_enabled_check_button, "active", G_SETTINGS_BIND_DEFAULT);
#else
	gtk_widget_destroy (GTK_WIDGET (preferences_dialog->spell_checking_enabled_check_button));
	preferences_dialog->settings = NULL;
#endif /* ENABLE_SPELL_CHECKING */

	return preferences_dialog;
}

static void
pd_key_combo_changed_cb (GtkComboBox *combo_box, AlmanahPreferencesDialog *preferences_dialog)
{
	const gchar *key;
	GError *error = NULL;

	/* Save the new encryption key to GSettings */
	key = gtk_combo_box_get_active_id (preferences_dialog->key_combo);
	if (key == NULL)
		key = "";

	if (g_settings_set_string (preferences_dialog->settings, "encryption-key", key) == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (preferences_dialog),
							    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							    _("Error saving the encryption key"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
	}
}

static void
pd_new_key_button_clicked_cb (GtkButton *button, AlmanahPreferencesDialog *preferences_dialog)
{
	/* NOTE: pilfered from cryptui_need_to_get_keys */
	const gchar *argv[2] = { "seahorse", NULL };
	GError *error = NULL;

	if (g_spawn_async (NULL, (gchar**) argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error) == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (preferences_dialog),
							    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							    _("Error opening Seahorse"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
	}
}
