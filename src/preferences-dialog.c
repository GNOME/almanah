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
#define LIBCRYPTUI_API_SUBJECT_TO_CHANGE
#include <cryptui-key-combo.h>
#include <cryptui-keyset.h>
#include <cryptui.h>
#include <atk/atk.h>

#include "preferences-dialog.h"
#include "interface.h"
#include "main-window.h"

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void almanah_preferences_dialog_dispose (GObject *object);
static void pd_key_combo_changed_cb (GtkComboBox *combo_box, AlmanahPreferencesDialog *preferences_dialog);
static void pd_new_key_button_clicked_cb (GtkButton *button, AlmanahPreferencesDialog *preferences_dialog);

typedef struct {
	GSettings *settings;
	CryptUIKeyset *keyset;
	CryptUIKeyStore *key_store;
	GtkComboBox *key_combo;
#ifdef ENABLE_SPELL_CHECKING
	guint spell_checking_enabled_id;
	GtkCheckButton *spell_checking_enabled_check_button;
#endif /* ENABLE_SPELL_CHECKING */
} AlmanahPreferencesDialogPrivate;

struct _AlmanahPreferencesDialog {
	GtkDialog parent;
};

enum {
	PROP_SETTINGS = 1,
};

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahPreferencesDialog, almanah_preferences_dialog, GTK_TYPE_DIALOG)

static void
almanah_preferences_dialog_class_init (AlmanahPreferencesDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = get_property;
	gobject_class->set_property = set_property;
	gobject_class->dispose = almanah_preferences_dialog_dispose;

	g_object_class_install_property (gobject_class, PROP_SETTINGS,
	                                 g_param_spec_object ("settings",
	                                                      "Settings", "Settings instance to modify.",
	                                                      G_TYPE_SETTINGS,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_preferences_dialog_init (AlmanahPreferencesDialog *self)
{
	gtk_window_set_modal (GTK_WINDOW (self), FALSE);
	gtk_window_set_title (GTK_WINDOW (self), _("Preferences"));
	gtk_widget_set_size_request (GTK_WIDGET (self), 400, -1);
	gtk_window_set_resizable (GTK_WINDOW (self), TRUE); /* needs to be resizeable so long keys can be made visible in the list */
}

static void
almanah_preferences_dialog_dispose (GObject *object)
{
	AlmanahPreferencesDialogPrivate *priv = almanah_preferences_dialog_get_instance_private (ALMANAH_PREFERENCES_DIALOG (object));

	if (priv->keyset != NULL) {
		g_object_unref (priv->keyset);
		priv->keyset = NULL;
	}

	if (priv->key_store != NULL) {
		g_object_unref (priv->key_store);
		priv->key_store = NULL;
	}

	if (priv->settings != NULL)
		g_object_unref (priv->settings);
	priv->settings = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_preferences_dialog_parent_class)->dispose (object);
}

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahPreferencesDialogPrivate *priv = almanah_preferences_dialog_get_instance_private (ALMANAH_PREFERENCES_DIALOG (object));

	switch (property_id) {
		case PROP_SETTINGS:
			g_value_set_object (value, priv->settings);
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
	AlmanahPreferencesDialogPrivate *priv = almanah_preferences_dialog_get_instance_private (self);

	switch (property_id) {
		case PROP_SETTINGS:
			priv->settings = g_value_dup_object (value);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/* Filter the key list so it's not pages and pages long */
static gboolean
key_store_filter_cb (CryptUIKeyset *keyset, const gchar *key, gpointer user_data)
{
	guint flags = cryptui_keyset_key_flags (keyset, key);
	return flags & CRYPTUI_FLAG_CAN_SIGN; /* if the key can sign, we have the private key part and can decrypt the database */
}

AlmanahPreferencesDialog *
almanah_preferences_dialog_new (GSettings *settings)
{
	GtkBuilder *builder;
	GtkGrid *grid;
	GtkWidget *label, *button;
	AtkObject *a11y_label, *a11y_key_combo;
	gchar *key;
	AlmanahPreferencesDialog *preferences_dialog;
	AlmanahPreferencesDialogPrivate *priv;
	GError *error = NULL;
	const gchar *object_names[] = {
		"almanah_preferences_dialog",
		NULL
	};

	g_return_val_if_fail (G_IS_SETTINGS (settings), NULL);

	builder = gtk_builder_new ();

	if (gtk_builder_add_objects_from_resource (builder, "/org/gnome/Almanah/ui/almanah.ui", (gchar**) object_names, &error) == 0) {
		/* Show an error */
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
							    GTK_DIALOG_MODAL,
							    GTK_MESSAGE_ERROR,
							    GTK_BUTTONS_OK,
							    _("UI data could not be loaded"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
		g_object_unref (builder);

		return NULL;
	}

	gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	preferences_dialog = ALMANAH_PREFERENCES_DIALOG (gtk_builder_get_object (builder, "almanah_preferences_dialog"));
	gtk_builder_connect_signals (builder, preferences_dialog);

	if (preferences_dialog == NULL) {
		g_object_unref (builder);
		return NULL;
	}

	priv = almanah_preferences_dialog_get_instance_private (preferences_dialog);
	priv->settings = g_object_ref (settings);
	grid = GTK_GRID (gtk_builder_get_object (builder, "almanah_pd_grid"));
	gtk_widget_set_halign (GTK_WIDGET (grid), GTK_ALIGN_CENTER);
	gtk_widget_set_valign (GTK_WIDGET (grid), GTK_ALIGN_CENTER);

	/* Grab our child widgets */
	label = gtk_label_new (_("Encryption key: "));
	gtk_grid_attach (grid, label, 0, 0, 1, 1);

	priv->keyset = cryptui_keyset_new ("openpgp", FALSE);
	priv->key_store = cryptui_key_store_new (priv->keyset, FALSE, _("None (don't encrypt)"));
	cryptui_key_store_set_filter (priv->key_store, key_store_filter_cb, NULL);
	priv->key_combo = cryptui_key_combo_new (priv->key_store);
	gtk_grid_attach (grid, GTK_WIDGET (priv->key_combo), 1, 0, 1, 1);

	button = gtk_button_new_with_mnemonic (_("New _Key"));
	gtk_grid_attach (grid, button, 2, 0, 1, 1);
	g_signal_connect (button, "clicked", G_CALLBACK (pd_new_key_button_clicked_cb), preferences_dialog);

	/* Set up the accessibility relationships */
	a11y_label = gtk_widget_get_accessible (GTK_WIDGET (label));
	a11y_key_combo = gtk_widget_get_accessible (GTK_WIDGET (priv->key_combo));
	atk_object_add_relationship (a11y_label, ATK_RELATION_LABEL_FOR, a11y_key_combo);
	atk_object_add_relationship (a11y_key_combo, ATK_RELATION_LABELLED_BY, a11y_label);

	/* Set the selected key combo value */
	key = g_settings_get_string (priv->settings, "encryption-key");
	if (key != NULL && *key == '\0') {
		g_free (key);
		key = NULL;
	}

	cryptui_key_combo_set_key (priv->key_combo, key);
	g_free (key);

	g_signal_connect (priv->key_combo, "changed", G_CALLBACK (pd_key_combo_changed_cb), preferences_dialog);

#ifdef ENABLE_SPELL_CHECKING
	/* Set up the "Enable spell checking" check button */
	priv->spell_checking_enabled_check_button = GTK_CHECK_BUTTON (gtk_check_button_new_with_mnemonic (_("Enable _spell checking")));
	gtk_grid_attach (grid, GTK_WIDGET (priv->spell_checking_enabled_check_button), 0, 2, 2, 1);

	g_settings_bind (priv->settings, "spell-checking-enabled", priv->spell_checking_enabled_check_button, "active", G_SETTINGS_BIND_DEFAULT);
#endif /* ENABLE_SPELL_CHECKING */

	g_object_unref (builder);

	return preferences_dialog;
}

static void
pd_key_combo_changed_cb (GtkComboBox *combo_box, AlmanahPreferencesDialog *preferences_dialog)
{
	AlmanahPreferencesDialogPrivate *priv = almanah_preferences_dialog_get_instance_private (preferences_dialog);
	const gchar *key;
	GError *error = NULL;

	/* Save the new encryption key to GSettings */
	key = cryptui_key_combo_get_key (priv->key_combo);
	if (key == NULL)
		key = "";

	if (g_settings_set_string (priv->settings, "encryption-key", key) == FALSE) {
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
