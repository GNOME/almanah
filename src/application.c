/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2011 <philip@tecnocode.co.uk>
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

#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <locale.h>

#include "application.h"
#include "event-manager.h"
#include "import-export-dialog.h"
#include "main-window.h"
#include "preferences-dialog.h"
#include "printing.h"
#include "search-dialog.h"
#include "storage-manager.h"
#include "interface.h"

static void constructed (GObject *object);
static void dispose (GObject *object);
static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

static void startup (GApplication *application);
static void activate (GApplication *application);
static void window_removed (GtkApplication *application, GtkWindow *window);

/* Application actions */
static void action_search_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void action_preferences_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void action_import_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void action_export_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void action_print_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void action_about_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void action_quit_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data);

/* Some callbacks */
void almanah_application_style_provider_parsing_error_cb (GtkCssProvider *provider, GtkCssSection *section, GError *error, gpointer user_data);

typedef struct {
	gboolean debug;

	GSettings *settings;
	AlmanahStorageManager *storage_manager;
	AlmanahEventManager *event_manager;

	AlmanahMainWindow *main_window;

	GtkPrintSettings *print_settings;
	GtkPageSetup *page_setup;
} AlmanahApplicationPrivate;

struct _AlmanahApplication {
	GtkApplication parent_instance;
};

enum {
	PROP_DEBUG = 1
};

static GActionEntry app_entries[] = {
	{"search", action_search_cb, NULL, NULL, NULL},
	{"preferences", action_preferences_cb, NULL, NULL, NULL },
	{"import", action_import_cb, NULL, NULL, NULL },
	{"export", action_export_cb, NULL, NULL, NULL },
	{"print", action_print_cb, NULL, NULL, NULL },
	{"about", action_about_cb, NULL, NULL, NULL },
	{"quit", action_quit_cb, NULL, NULL, NULL },
};

G_DEFINE_TYPE_WITH_PRIVATE (AlmanahApplication, almanah_application, GTK_TYPE_APPLICATION)

static void
almanah_application_class_init (AlmanahApplicationClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GApplicationClass *gapplication_class = G_APPLICATION_CLASS (klass);
	GtkApplicationClass *gtkapplication_class = GTK_APPLICATION_CLASS (klass);

	gobject_class->constructed = constructed;
	gobject_class->dispose = dispose;
	gobject_class->get_property = get_property;
	gobject_class->set_property = set_property;

	gapplication_class->startup = startup;
	gapplication_class->activate = activate;

	gtkapplication_class->window_removed = window_removed;

	g_object_class_install_property (gobject_class, PROP_DEBUG,
	                                 g_param_spec_boolean ("debug",
	                                                       "Debugging Mode", "Whether debugging mode is active.",
	                                                       FALSE,
	                                                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_application_init (AlmanahApplication *self)
{
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (self);
	priv->debug = FALSE;
}

static void
constructed (GObject *object)
{
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (ALMANAH_APPLICATION (object));

	/* Set various properties up */
	g_application_set_application_id (G_APPLICATION (object), "org.gnome.Almanah");

	/* Localisation */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	const GOptionEntry options[] = {
		{ "debug", 0, 0, G_OPTION_ARG_NONE, &(priv->debug), N_("Enable debug mode"), NULL },
		{ NULL }
	};

	g_application_add_main_option_entries (G_APPLICATION (object), options);
	g_application_set_option_context_summary (G_APPLICATION (object),
	                                          _("Manage your diary. Only one instance of the program may be open at any time."));

	g_set_application_name (_("Almanah Diary"));
	g_set_prgname ("org.gnome.Almanah");
	gtk_window_set_default_icon_name ("org.gnome.Almanah");

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_application_parent_class)->constructed (object);
}

static void
dispose (GObject *object)
{
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (ALMANAH_APPLICATION (object));

	if (priv->main_window != NULL)
		gtk_widget_destroy (GTK_WIDGET (priv->main_window));
	priv->main_window = NULL;

	if (priv->event_manager != NULL)
		g_object_unref (priv->event_manager);
	priv->event_manager = NULL;

	if (priv->storage_manager != NULL)
		g_object_unref (priv->storage_manager);
	priv->storage_manager = NULL;

	if (priv->settings != NULL)
		g_object_unref (priv->settings);
	priv->settings = NULL;

	if (priv->page_setup != NULL)
		g_object_unref (priv->page_setup);
	priv->page_setup = NULL;

	if (priv->print_settings != NULL)
		g_object_unref (priv->print_settings);
	priv->print_settings = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_application_parent_class)->dispose (object);
}

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (ALMANAH_APPLICATION (object));

	switch (property_id) {
		case PROP_DEBUG:
			g_value_set_boolean (value, priv->debug);
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
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (ALMANAH_APPLICATION (object));

	switch (property_id) {
		case PROP_DEBUG:
			priv->debug = g_value_get_boolean (value);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
debug_handler (const char *log_domain, GLogLevelFlags log_level, const char *message, AlmanahApplication *self)
{
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (self);

	/* Only display debug messages if we've been run with --debug */
	if (priv->debug == TRUE) {
		g_log_default_handler (log_domain, log_level, message, NULL);
	}
}

/* This function is taken from the Gedit code, so thanks guys! */
static void
add_accelerator (GtkApplication *app, const gchar *action_name, const gchar *accel)
{
	const gchar *vaccels[] = {
		accel,
		NULL
	};

	gtk_application_set_accels_for_action (app, action_name, vaccels);
}

static void
startup (GApplication *application)
{
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (ALMANAH_APPLICATION (application));
	gchar *db_filename;
	GError *error = NULL;
	GtkCssProvider *style_provider;

	/* Chain up. */
	G_APPLICATION_CLASS (almanah_application_parent_class)->startup (application);

	/* Debug log handling */
	g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, (GLogFunc) debug_handler, application);

	/* Open GSettings */
	priv->settings = g_settings_new ("org.gnome.almanah");

	/* Ensure the DB directory exists */
	if (g_file_test (g_get_user_data_dir (), G_FILE_TEST_IS_DIR) == FALSE) {
		g_mkdir_with_parents (g_get_user_data_dir (), 0700);
	}

	/* Open the DB */
	db_filename = g_build_filename (g_get_user_data_dir (), "diary.db", NULL);
	priv->storage_manager = almanah_storage_manager_new (db_filename, priv->settings);
	g_free (db_filename);

	if (almanah_storage_manager_connect (priv->storage_manager, &error) == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		                                            _("Error opening database"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		/* TODO */
		exit (1);
	}

	/* Create the event manager */
	priv->event_manager = almanah_event_manager_new ();

	/* Set up printing objects */
	priv->print_settings = gtk_print_settings_new ();

#ifdef GTK_PRINT_SETTINGS_OUTPUT_BASENAME
	/* Translators: This is the default name of the PDF/PS/SVG file the diary is printed to if "Print to File" is chosen. */
	gtk_print_settings_set (priv->print_settings, GTK_PRINT_SETTINGS_OUTPUT_BASENAME, _("Diary"));
#endif

	priv->page_setup = gtk_page_setup_new ();

	/* Application actions */
	g_action_map_add_action_entries (G_ACTION_MAP (application), app_entries, G_N_ELEMENTS (app_entries), application);

	/* Application CSS styles */
	style_provider = gtk_css_provider_new ();
	g_signal_connect (G_OBJECT (style_provider), "parsing-error", G_CALLBACK (almanah_application_style_provider_parsing_error_cb), NULL);
	gtk_css_provider_load_from_resource (style_provider, "/org/gnome/Almanah/css/almanah.css");
	gtk_style_context_add_provider_for_screen (gdk_screen_get_default (), GTK_STYLE_PROVIDER (style_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (style_provider);

	/* Shortcuts */
	add_accelerator(GTK_APPLICATION (application), "app.quit", "<Primary>Q");
	add_accelerator(GTK_APPLICATION (application), "app.search", "<Primary>F");
	add_accelerator(GTK_APPLICATION (application), "win.select-date", "<Primary>D");
	add_accelerator(GTK_APPLICATION (application), "win.bold", "<Primary>B");
	add_accelerator(GTK_APPLICATION (application), "win.italic", "<Primary>I");
	add_accelerator(GTK_APPLICATION (application), "win.underline", "<Primary>U");
	add_accelerator(GTK_APPLICATION (application), "win.hyperlink", "<Primary>H");
	add_accelerator(GTK_APPLICATION (application), "win.insert-time", "<Primary>T");
	add_accelerator(GTK_APPLICATION (application), "win.important", "<Primary>M");
	add_accelerator(GTK_APPLICATION (application), "win.undo", "<Primary>Z");
	add_accelerator(GTK_APPLICATION (application), "win.redo", "<Primary><Shift>Z");
}

/* Nullify our pointer to the main window when it gets destroyed (e.g. when we quit) so that we don't then try
 * to destroy it again in dispose(). */
static void
main_window_destroy_cb (AlmanahMainWindow *main_window, AlmanahApplication *self)
{
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (self);

	priv->main_window = NULL;
}

static void
activate (GApplication *application)
{
	AlmanahApplication *self = ALMANAH_APPLICATION (application);
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (self);

	/* Create the interface */
	if (priv->main_window == NULL) {
		priv->main_window = almanah_main_window_new (self);
		gtk_widget_show_all (GTK_WIDGET (priv->main_window));
		g_signal_connect (priv->main_window, "destroy", (GCallback) main_window_destroy_cb, application);
	}

	/* Bring it to the foreground */
	gtk_window_present (GTK_WINDOW (priv->main_window));
}

static void
storage_manager_disconnected_cb (__attribute__ ((unused)) AlmanahStorageManager *storage_manager, const gchar *gpgme_error_message, const gchar *warning_message, GtkApplication *self)
{
	if (gpgme_error_message != NULL || warning_message != NULL) {
		GtkWidget *dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		                                            _("Error encrypting database"));

		if (gpgme_error_message != NULL && warning_message != NULL)
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s %s", warning_message, gpgme_error_message);
		else if (gpgme_error_message != NULL)
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", gpgme_error_message);
		else
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", warning_message);

		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}

	/* Allow the end of the applaction */
	g_application_release (G_APPLICATION (self));
}

static gboolean
storage_disconnect_idle_cb (AlmanahStorageManager *storage_manager)
{
	almanah_storage_manager_disconnect (storage_manager, NULL);

	return FALSE;
}

static void
window_removed (GtkApplication *application, GtkWindow *window)
{
	/* This would normally result in the end of the application, but we need to close the database connection first
	   to prevent an unencrypted database in the filesystem, and we don't want a bug like that.
	   So, we append a reference to the application when the user close the main window. When the application disconnect
	   from the database, allowing the encryption if necessary, we remove this reference with g_application_release.
	   See: https://bugzilla.gnome.org/show_bug.cgi?id=695117 */
	if (ALMANAH_IS_MAIN_WINDOW (window)) {
		AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (ALMANAH_APPLICATION (application));

		g_application_hold (G_APPLICATION (application));

		g_signal_connect (priv->storage_manager, "disconnected", (GCallback) storage_manager_disconnected_cb, application);
		g_idle_add ((GSourceFunc) storage_disconnect_idle_cb, priv->storage_manager);
	}

	GTK_APPLICATION_CLASS (almanah_application_parent_class)->window_removed (application, window);
}

static void
action_search_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	AlmanahApplication *application = ALMANAH_APPLICATION (user_data);
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (application);
	AlmanahSearchDialog *dialog = almanah_search_dialog_new ();

	gtk_window_set_application (GTK_WINDOW (dialog), GTK_APPLICATION (application));
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (priv->main_window));
	gtk_widget_show (GTK_WIDGET (dialog));
	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
action_preferences_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	AlmanahApplication *application = ALMANAH_APPLICATION (user_data);
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (application);
	GSettings *settings;
	AlmanahPreferencesDialog *dialog;
	settings = almanah_application_dup_settings (application);
	dialog = almanah_preferences_dialog_new (settings);
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (priv->main_window));
	g_object_unref (settings);

	gtk_widget_show_all (GTK_WIDGET (dialog));
	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
action_import_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	AlmanahApplication *application = ALMANAH_APPLICATION (user_data);
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (application);
	AlmanahStorageManager *storage_manager;
	GtkWidget *dialog;

	storage_manager = almanah_application_dup_storage_manager (application);
	dialog = GTK_WIDGET (almanah_import_export_dialog_new (storage_manager, TRUE));
	g_object_unref (storage_manager);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (priv->main_window));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	/* The dialog destroys itself once done */
	gtk_widget_show_all (dialog);
}

static void
action_export_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	AlmanahApplication *application = ALMANAH_APPLICATION (user_data);
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (application);
	AlmanahStorageManager *storage_manager;
	GtkWidget *dialog;

	storage_manager = almanah_application_dup_storage_manager (application);
	dialog = GTK_WIDGET (almanah_import_export_dialog_new (storage_manager, FALSE));
	g_object_unref (storage_manager);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (priv->main_window));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	/* The dialog destroys itself once done */
	gtk_widget_show_all (dialog);
}

static void
action_print_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	AlmanahApplication *application = ALMANAH_APPLICATION (user_data);
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (application);
	AlmanahStorageManager *storage_manager;

	storage_manager = almanah_application_dup_storage_manager (application);
	almanah_print_entries (FALSE, GTK_WINDOW (priv->main_window), &(priv->page_setup), &(priv->print_settings), storage_manager);
	g_object_unref (storage_manager);
}

static void
action_about_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	AlmanahApplication *application = ALMANAH_APPLICATION (user_data);
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (application);
	AlmanahStorageManager *storage_manager;
	gchar *license, *description;
	guint entry_count;

	const gchar *authors[] =
	{
		"Philip Withnall <philip@tecnocode.co.uk>",
		NULL
	};
	const gchar *license_parts[] = {
		N_("Almanah is free software: you can redistribute it and/or modify "
		   "it under the terms of the GNU General Public License as published by "
		   "the Free Software Foundation, either version 3 of the License, or "
		   "(at your option) any later version."),
		N_("Almanah is distributed in the hope that it will be useful, "
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		   "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License "
		   "along with Almanah.  If not, see <http://www.gnu.org/licenses/>."),
	};

	license = g_strjoin ("\n\n",
			  _(license_parts[0]),
			  _(license_parts[1]),
			  _(license_parts[2]),
			  NULL);

	storage_manager = almanah_application_dup_storage_manager (application);
	almanah_storage_manager_get_statistics (storage_manager, &entry_count);
	g_object_unref (storage_manager);

	description = g_strdup_printf (_("A helpful diary keeper, storing %u entries."), entry_count);

	gtk_show_about_dialog (GTK_WINDOW (priv->main_window),
				"version", VERSION,
				"copyright", _("Copyright \xc2\xa9 2008-2009 Philip Withnall"),
				"comments", description,
				"authors", authors,
				/* Translators: please include your names here to be credited for your hard work!
				 * Format:
				 * "Translator name 1 <translator@email.address>\n"
				 * "Translator name 2 <translator2@email.address>"
				 */
				"translator-credits", _("translator-credits"),
				"logo-icon-name", "org.gnome.Almanah",
				"license", license,
				"wrap-license", TRUE,
				"website-label", _("Almanah Website"),
				"website", PACKAGE_URL,
				NULL);

	g_free (license);
	g_free (description);
}

static void
action_quit_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	AlmanahApplication *application = ALMANAH_APPLICATION (user_data);
	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (application);
	AlmanahMainWindow *main_window = priv->main_window;

	/* Hide the window to make things look faster */
	gtk_widget_hide (GTK_WIDGET (main_window));

	almanah_main_window_save_current_entry (main_window, TRUE);
	gtk_widget_destroy (GTK_WIDGET (main_window));
}

void
almanah_application_style_provider_parsing_error_cb (__attribute__ ((unused)) GtkCssProvider *provider,
						     __attribute__ ((unused)) GtkCssSection  *section,
						     GError         *error,
						     __attribute__ ((unused)) gpointer        user_data)
{
	g_warning (_("Couldn't load the CSS resources. The interface might not be styled correctly: %s"), error->message);
}

AlmanahApplication *
almanah_application_new (void)
{
	return ALMANAH_APPLICATION (g_object_new (ALMANAH_TYPE_APPLICATION, NULL));
}

gboolean
almanah_application_get_debug (AlmanahApplication *self)
{
	g_return_val_if_fail (ALMANAH_IS_APPLICATION (self), FALSE);

	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (self);

	return priv->debug;
}

AlmanahEventManager *
almanah_application_dup_event_manager (AlmanahApplication *self)
{
	g_return_val_if_fail (ALMANAH_IS_APPLICATION (self), NULL);

	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (self);

	return g_object_ref (priv->event_manager);
}

AlmanahStorageManager *
almanah_application_dup_storage_manager (AlmanahApplication *self)
{
	g_return_val_if_fail (ALMANAH_IS_APPLICATION (self), NULL);

	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (self);

	return g_object_ref (priv->storage_manager);
}

GSettings *
almanah_application_dup_settings (AlmanahApplication *self)
{
	g_return_val_if_fail (ALMANAH_IS_APPLICATION (self), NULL);

	AlmanahApplicationPrivate *priv = almanah_application_get_instance_private (self);

	return g_object_ref (priv->settings);
}
