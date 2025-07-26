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

#include <atk/atk.h>
#include <config.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "preferences-dialog.h"
#include "secret-keys-store.h"

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void almanah_preferences_dialog_dispose (GObject *object);
static void pd_key_combo_changed_cb (GtkComboBox *combo_box, AlmanahPreferencesDialog *preferences_dialog);
static void pd_new_key_button_clicked_cb (GtkButton *button, AlmanahPreferencesDialog *preferences_dialog);

struct _AlmanahPreferencesDialog {
	GtkDialog parent;

	GSettings *settings;
	GtkGrid *almanah_pd_grid;
	GtkComboBox *key_combo;
	AlmanahSecretKeysStore *key_store;
#ifdef ENABLE_SPELL_CHECKING
	guint spell_checking_enabled_id;
	GtkCheckButton *spell_checking_enabled_check_button;
#endif /* ENABLE_SPELL_CHECKING */
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

	gtk_widget_class_bind_template_child (widget_class, AlmanahPreferencesDialog, almanah_pd_grid);
}

static void
almanah_preferences_dialog_init (AlmanahPreferencesDialog *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));
	gtk_window_set_modal (GTK_WINDOW (self), FALSE);
	gtk_window_set_title (GTK_WINDOW (self), _ ("Preferences"));
	gtk_widget_set_size_request (GTK_WIDGET (self), 400, -1);
	gtk_window_set_resizable (GTK_WINDOW (self), TRUE); /* needs to be resizeable so long keys can be made visible in the list */
}

static void
almanah_preferences_dialog_dispose (GObject *object)
{
	AlmanahPreferencesDialog *self = ALMANAH_PREFERENCES_DIALOG (object);

	g_clear_object (&self->key_store);
	g_clear_object (&self->settings);

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
	GtkGrid *grid;
	GtkWidget *label, *button;
	AtkObject *a11y_label, *a11y_key_combo;
	g_autofree gchar *key = NULL;
	AlmanahPreferencesDialog *preferences_dialog;

	g_return_val_if_fail (G_IS_SETTINGS (settings), NULL);

	preferences_dialog = g_object_new (ALMANAH_TYPE_PREFERENCES_DIALOG, NULL);

	preferences_dialog->settings = g_object_ref (settings);
	grid = preferences_dialog->almanah_pd_grid;
	gtk_widget_set_halign (GTK_WIDGET (grid), GTK_ALIGN_CENTER);
	gtk_widget_set_valign (GTK_WIDGET (grid), GTK_ALIGN_CENTER);

	/* Grab our child widgets */
	label = gtk_label_new (_ ("Encryption key: "));
	gtk_grid_attach (grid, label, 0, 0, 1, 1);

	preferences_dialog->key_store = almanah_secret_keys_store_new ();
	preferences_dialog->key_combo = GTK_COMBO_BOX (gtk_combo_box_new_with_model (GTK_TREE_MODEL (preferences_dialog->key_store)));
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
	gtk_combo_box_set_id_column (preferences_dialog->key_combo, SECRET_KEYS_STORE_COLUMN_ID);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (preferences_dialog->key_combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (preferences_dialog->key_combo), renderer,
	                                "text", SECRET_KEYS_STORE_COLUMN_LABEL, NULL);
	gtk_grid_attach (grid, GTK_WIDGET (preferences_dialog->key_combo), 1, 0, 1, 1);

	button = gtk_button_new_with_mnemonic (_ ("New _Key"));
	gtk_grid_attach (grid, button, 2, 0, 1, 1);
	g_signal_connect (button, "clicked", G_CALLBACK (pd_new_key_button_clicked_cb), preferences_dialog);

	/* Set up the accessibility relationships */
	a11y_label = gtk_widget_get_accessible (GTK_WIDGET (label));
	a11y_key_combo = gtk_widget_get_accessible (GTK_WIDGET (preferences_dialog->key_combo));
	atk_object_add_relationship (a11y_label, ATK_RELATION_LABEL_FOR, a11y_key_combo);
	atk_object_add_relationship (a11y_key_combo, ATK_RELATION_LABELLED_BY, a11y_label);

	/* Set the selected key combo value */
	key = g_settings_get_string (preferences_dialog->settings, "encryption-key");
	gtk_combo_box_set_active_id (preferences_dialog->key_combo, key);

	g_signal_connect (preferences_dialog->key_combo, "changed", G_CALLBACK (pd_key_combo_changed_cb), preferences_dialog);

#ifdef ENABLE_SPELL_CHECKING
	/* Set up the "Enable spell checking" check button */
	preferences_dialog->spell_checking_enabled_check_button = GTK_CHECK_BUTTON (gtk_check_button_new_with_mnemonic (_ ("Enable _spell checking")));
	gtk_grid_attach (grid, GTK_WIDGET (preferences_dialog->spell_checking_enabled_check_button), 0, 2, 2, 1);

	g_settings_bind (preferences_dialog->settings, "spell-checking-enabled", preferences_dialog->spell_checking_enabled_check_button, "active", G_SETTINGS_BIND_DEFAULT);
#endif /* ENABLE_SPELL_CHECKING */

	return preferences_dialog;
}

static void
pd_key_combo_changed_cb (GtkComboBox *combo_box, AlmanahPreferencesDialog *preferences_dialog)
{
	const gchar *key;

	/* Save the new encryption key to GSettings */
	key = gtk_combo_box_get_active_id (preferences_dialog->key_combo);
	if (key == NULL) {
		key = "";
	}

	if (g_settings_set_string (preferences_dialog->settings, "encryption-key", key) == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (preferences_dialog),
		                                            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		                                            _ ("Error saving the encryption key"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
}

static void
pd_new_key_button_clicked_cb (GtkButton *button, AlmanahPreferencesDialog *preferences_dialog)
{
	/* NOTE: pilfered from cryptui_need_to_get_keys */
	const gchar *argv[2] = { "seahorse", NULL };
	g_autoptr (GError) error = NULL;

	if (g_spawn_async (NULL, (gchar **) argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error) == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (preferences_dialog),
		                                            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		                                            _ ("Error opening Seahorse"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
}
