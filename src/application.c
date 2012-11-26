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

#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "application.h"
#include "event-manager.h"
#include "import-export-dialog.h"
#include "main-window.h"
#include "preferences-dialog.h"
#include "printing.h"
#include "search-dialog.h"
#include "storage-manager.h"

static void constructed (GObject *object);
static void dispose (GObject *object);
static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

static void startup (GApplication *application);
static void activate (GApplication *application);
static gint handle_command_line (GApplication *application, GApplicationCommandLine *command_line);
static void quit_main_loop (GApplication *application);

static void almanah_application_init_actions (AlmanahApplication *self);

/* GMenu application actions */
static void action_search_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void action_preferences_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void action_import_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void action_export_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void action_print_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void action_about_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void action_quit_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data);

struct _AlmanahApplicationPrivate {
	gboolean debug;

	GSettings *settings;
	AlmanahStorageManager *storage_manager;
	AlmanahEventManager *event_manager;

	AlmanahMainWindow *main_window;

	GtkPrintSettings *print_settings;
	GtkPageSetup *page_setup;
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

G_DEFINE_TYPE (AlmanahApplication, almanah_application, GTK_TYPE_APPLICATION)

static void
almanah_application_class_init (AlmanahApplicationClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GApplicationClass *gapplication_class = G_APPLICATION_CLASS (klass);

	g_type_class_add_private (klass, sizeof (AlmanahApplicationPrivate));

	gobject_class->constructed = constructed;
	gobject_class->dispose = dispose;
	gobject_class->get_property = get_property;
	gobject_class->set_property = set_property;

	gapplication_class->startup = startup;
	gapplication_class->activate = activate;
	gapplication_class->command_line = handle_command_line;
	gapplication_class->quit_mainloop = quit_main_loop;

	g_object_class_install_property (gobject_class, PROP_DEBUG,
	                                 g_param_spec_boolean ("debug",
	                                                       "Debugging Mode", "Whether debugging mode is active.",
	                                                       FALSE,
	                                                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
almanah_application_init (AlmanahApplication *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_APPLICATION, AlmanahApplicationPrivate);
	self->priv->debug = FALSE;
}

static void
constructed (GObject *object)
{
	/* Set various properties up */
	g_application_set_application_id (G_APPLICATION (object), "org.gnome.Almanah");
	g_application_set_flags (G_APPLICATION (object), G_APPLICATION_HANDLES_COMMAND_LINE);

	/* Localisation */
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_set_application_name (_("Almanah Diary"));
	gtk_window_set_default_icon_name ("almanah");

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_application_parent_class)->constructed (object);
}

static void
dispose (GObject *object)
{
	AlmanahApplicationPrivate *priv = ALMANAH_APPLICATION (object)->priv;

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
	AlmanahApplicationPrivate *priv = ALMANAH_APPLICATION (object)->priv;

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
	AlmanahApplicationPrivate *priv = ALMANAH_APPLICATION (object)->priv;

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
	/* Only display debug messages if we've been run with --debug */
	if (self->priv->debug == TRUE) {
		g_log_default_handler (log_domain, log_level, message, NULL);
	}
}

static void
startup (GApplication *application)
{
	AlmanahApplicationPrivate *priv = ALMANAH_APPLICATION (application)->priv;
	gchar *db_filename;
	GError *error = NULL;
	GtkCssProvider *style_provider;
	gchar *css_path;

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
	priv->storage_manager = almanah_storage_manager_new (db_filename, NULL);
	g_free (db_filename);

	g_settings_bind (priv->settings, "encryption-key", priv->storage_manager, "encryption-key",
	                 G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

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

	/* Load GMenu application actions */
	almanah_application_init_actions (ALMANAH_APPLICATION (application));

	css_path = g_build_filename (almanah_get_css_path (), "almanah.css", NULL);
	style_provider = gtk_css_provider_new ();
	if (!gtk_css_provider_load_from_path (style_provider, css_path, NULL)) {
		/* Error loading the CSS */
		g_warning (_("Couldn't load the CSS file '%s'. The interface might not be styled correctly"), css_path);
		g_error_free (error);
	} else {
		gtk_style_context_add_provider_for_screen (gdk_screen_get_default (), GTK_STYLE_PROVIDER (style_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
	g_free (css_path);
	g_object_unref (style_provider);
}

/* Nullify our pointer to the main window when it gets destroyed (e.g. when we quit) so that we don't then try
 * to destroy it again in dispose(). */
static void
main_window_destroy_cb (AlmanahMainWindow *main_window, AlmanahApplication *self)
{
	self->priv->main_window = NULL;
}

static void
activate (GApplication *application)
{
	AlmanahApplication *self = ALMANAH_APPLICATION (application);
	AlmanahApplicationPrivate *priv = self->priv;

	/* Create the interface */
	if (priv->main_window == NULL) {
		priv->main_window = almanah_main_window_new (self);
		gtk_widget_show_all (GTK_WIDGET (priv->main_window));
		g_signal_connect (priv->main_window, "destroy", (GCallback) main_window_destroy_cb, application);
	}

	/* Bring it to the foreground */
	gtk_window_present (GTK_WINDOW (priv->main_window));
}

static gint
handle_command_line (GApplication *application, GApplicationCommandLine *command_line)
{
	AlmanahApplicationPrivate *priv = ALMANAH_APPLICATION (application)->priv;
	GOptionContext *context;
	GError *error = NULL;
	gchar **args, **argv;
	gint argc, i, status = 0;

	const GOptionEntry options[] = {
		{ "debug", 0, 0, G_OPTION_ARG_NONE, &(priv->debug), N_("Enable debug mode"), NULL },
		{ NULL }
	};

	args = g_application_command_line_get_arguments (command_line, &argc);

	/* We have to make an extra copy of the array, since g_option_context_parse() assumes that it can remove strings from the array without
	 * freeing them. */
	argv = g_new (gchar*, argc + 1);
	for (i = 0; i <= argc; i++) {
		argv[i] = args[i];
	}

	/* Options */
	context = g_option_context_new (NULL);
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);

	g_option_context_set_summary (context, _("Manage your diary. Only one instance of the program may be open at any time."));

	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));

	if (g_option_context_parse (context, &argc, &argv, &error) == TRUE) {
		/* Activate the remote instance */
		g_application_activate (application);
		status = 0;
	} else {
		/* Print an error */
		g_application_command_line_printerr (command_line, _("Command line options could not be parsed: %s\n"), error->message);
		g_error_free (error);

		status = 1;
	}

	g_option_context_free (context);

	g_free (argv);
	g_strfreev (args);

	return status;
}

static void
storage_manager_disconnected_cb (AlmanahStorageManager *self, const gchar *gpgme_error_message, const gchar *warning_message, GApplication *application)
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

	/* Chain up to the parent class */
	G_APPLICATION_CLASS (almanah_application_parent_class)->quit_mainloop (application);
}

static void
quit_main_loop (GApplication *application)
{
	AlmanahApplicationPrivate *priv = ALMANAH_APPLICATION (application)->priv;

	/* This would normally result in gtk_main_quit() being called, but we need to close the database connection first. */
	g_signal_connect (priv->storage_manager, "disconnected", (GCallback) storage_manager_disconnected_cb, application);
	almanah_storage_manager_disconnect (priv->storage_manager, NULL);

	/* Quitting is actually done in storage_manager_disconnected_cb, which is called once
	 * the storage manager has encrypted the DB and disconnected from it. */
}

static void
almanah_application_init_actions (AlmanahApplication *self)
{
	GtkBuilder *builder;
	GError *error = NULL;
	const gchar *interface_filename = almanah_get_interface_app_menu_filename ();

	g_action_map_add_action_entries (G_ACTION_MAP (self), app_entries, G_N_ELEMENTS (app_entries), self);

	builder = gtk_builder_new ();
	if (gtk_builder_add_from_file (builder, interface_filename, &error) == FALSE) {
		/* Show an error */
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
							    GTK_DIALOG_MODAL,
							    GTK_MESSAGE_ERROR,
							    GTK_BUTTONS_OK,
							    _("UI file \"%s\" could not be loaded"), interface_filename);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
		g_object_unref (builder);

		exit (1);
	}

	gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	gtk_application_set_app_menu (GTK_APPLICATION (self), G_MENU_MODEL (gtk_builder_get_object (builder, "almanah_app_menu")));

#ifndef ENABLE_ENCRYPTION
#ifndef ENABLE_SPELL_CHECKING
	/* Remove the "Preferences" entry from the menu */
	g_action_map_remove_action (G_ACTION_MAP (self), "preferences");
#endif /* !ENABLE_SPELL_CHECKING */
#endif /* !ENABLE_ENCRYPTION */

	g_object_unref (builder);
}

static void
action_search_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	AlmanahApplication *application;
	AlmanahSearchDialog *dialog = almanah_search_dialog_new ();

	application = ALMANAH_APPLICATION (user_data);
	gtk_window_set_application (GTK_WINDOW (dialog), GTK_APPLICATION (application));
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (application->priv->main_window));
	gtk_widget_show (GTK_WIDGET (dialog));
	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
action_preferences_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
#if defined(ENABLE_ENCRYPTION) || defined(ENABLE_SPELL_CHECKING)
	AlmanahApplication *application;
	GSettings *settings;
	AlmanahPreferencesDialog *dialog;

	application = ALMANAH_APPLICATION (user_data);
	settings = almanah_application_dup_settings (application);
	dialog = almanah_preferences_dialog_new (settings);
	g_object_unref (settings);

	gtk_widget_show_all (GTK_WIDGET (dialog));
	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (GTK_WIDGET (dialog));
#endif /* ENABLE_ENCRYPTION || ENABLE_SPELL_CHECKING */
}

static void
action_import_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	AlmanahApplication *application;
	AlmanahStorageManager *storage_manager;
	GtkWidget *dialog;

	application = ALMANAH_APPLICATION (user_data);
	storage_manager = almanah_application_dup_storage_manager (application);
	dialog = GTK_WIDGET (almanah_import_export_dialog_new (storage_manager, TRUE));
	g_object_unref (storage_manager);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (application->priv->main_window));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	/* The dialog destroys itself once done */
	gtk_widget_show_all (dialog);
}

static void
action_export_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	AlmanahApplication *application;
	AlmanahStorageManager *storage_manager;
	GtkWidget *dialog;

	application = ALMANAH_APPLICATION (user_data);
	storage_manager = almanah_application_dup_storage_manager (application);
	dialog = GTK_WIDGET (almanah_import_export_dialog_new (storage_manager, FALSE));
	g_object_unref (storage_manager);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (application->priv->main_window));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	/* The dialog destroys itself once done */
	gtk_widget_show_all (dialog);
}

static void
action_print_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	AlmanahApplication *application;
	AlmanahStorageManager *storage_manager;

	application = ALMANAH_APPLICATION (user_data);
	storage_manager = almanah_application_dup_storage_manager (application);
	almanah_print_entries (FALSE, GTK_WINDOW (application->priv->main_window), &(application->priv->page_setup), &(application->priv->print_settings), storage_manager);
	g_object_unref (storage_manager);
}

static void
action_about_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	AlmanahApplication *application;
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

	application = ALMANAH_APPLICATION (user_data);
	storage_manager = almanah_application_dup_storage_manager (application);
	almanah_storage_manager_get_statistics (storage_manager, &entry_count);
	g_object_unref (storage_manager);

	description = g_strdup_printf (_("A helpful diary keeper, storing %u entries."), entry_count);

	gtk_show_about_dialog (GTK_WINDOW (application->priv->main_window),
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
				"logo-icon-name", "almanah",
				"license", license,
				"wrap-license", TRUE,
				"website-label", _("Almanah Website"),
				"website", "http://live.gnome.org/Almanah_Diary",
				NULL);

	g_free (license);
	g_free (description);
}

static void
action_quit_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	AlmanahMainWindow *main_window;

	main_window = ALMANAH_APPLICATION (user_data)->priv->main_window;

	/* Hide the window to make things look faster */
	gtk_widget_hide (GTK_WIDGET (main_window));

	almanah_main_window_save_current_entry (main_window, TRUE);
	gtk_widget_destroy (GTK_WIDGET (main_window));
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
	return self->priv->debug;
}

AlmanahEventManager *
almanah_application_dup_event_manager (AlmanahApplication *self)
{
	g_return_val_if_fail (ALMANAH_IS_APPLICATION (self), NULL);
	return g_object_ref (self->priv->event_manager);
}

AlmanahStorageManager *
almanah_application_dup_storage_manager (AlmanahApplication *self)
{
	g_return_val_if_fail (ALMANAH_IS_APPLICATION (self), NULL);
	return g_object_ref (self->priv->storage_manager);
}

GSettings *
almanah_application_dup_settings (AlmanahApplication *self)
{
	g_return_val_if_fail (ALMANAH_IS_APPLICATION (self), NULL);
	return g_object_ref (self->priv->settings);
}
