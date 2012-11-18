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
#include <time.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#ifdef ENABLE_SPELL_CHECKING
#include <gtkspell/gtkspell.h>
#endif /* ENABLE_SPELL_CHECKING */

#include "main-window.h"
#include "interface.h"
#include "date-entry-dialog.h"
#include "entry.h"
#include "storage-manager.h"
#include "event-manager.h"
#include "event.h"
#include "uri-entry-dialog.h"
#include "widgets/calendar.h"
#include "widgets/calendar-button.h"
#include "widgets/font-style-menu-action.h"
#include "widgets/hyperlink-tag.h"

/* Interval for automatically saving the current entry. Currently an arbitrary 10 minutes. */
#define SAVE_ENTRY_INTERVAL 10 * 60 /* seconds */

static void almanah_main_window_dispose (GObject *object);
#ifdef ENABLE_SPELL_CHECKING
static void spell_checking_enabled_changed_cb (GSettings *settings, gchar *key, AlmanahMainWindow *self);
static gboolean enable_spell_checking (AlmanahMainWindow *self, GError **error);
static void disable_spell_checking (AlmanahMainWindow *self);
#endif /* ENABLE_SPELL_CHECKING */
static void set_current_entry (AlmanahMainWindow *self, AlmanahEntry *entry);
static void save_window_state (AlmanahMainWindow *self);
static void restore_window_state (AlmanahMainWindow *self);
static gboolean mw_delete_event_cb (GtkWindow *window, gpointer user_data);
static void mw_entry_buffer_cursor_position_cb (GObject *object, GParamSpec *pspec, AlmanahMainWindow *main_window);
static void mw_entry_buffer_insert_text_cb (GtkTextBuffer *text_buffer, GtkTextIter *start, gchar *text, gint len, AlmanahMainWindow *main_window);
static void mw_entry_buffer_insert_text_after_cb (GtkTextBuffer *text_buffer, GtkTextIter *start, gchar *text, gint len, AlmanahMainWindow *main_window);
static void mw_entry_buffer_has_selection_cb (GObject *object, GParamSpec *pspec, AlmanahMainWindow *main_window);
static void mw_bold_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window);
static void mw_italic_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window);
static void mw_underline_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window);
static void mw_hyperlink_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window);
static void mw_events_updated_cb (AlmanahEventManager *event_manager, AlmanahEventFactoryType type_id, AlmanahMainWindow *main_window);
static void mw_font_style_menu_position_func (GtkMenu *menu, int *x, int *y, gboolean *push_in, GtkMenuToolButton *button);
static GtkMenuToolButton *mw_get_font_style_tool_button_from_action (GtkAction *action);
static gboolean save_entry_timeout_cb (AlmanahMainWindow *self);
static void mw_setup_toolbar (AlmanahMainWindow *main_window, AlmanahApplication *application, GtkToolbar *toolbar, GtkAction *today_action, GtkAction *select_date_action, GtkWidget *menu_popup);

/* GtkBuilder callbacks */
void mw_cut_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_copy_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_paste_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_delete_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_insert_time_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_important_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_select_date_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_jump_to_today_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_old_entries_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_events_tree_view_row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, AlmanahMainWindow *main_window);
void mw_font_style_activate_cb (AlmanahFontStyleMenuAction *action, AlmanahMainWindow *main_window);

/* Other callbacks */
void mw_calendar_day_selected_cb (AlmanahCalendarButton *calendar, AlmanahMainWindow *main_window);
static void mw_menu_button_popup_visible_cb (GtkWidget *menu, GParamSpec *pspec, GtkWidget *button);

struct _AlmanahMainWindowPrivate {
	GtkTextView *entry_view;
	GtkTextBuffer *entry_buffer;
	AlmanahCalendarButton *calendar_button;
	GtkListStore *event_store;
	GtkWidget *events_expander;
	GtkLabel *events_count_label;
	GtkTreeSelection *events_selection;
	GtkToggleAction *bold_action;
	GtkToggleAction *italic_action;
	GtkToggleAction *underline_action;
	GtkToggleAction *hyperlink_action;
	GtkAction *cut_action;
	GtkAction *copy_action;
	GtkAction *delete_action;
	GtkAction *important_action;
	GtkMenuToolButton *font_style_tool_button;

	gboolean updating_formatting;
	gboolean pending_bold_active;
	gboolean pending_italic_active;
	gboolean pending_underline_active;

	AlmanahEntry *current_entry; /* whether it's been modified is stored as gtk_text_buffer_get_modified (priv->entry_buffer) */
	gulong current_entry_notify_id; /* signal handler for current_entry::notify */
	guint save_entry_timeout_id; /* source ID for timer to save current entry periodically */

#ifdef ENABLE_SPELL_CHECKING
	GSettings *settings;
	gulong spell_checking_enabled_changed_id; /* signal handler for application->settings::changed::spell-checking-enabled */
#endif /* ENABLE_SPELL_CHECKING */
};

G_DEFINE_TYPE (AlmanahMainWindow, almanah_main_window, GTK_TYPE_APPLICATION_WINDOW)
#define ALMANAH_MAIN_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_MAIN_WINDOW, AlmanahMainWindowPrivate))

static void
almanah_main_window_class_init (AlmanahMainWindowClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	g_type_class_add_private (klass, sizeof (AlmanahMainWindowPrivate));
	gobject_class->dispose = almanah_main_window_dispose;
}

static void
almanah_main_window_init (AlmanahMainWindow *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_MAIN_WINDOW, AlmanahMainWindowPrivate);

	gtk_window_set_title (GTK_WINDOW (self), _("Almanah Diary"));
	g_signal_connect (self, "delete-event", G_CALLBACK (mw_delete_event_cb), NULL);
}

static void
almanah_main_window_dispose (GObject *object)
{
	AlmanahMainWindowPrivate *priv = ALMANAH_MAIN_WINDOW (object)->priv;

	if (priv->save_entry_timeout_id != 0) {
		g_source_remove (priv->save_entry_timeout_id);
		priv->save_entry_timeout_id = 0;
	}

	set_current_entry (ALMANAH_MAIN_WINDOW (object), NULL);

#ifdef ENABLE_SPELL_CHECKING
	if (priv->settings != NULL) {
		if (priv->spell_checking_enabled_changed_id != 0) {
			g_signal_handler_disconnect (priv->settings, priv->spell_checking_enabled_changed_id);
			priv->spell_checking_enabled_changed_id = 0;
		}

		g_object_unref (priv->settings);
		priv->settings = NULL;
	}
#endif /* ENABLE_SPELL_CHECKING */

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_main_window_parent_class)->dispose (object);
}

AlmanahMainWindow *
almanah_main_window_new (AlmanahApplication *application)
{
	GtkBuilder *builder;
	AlmanahEventManager *event_manager;
	AlmanahMainWindow *main_window;
	AlmanahMainWindowPrivate *priv;
	GtkToolbar *toolbar;
	GtkAction *today_action, *select_date_action;
	GtkUIManager *manager;
	GtkWidget *menu_popup;
	GError *error = NULL;
	const gchar *interface_filename = almanah_get_interface_filename ();
	const gchar *object_names[] = {
		"almanah_main_window",
		"almanah_mw_event_store",
		"almanah_ui_manager",
		NULL
	};

	g_return_val_if_fail (ALMANAH_IS_APPLICATION (application), NULL);

	builder = gtk_builder_new ();

	if (gtk_builder_add_objects_from_file (builder, interface_filename, (gchar**) object_names, &error) == FALSE) {
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

		return NULL;
	}

	gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	main_window = ALMANAH_MAIN_WINDOW (gtk_builder_get_object (builder, "almanah_main_window"));
	gtk_builder_connect_signals (builder, main_window);

	if (main_window == NULL) {
		g_object_unref (builder);
		return NULL;
	}

	/* Set up the application */
	gtk_window_set_application (GTK_WINDOW (main_window), GTK_APPLICATION (application));

	priv = ALMANAH_MAIN_WINDOW (main_window)->priv;

	/* Grab our child widgets */
	priv->entry_view = GTK_TEXT_VIEW (gtk_builder_get_object (builder, "almanah_mw_entry_view"));
	priv->entry_buffer = gtk_text_view_get_buffer (priv->entry_view);
	priv->event_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "almanah_mw_event_store"));
	priv->events_expander = GTK_WIDGET (gtk_builder_get_object (builder, "almanah_mw_events_expander"));
	priv->events_count_label = GTK_LABEL (gtk_builder_get_object (builder, "almanah_mw_events_count_label"));
	priv->events_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_builder_get_object (builder, "almanah_mw_events_tree_view")));
	priv->bold_action = GTK_TOGGLE_ACTION (gtk_builder_get_object (builder, "almanah_ui_bold"));;
	priv->italic_action = GTK_TOGGLE_ACTION (gtk_builder_get_object (builder, "almanah_ui_italic"));
	priv->underline_action = GTK_TOGGLE_ACTION (gtk_builder_get_object (builder, "almanah_ui_underline"));
	priv->hyperlink_action = GTK_TOGGLE_ACTION (gtk_builder_get_object (builder, "almanah_ui_hyperlink"));
	priv->cut_action = GTK_ACTION (gtk_builder_get_object (builder, "almanah_ui_cut"));
	priv->copy_action = GTK_ACTION (gtk_builder_get_object (builder, "almanah_ui_copy"));
	priv->delete_action = GTK_ACTION (gtk_builder_get_object (builder, "almanah_ui_delete"));
	priv->important_action = GTK_ACTION (gtk_builder_get_object (builder, "almanah_ui_important"));
	priv->font_style_tool_button = mw_get_font_style_tool_button_from_action (GTK_ACTION (gtk_builder_get_object (builder, "almanah_ui_font_style")));

#ifdef ENABLE_SPELL_CHECKING
	/* Set up spell checking, if it's enabled */
	priv->settings = almanah_application_dup_settings (application);

	if (g_settings_get_boolean (priv->settings, "spell-checking-enabled") == TRUE) {
		enable_spell_checking (main_window, NULL);
	}

	/* We don't use g_settings_bind() because enabling spell checking could fail, and we need to show an error dialogue */
	priv->spell_checking_enabled_changed_id = g_signal_connect (priv->settings, "changed::spell-checking-enabled",
	                                                            (GCallback) spell_checking_enabled_changed_cb, main_window);
#endif /* ENABLE_SPELL_CHECKING */

	/* Set up text formatting. It's important this is done after setting up GtkSpell, so that we know whether to
	 * create a dummy gtkspell-misspelled text tag. */
	almanah_interface_create_text_tags (priv->entry_buffer, TRUE);

	/* Make sure we're notified if the cursor moves position so we can check the tag stack */
	g_signal_connect (priv->entry_buffer, "notify::cursor-position", G_CALLBACK (mw_entry_buffer_cursor_position_cb), main_window);

	/* Make sure we're notified when text is inserted, so we can format it consistently.
	 * This must be done after the default handler, as that's where the text is actually inserted. */
	g_signal_connect (priv->entry_buffer, "insert-text", G_CALLBACK (mw_entry_buffer_insert_text_cb), main_window);
	g_signal_connect_after (priv->entry_buffer, "insert-text", G_CALLBACK (mw_entry_buffer_insert_text_after_cb), main_window);

	/* Similarly, make sure we're notified when there's a selection so we can change the status of cut/copy/paste actions */
	g_signal_connect (priv->entry_buffer, "notify::has-selection", G_CALLBACK (mw_entry_buffer_has_selection_cb), main_window);

	/* Connect up the formatting actions */
	g_signal_connect (priv->bold_action, "toggled", G_CALLBACK (mw_bold_toggled_cb), main_window);
	g_signal_connect (priv->italic_action, "toggled", G_CALLBACK (mw_italic_toggled_cb), main_window);
	g_signal_connect (priv->underline_action, "toggled", G_CALLBACK (mw_underline_toggled_cb), main_window);
	g_signal_connect (priv->hyperlink_action, "toggled", (GCallback) mw_hyperlink_toggled_cb, main_window);

	/* Notification for event changes */
	event_manager = almanah_application_dup_event_manager (application);
	g_signal_connect (event_manager, "events-updated", G_CALLBACK (mw_events_updated_cb), main_window);
	g_object_unref (event_manager);

	/* Set up the toolbar */
	toolbar = GTK_TOOLBAR (gtk_builder_get_object (builder, "almanah_mw_toolbar"));
	today_action = GTK_ACTION (gtk_builder_get_object (builder, "almanah_ui_jump_to_today"));
	select_date_action = GTK_ACTION (gtk_builder_get_object (builder, "almanah_ui_select_date"));
	manager = GTK_UI_MANAGER (gtk_builder_get_object (builder, "almanah_ui_manager"));
	menu_popup = gtk_ui_manager_get_widget (manager, "/almanah_mw_menu_button");
	mw_setup_toolbar (main_window, application, toolbar, today_action, select_date_action, menu_popup);

	/* Select the current day and month */
	mw_jump_to_today_activate_cb (NULL, main_window);

	/* Set up a timeout for saving the current entry every so often. */
	priv->save_entry_timeout_id = g_timeout_add_seconds (SAVE_ENTRY_INTERVAL, (GSourceFunc) save_entry_timeout_cb, main_window);

	g_object_unref (builder);

	restore_window_state (main_window);

	return main_window;
}

static void
current_entry_notify_cb (AlmanahEntry *entry, GParamSpec *pspec, AlmanahMainWindow *self)
{
	/* As the entry's been changed, mark it as edited so that it has to be saved */
	gtk_text_buffer_set_modified (self->priv->entry_buffer, TRUE);
}

static void
set_current_entry (AlmanahMainWindow *self, AlmanahEntry *entry)
{
	AlmanahMainWindowPrivate *priv = self->priv;

	/* Disconnect from and unref the old entry */
	if (priv->current_entry != NULL) {
		g_signal_handler_disconnect (priv->current_entry, priv->current_entry_notify_id);
		g_object_unref (priv->current_entry);
	}

	priv->current_entry = NULL;
	priv->current_entry_notify_id = 0;

	/* Ref and connect to the new entry */
	if (entry != NULL) {
		priv->current_entry = g_object_ref (entry);
		priv->current_entry_notify_id = g_signal_connect (entry, "notify", (GCallback) current_entry_notify_cb, self);
	}
}

static GFile *
get_window_state_file (void)
{
	GFile *key_file_path;
	gchar *filename;

	filename = g_build_filename (g_get_user_config_dir (), PACKAGE_NAME, "state.ini", NULL);
	key_file_path = g_file_new_for_path (filename);
	g_free (filename);

	return key_file_path;
}

static void
save_window_state (AlmanahMainWindow *self)
{
	GKeyFile *key_file;
	GFile *key_file_path = NULL, *key_file_directory;
	gchar *key_file_data;
	gsize key_file_length;
	GdkWindow *window;
	GdkWindowState state;
	gint width, height, x, y;
	GError *error = NULL;

	/* Overwrite the existing state file with a new one */
	key_file = g_key_file_new ();

	window = gtk_widget_get_window (GTK_WIDGET (self));
	state = gdk_window_get_state (window);

	/* Maximisation state */
	g_key_file_set_boolean (key_file, "main-window", "maximized", state & GDK_WINDOW_STATE_MAXIMIZED ? TRUE : FALSE);

	/* Save the window dimensions */
	gtk_window_get_size (GTK_WINDOW (self), &width, &height);

	g_key_file_set_integer (key_file, "main-window", "width", width);
	g_key_file_set_integer (key_file, "main-window", "height", height);

	/* Save the window position */
	gtk_window_get_position (GTK_WINDOW (self), &x, &y);

	g_key_file_set_integer (key_file, "main-window", "x-position", x);
	g_key_file_set_integer (key_file, "main-window", "y-position", y);

	/* Serialise the key file data */
	key_file_data = g_key_file_to_data (key_file, &key_file_length, &error);
	g_key_file_free (key_file);

	if (error != NULL) {
		g_warning ("Error generating window state data: %s", error->message);
		g_error_free (error);
		goto done;
	}

	/* Ensure that the correct directories exist */
	key_file_path = get_window_state_file ();

	key_file_directory = g_file_get_parent (key_file_path);
	g_file_make_directory_with_parents (key_file_directory, NULL, &error);
	g_object_unref (key_file_directory);

	if (error != NULL) {
		if (error->code != G_IO_ERROR_EXISTS) {
			gchar *parse_name = g_file_get_parse_name (key_file_path);
			g_warning ("Error creating directory for window state data file “%s”: %s", parse_name, error->message);
			g_free (parse_name);
		}

		g_clear_error (&error);

		/* Continue to attempt to write the file anyway */
	}

	/* Save the new key file (synchronously, since we want it to complete before we finish quitting the program) */
	g_file_replace_contents (key_file_path, key_file_data, key_file_length, NULL, FALSE, G_FILE_CREATE_PRIVATE, NULL, NULL, &error);

	if (error != NULL) {
		gchar *parse_name = g_file_get_parse_name (key_file_path);
		g_warning ("Error saving window state data to “%s”: %s", parse_name, error->message);
		g_free (parse_name);

		g_error_free (error);
		goto done;
	}

done:
	if (key_file_path != NULL) {
		g_object_unref (key_file_path);
	}

	g_free (key_file_data);
}

static void
restore_window_state_cb (GFile *key_file_path, GAsyncResult *result, AlmanahMainWindow *self)
{
	GKeyFile *key_file = NULL;
	gchar *key_file_data = NULL;
	gsize key_file_length;
	gint width = -1, height = -1, x = -1, y = -1;
	GError *error = NULL;

	g_file_load_contents_finish (key_file_path, result, &key_file_data, &key_file_length, NULL, &error);

	if (error != NULL) {
		if (error->code != G_IO_ERROR_NOT_FOUND) {
			gchar *parse_name = g_file_get_parse_name (key_file_path);
			g_warning ("Error loading window state data from “%s”: %s", parse_name, error->message);
			g_free (parse_name);
		}

		g_error_free (error);
		goto done;
	}

	/* Skip loading the key file if it has zero length */
	if (key_file_length == 0) {
		goto done;
	}

	/* Load the key file's data into the GKeyFile */
	key_file = g_key_file_new ();
	g_key_file_load_from_data (key_file, key_file_data, key_file_length, G_KEY_FILE_NONE, &error);

	if (error != NULL) {
		gchar *parse_name = g_file_get_parse_name (key_file_path);
		g_warning ("Error loading window state data from “%s”: %s", parse_name, error->message);
		g_free (parse_name);

		g_error_free (error);
		goto done;
	}

	/* Load the appropriate keys from the file, ignoring errors */
	width = g_key_file_get_integer (key_file, "main-window", "width", NULL);
	height = g_key_file_get_integer (key_file, "main-window", "height", NULL);
	x = g_key_file_get_integer (key_file, "main-window", "x-position", &error);

	if (error != NULL) {
		x = -1;
		g_clear_error (&error);
	}

	y = g_key_file_get_integer (key_file, "main-window", "y-position", &error);

	if (error != NULL) {
		x = -1;
		g_clear_error (&error);
	}

	/* Make sure the dimensions and position are sane */
	if (width > 1 && height > 1) {
		GdkScreen *screen;
		gint max_width, max_height;

		screen = gtk_widget_get_screen (GTK_WIDGET (self));
		max_width = gdk_screen_get_width (screen);
		max_height = gdk_screen_get_height (screen);

		width = CLAMP (width, 0, max_width);
		height = CLAMP (height, 0, max_height);

		x = CLAMP (x, 0, max_width - width);
		y = CLAMP (y, 0, max_height - height);

		gtk_window_resize (GTK_WINDOW (self), width, height);
	}

	if (x >= 0 && y >= 0) {
		gtk_window_move (GTK_WINDOW (self), x, y);
	}

	/* Maximised? */
	if (g_key_file_get_boolean (key_file, "main-window", "maximized", NULL) == TRUE) {
		gtk_window_maximize (GTK_WINDOW (self));
	}

done:
	g_free (key_file_data);

	if (key_file != NULL) {
		g_key_file_free (key_file);
	}
}

static void
restore_window_state (AlmanahMainWindow *self)
{
	GFile *key_file_path;

	/* Asynchronously load up the state key file */
	key_file_path = get_window_state_file ();
	g_file_load_contents_async (key_file_path, NULL, (GAsyncReadyCallback) restore_window_state_cb, self);
	g_object_unref (key_file_path);
}

void
almanah_main_window_save_current_entry (AlmanahMainWindow *self, gboolean prompt_user)
{
	gboolean entry_exists, existing_entry_is_empty, entry_is_empty;
	GDate date, last_edited;
	AlmanahStorageManager *storage_manager;
	AlmanahMainWindowPrivate *priv = self->priv;
	AlmanahEntryEditability editability;

	g_assert (priv->entry_buffer != NULL);

	/* Don't save if it hasn't been/can't be edited */
	if (priv->current_entry == NULL ||
	    gtk_text_view_get_editable (priv->entry_view) == FALSE ||
	    gtk_text_buffer_get_modified (priv->entry_buffer) == FALSE)
		return;

	storage_manager = almanah_application_dup_storage_manager (ALMANAH_APPLICATION (gtk_window_get_application (GTK_WINDOW (self))));

	almanah_entry_get_date (priv->current_entry, &date);
	editability = almanah_entry_get_editability (priv->current_entry);
	entry_exists = almanah_storage_manager_entry_exists (storage_manager, &date);
	existing_entry_is_empty = almanah_entry_is_empty (priv->current_entry);
	entry_is_empty = (gtk_text_buffer_get_char_count (priv->entry_buffer) == 0) ? TRUE : FALSE;

	/* Make sure they're editable: don't allow entries in the future to be edited,
	 * but allow entries in the past to be added or edited, as long as permission is given.
	 * If an entry is being deleted, permission must be given for that as a priority. */
	if (editability == ALMANAH_ENTRY_FUTURE) {
		/* Can't edit entries for dates in the future */
		goto done;
	} else if (editability == ALMANAH_ENTRY_PAST && (existing_entry_is_empty == FALSE || entry_is_empty == FALSE)) {
		/* Attempting to edit an existing entry in the past */
		gchar date_string[100];
		GtkWidget *dialog;

		/* No-op if we're not allowed to prompt the user. */
		if (prompt_user == FALSE) {
			goto done;
		}

		/* Translators: This is a strftime()-format string for the date to display when asking about editing a diary entry. */
		g_date_strftime (date_string, sizeof (date_string), _("%A, %e %B %Y"), &date);

		dialog = gtk_message_dialog_new (GTK_WINDOW (self),
						 GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
						 _("Are you sure you want to edit this diary entry for %s?"),
						 date_string);
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					GTK_STOCK_EDIT, GTK_RESPONSE_ACCEPT,
					NULL);

		gtk_widget_show_all (dialog);
		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT) {
			/* Cancelled the edit */
			gtk_widget_destroy (dialog);
			goto done;
		}

		gtk_widget_destroy (dialog);
	} else if (entry_exists == TRUE && existing_entry_is_empty == FALSE && entry_is_empty == TRUE) {
		/* Deleting an existing entry */
		gchar date_string[100];
		GtkWidget *dialog;

		/* No-op if we're not allowed to prompt the user. */
		if (prompt_user == FALSE) {
			goto done;
		}

		/* Translators: This is a strftime()-format string for the date to display when asking about deleting a diary entry. */
		g_date_strftime (date_string, sizeof (date_string), _("%A, %e %B %Y"), &date);

		dialog = gtk_message_dialog_new (GTK_WINDOW (self),
						 GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
						 _("Are you sure you want to delete this diary entry for %s?"),
						 date_string);
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					GTK_STOCK_DELETE, GTK_RESPONSE_ACCEPT,
					NULL);

		gtk_widget_show_all (dialog);
		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT) {
			/* Cancelled deletion */
			gtk_widget_destroy (dialog);
			goto done;
		}

		gtk_widget_destroy (dialog);
	}

	/* Save the entry */
	almanah_entry_set_content (priv->current_entry, priv->entry_buffer);
	gtk_text_buffer_set_modified (priv->entry_buffer, FALSE);

	g_date_set_time_t (&last_edited, time (NULL));
	almanah_entry_set_last_edited (priv->current_entry, &last_edited);

	/* Store the entry! */
	almanah_storage_manager_set_entry (storage_manager, priv->current_entry);

	if (entry_is_empty == TRUE) {
		/* Since the entry is empty, remove all the events from the treeview */
		gtk_list_store_clear (priv->event_store);
	}

done:
	g_object_unref (storage_manager);
}

static gboolean
save_entry_timeout_cb (AlmanahMainWindow *self)
{
	almanah_main_window_save_current_entry (self, FALSE);
	return TRUE;
}

void
almanah_main_window_select_date (AlmanahMainWindow *self, GDate *date)
{
	almanah_calendar_button_select_date (self->priv->calendar_button, date);
}

static void
mw_entry_buffer_cursor_position_cb (GObject *object, GParamSpec *pspec, AlmanahMainWindow *main_window)
{
	GtkTextIter iter;
	AlmanahMainWindowPrivate *priv = main_window->priv;
	GSList *_tag_list = NULL, *tag_list = NULL;
	gboolean range_selected = FALSE;
	gboolean bold_toggled = FALSE, italic_toggled = FALSE, underline_toggled = FALSE, hyperlink_toggled = FALSE;

	/* Ensure we don't overwrite current formatting options when characters are being typed.
	 * (Execution of this function will be sandwiched between:
	 * * mw_entry_buffer_insert_text_cb and
	 * * mw_entry_buffer_insert_text_after_cb */
	if (priv->updating_formatting == TRUE)
		return;

	/* Only get the tag list if there's no selection (just an insertion cursor),
	 * since we want the buttons untoggled if there's a selection. */
	range_selected = gtk_text_buffer_get_selection_bounds (priv->entry_buffer, &iter, NULL);
	if (range_selected == FALSE)
		_tag_list = gtk_text_iter_get_tags (&iter);

	/* Block signal handlers for the formatting actions while we're executing,
	 * so formatting doesn't get unwittingly changed. */
	priv->updating_formatting = TRUE;

	tag_list = _tag_list;
	while (tag_list != NULL) {
		GtkTextTag *tag;
		gchar *tag_name;
		GtkToggleAction *action = NULL;

		tag = GTK_TEXT_TAG (tag_list->data);
		g_object_get (tag, "name", &tag_name, NULL);

		/* See if we can do anything with the tag */
		if (tag_name != NULL) {
			if (strcmp (tag_name, "bold") == 0) {
				action = priv->bold_action;
				bold_toggled = TRUE;
			} else if (strcmp (tag_name, "italic") == 0) {
				action = priv->italic_action;
				italic_toggled = TRUE;
			} else if (strcmp (tag_name, "underline") == 0) {
				action = priv->underline_action;
				underline_toggled = TRUE;
			}
		}

		/* Hyperlink? */
		if (ALMANAH_IS_HYPERLINK_TAG (tag)) {
			action = priv->hyperlink_action;
			hyperlink_toggled = TRUE;
		}

		if (action != NULL) {
			/* Force the toggle status on the action */
			gtk_toggle_action_set_active (action, TRUE);
		} else if (tag_name == NULL || strcmp (tag_name, "gtkspell-misspelled") != 0) {
			/* Print a warning about the unknown tag */
			g_warning (_("Unknown or duplicate text tag \"%s\" in entry. Ignoring."), tag_name);
		}

		g_free (tag_name);
		tag_list = tag_list->next;
	}

	g_slist_free (_tag_list);

	if (range_selected == FALSE) {
		/* Untoggle the remaining actions */
		if (bold_toggled == FALSE)
			gtk_toggle_action_set_active (priv->bold_action, FALSE);
		if (italic_toggled == FALSE)
			gtk_toggle_action_set_active (priv->italic_action, FALSE);
		if (underline_toggled == FALSE)
			gtk_toggle_action_set_active (priv->underline_action, FALSE);

		if (hyperlink_toggled == FALSE) {
			gtk_toggle_action_set_active (priv->hyperlink_action, FALSE);
		}
	}

	/* Unblock signals */
	priv->updating_formatting = FALSE;
}

static void
mw_entry_buffer_insert_text_cb (GtkTextBuffer *text_buffer, GtkTextIter *start, gchar *text, gint len, AlmanahMainWindow *main_window)
{
	AlmanahMainWindowPrivate *priv = main_window->priv;

	priv->updating_formatting = TRUE;

	priv->pending_bold_active = gtk_toggle_action_get_active (priv->bold_action);
	priv->pending_italic_active = gtk_toggle_action_get_active (priv->italic_action);
	priv->pending_underline_active = gtk_toggle_action_get_active (priv->underline_action);
}

static void
mw_entry_buffer_insert_text_after_cb (GtkTextBuffer *text_buffer, GtkTextIter *end, gchar *text, gint len, AlmanahMainWindow *main_window)
{
	GtkTextIter start;
	AlmanahMainWindowPrivate *priv = main_window->priv;

	start = *end;
	gtk_text_iter_backward_chars (&start, len);

	if (priv->pending_bold_active == TRUE)
		gtk_text_buffer_apply_tag_by_name (text_buffer, "bold", &start, end);
	if (priv->pending_italic_active == TRUE)
		gtk_text_buffer_apply_tag_by_name (text_buffer, "italic", &start, end);
	if (priv->pending_underline_active == TRUE)
		gtk_text_buffer_apply_tag_by_name (text_buffer, "underline", &start, end);

	priv->updating_formatting = FALSE;
}

static void
mw_entry_buffer_has_selection_cb (GObject *object, GParamSpec *pspec, AlmanahMainWindow *main_window)
{
	gboolean has_selection = gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (object));

	gtk_action_set_sensitive (main_window->priv->cut_action, has_selection);
	gtk_action_set_sensitive (main_window->priv->copy_action, has_selection);
	gtk_action_set_sensitive (main_window->priv->delete_action, has_selection);
}

static gboolean
mw_delete_event_cb (GtkWindow *window, gpointer user_data)
{
	almanah_main_window_save_current_entry (ALMANAH_MAIN_WINDOW (window), TRUE);
	save_window_state (ALMANAH_MAIN_WINDOW (window));

	gtk_widget_destroy (GTK_WIDGET (window));

	return TRUE;
}

void
mw_cut_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	GtkClipboard *clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (GTK_WIDGET (main_window)), GDK_SELECTION_CLIPBOARD);
	gtk_text_buffer_cut_clipboard (main_window->priv->entry_buffer, clipboard, TRUE);
}

void
mw_copy_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	GtkClipboard *clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (GTK_WIDGET (main_window)), GDK_SELECTION_CLIPBOARD);
	gtk_text_buffer_copy_clipboard (main_window->priv->entry_buffer, clipboard);
}

void
mw_paste_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	GtkClipboard *clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (GTK_WIDGET (main_window)), GDK_SELECTION_CLIPBOARD);
	gtk_text_buffer_paste_clipboard (main_window->priv->entry_buffer, clipboard, NULL, TRUE);
}

void
mw_delete_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	gtk_text_buffer_delete_selection (main_window->priv->entry_buffer, TRUE, TRUE);
}

void
mw_insert_time_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	gchar time_string[100];
	time_t time_struct;

	time_struct = time (NULL);
	strftime (time_string, sizeof (time_string), "%X", localtime (&time_struct));
	gtk_text_buffer_insert_at_cursor (main_window->priv->entry_buffer, time_string, -1);
}

void
mw_important_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	almanah_entry_set_is_important (main_window->priv->current_entry, gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

void
mw_select_date_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	AlmanahDateEntryDialog *dialog = almanah_date_entry_dialog_new ();

	almanah_calendar_button_popdown (main_window->priv->calendar_button);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (main_window));
	gtk_widget_show_all (GTK_WIDGET (dialog));
	if (almanah_date_entry_dialog_run (dialog) == TRUE) {
		GDate new_date;

		/* Switch to the specified date */
		almanah_date_entry_dialog_get_date (dialog, &new_date);
		almanah_main_window_select_date (main_window, &new_date);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
apply_formatting (AlmanahMainWindow *self, const gchar *tag_name, gboolean applying)
{
	AlmanahMainWindowPrivate *priv = self->priv;
	GtkTextIter start, end;

	/* Make sure we don't muck up the formatting when the actions are having
	 * their sensitivity set by the code. */
	if (priv->updating_formatting == TRUE)
		return;

	gtk_text_buffer_get_selection_bounds (priv->entry_buffer, &start, &end);
	if (applying == TRUE)
		gtk_text_buffer_apply_tag_by_name (priv->entry_buffer, tag_name, &start, &end);
	else
		gtk_text_buffer_remove_tag_by_name (priv->entry_buffer, tag_name, &start, &end);
	gtk_text_buffer_set_modified (priv->entry_buffer, TRUE);
}

static void
mw_bold_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window)
{
	apply_formatting (main_window, "bold", gtk_toggle_action_get_active (action));
}

static void
mw_italic_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window)
{
	apply_formatting (main_window, "italic", gtk_toggle_action_get_active (action));
}

static void
mw_underline_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window)
{
	apply_formatting (main_window, "underline", gtk_toggle_action_get_active (action));
}

static gboolean
hyperlink_tag_event_cb (GtkTextTag *tag, GObject *object, GdkEvent *event, GtkTextIter *iter, AlmanahMainWindow *self)
{
	AlmanahHyperlinkTag *hyperlink_tag = ALMANAH_HYPERLINK_TAG (tag);

	/* Open the hyperlink if it's control-clicked */
	if (event->type == GDK_BUTTON_RELEASE && event->button.state & GDK_CONTROL_MASK) {
		const gchar *uri;
		GError *error = NULL;

		uri = almanah_hyperlink_tag_get_uri (hyperlink_tag);

		/* Attempt to open the URI */
		gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (self)), uri, gdk_event_get_time (event), &error);

		if (error != NULL) {
			/* Error */
			GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (self),
			                                            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
			                                            _("Error opening URI"));
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			g_error_free (error);
		}

		return TRUE;
	}

	return FALSE;
}

static void
mw_hyperlink_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *self)
{
	AlmanahMainWindowPrivate *priv = self->priv;
	GtkTextIter start, end;

	/* Make sure we don't muck up the formatting when the actions are having
	 * their sensitivity set by the code. */
	if (priv->updating_formatting == TRUE)
		return;

	gtk_text_buffer_get_selection_bounds (priv->entry_buffer, &start, &end);

	if (gtk_toggle_action_get_active (action) == TRUE) {
		/* Add a new hyperlink on the selected text */
		AlmanahUriEntryDialog *uri_entry_dialog;

		/* Get a URI from the user */
		uri_entry_dialog = almanah_uri_entry_dialog_new ();
		gtk_window_set_transient_for (GTK_WINDOW (uri_entry_dialog), GTK_WINDOW (self));
		gtk_widget_show_all (GTK_WIDGET (uri_entry_dialog));

		if (almanah_uri_entry_dialog_run (uri_entry_dialog) == TRUE) {
			GtkTextTag *tag;
			GtkTextTagTable *table;

			/* Create and apply a new anonymous tag */
			tag = GTK_TEXT_TAG (almanah_hyperlink_tag_new (almanah_uri_entry_dialog_get_uri (uri_entry_dialog)));

			table = gtk_text_buffer_get_tag_table (priv->entry_buffer);
			gtk_text_tag_table_add (table, tag);

			gtk_text_buffer_apply_tag (priv->entry_buffer, tag, &start, &end);

			/* Connect up events */
			g_signal_connect (tag, "event", (GCallback) hyperlink_tag_event_cb, self);

			/* The text tag table keeps a reference */
			g_object_unref (tag);
		}

		gtk_widget_destroy (GTK_WIDGET (uri_entry_dialog));
	} else {
		GtkTextIter iter = start;
		GSList *tags, *i;

		/* Remove all hyperlinks which are active at the start iter. This covers the case of hyperlinks which span more than the
		 * selected text (i.e. begin before the start iter and end after the end iter). All other spanning hyperlinks will have an end point
		 * inside the selected text, and will be caught below. */
		tags  = gtk_text_iter_get_tags (&start);

		for (i = tags; i != NULL; i = i->next) {
			GtkTextTag *tag = GTK_TEXT_TAG (i->data);

			if (ALMANAH_IS_HYPERLINK_TAG (tag)) {
				GtkTextIter tag_start = start, tag_end = start;

				if (gtk_text_iter_backward_to_tag_toggle (&tag_start, tag) == TRUE &&
				    gtk_text_iter_forward_to_tag_toggle (&tag_end, tag) == TRUE) {
					gtk_text_buffer_remove_tag (priv->entry_buffer, tag, &tag_start, &tag_end);
				}
			}
		}

		g_slist_free (tags);

		/* Remove all hyperlinks which span the selected text and have an end point inside the selected text */
		while (gtk_text_iter_forward_to_tag_toggle (&iter, NULL) == TRUE) {
			/* Break once we've passed the end of the selected text range */
			if (gtk_text_iter_compare (&iter, &end) > 0) {
				break;
			}

			tags = gtk_text_iter_get_toggled_tags (&iter, FALSE);

			for (i = tags; i != NULL; i = i->next) {
				if (ALMANAH_IS_HYPERLINK_TAG (i->data)) {
					gtk_text_buffer_remove_tag (priv->entry_buffer, GTK_TEXT_TAG (i->data), &start, &end);
				}
			}

			g_slist_free (tags);
		}
	}

	gtk_text_buffer_set_modified (priv->entry_buffer, TRUE);
}

void
mw_jump_to_today_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	GDate current_date;
	g_date_set_time_t (&current_date, time (NULL));
	almanah_calendar_button_select_date (main_window->priv->calendar_button, &current_date);
}

void
mw_old_entries_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	/* TODO: Show the old entries */
	g_debug ("Old entries clicked, but nothing implemented yet...");
}

/**
 * Just copied from GtkMenuToolButton (menu_position_func),
 * but using the button widget (not the arrow widget) for
 * menu position calc, and removing the code for a vertical
 * toolbar situation.
 *
 * Copyright (C) 2003 Ricardo Fernandez Pascual
 * Copyright (C) 2004 Paolo Borelli
 * Licensed under the GPL v2
 */
static void
mw_font_style_menu_position_func (GtkMenu *menu, int *x, int *y, gboolean *push_in, GtkMenuToolButton *button)
{
	GtkAllocation arrow_allocation;
	GtkWidget *widget = GTK_WIDGET (button);
	GtkRequisition menu_req;
	GtkTextDirection direction;
	GdkRectangle monitor;
	gint monitor_num;
	GdkScreen *screen;
	GdkWindow *window;
	GtkAllocation allocation;

	gtk_widget_get_preferred_size (GTK_WIDGET (menu), &menu_req, NULL);
	direction = gtk_widget_get_direction (widget);
	window = gtk_widget_get_window (widget);

	screen = gtk_widget_get_screen (GTK_WIDGET (menu));
	monitor_num = gdk_screen_get_monitor_at_window (screen, window);
	if (monitor_num < 0)
		monitor_num = 0;
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	gtk_widget_get_allocation (widget, &allocation);

	gdk_window_get_origin (window, x, y);
	*x += allocation.x;
	*y += allocation.y;

	if (direction == GTK_TEXT_DIR_LTR)
		*x += MAX (allocation.width - menu_req.width, 0);
	else if (menu_req.width > allocation.width)
		*x -= menu_req.width - allocation.width;

	if ((*y + arrow_allocation.height + menu_req.height) <= monitor.y + monitor.height)
		*y += allocation.height;
	else if ((*y - menu_req.height) >= monitor.y)
		*y -= menu_req.height;
	else if (monitor.y + monitor.height - (*y + arrow_allocation.height) > *y)
		*y += allocation.height;
	else
		*y -= menu_req.height;

	*push_in = FALSE;
}

void
mw_font_style_activate_cb (AlmanahFontStyleMenuAction *action, AlmanahMainWindow *main_window)
{
	AlmanahMainWindowPrivate *priv = ALMANAH_MAIN_WINDOW_GET_PRIVATE(main_window);
	GtkMenu *menu = NULL;

	if (GTK_IS_MENU_TOOL_BUTTON (priv->font_style_tool_button)) {
		menu = GTK_MENU (gtk_menu_tool_button_get_menu (priv->font_style_tool_button));
	}

	if (GTK_IS_MENU (menu)) {
		gtk_menu_popup (menu, NULL, NULL,
				(GtkMenuPositionFunc) mw_font_style_menu_position_func,
				priv->font_style_tool_button,
				0,
				gtk_get_current_event_time());
	}
}

static GtkMenuToolButton *
mw_get_font_style_tool_button_from_action (GtkAction *action)
{
	GSList *proxies;
	GtkMenuToolButton *tool_button = NULL;

	proxies = gtk_action_get_proxies (GTK_ACTION (action));
	while (proxies != NULL) {
		if (GTK_IS_MENU_TOOL_BUTTON (proxies->data)) {
			tool_button = GTK_MENU_TOOL_BUTTON (proxies->data);
		}

		proxies = proxies->next;
	}

	return tool_button;
}

static void
clear_factory_events (AlmanahMainWindow *self, AlmanahEventFactoryType type_id)
{
	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL (self->priv->event_store);

	g_debug ("Removing events belonging to factory %u from the list store...", type_id);

	if (gtk_tree_model_get_iter_first (model, &iter) == FALSE)
		return;

	while (TRUE) {
		AlmanahEventFactoryType row_type_id;

		gtk_tree_model_get (model, &iter, 2, &row_type_id, -1);

		if (row_type_id == type_id) {
			AlmanahEvent *event;
			gtk_tree_model_get (model, &iter, 0, &event, -1);
			g_debug ("\t%s", almanah_event_format_value (event));

			if (gtk_list_store_remove (GTK_LIST_STORE (model), &iter) == FALSE)
				break;
		} else if (gtk_tree_model_iter_next (model, &iter) == FALSE) {
			/* Come to the end of the list */
			break;
		}
	}

	g_debug ("Finished removing events.");
}

static void
mw_events_updated_cb (AlmanahEventManager *event_manager, AlmanahEventFactoryType type_id, AlmanahMainWindow *main_window)
{
	AlmanahMainWindowPrivate *priv = main_window->priv;
	GSList *_events, *events;
	GDate date;
	guint events_count = 0;
	gchar *events_text;

	almanah_calendar_button_get_date (main_window->priv->calendar_button, &date);
	_events = almanah_event_manager_get_events (event_manager, type_id, &date);

	/* Clear all the events generated by this factory out of the list store first */
	clear_factory_events (main_window, type_id);

	g_debug ("Adding events from factory %u to the list store...", type_id);

	for (events = _events; events != NULL; events = g_slist_next (events)) {
		GtkTreeIter iter;
		AlmanahEvent *event = events->data;
		gchar *event_time;

		g_debug ("\t%s", almanah_event_format_value (event));

		/* Translators: this is an event source name (like Calendar appointment) and the time when the event takes place */
		event_time = g_strdup_printf (_("%s @ %s"), almanah_event_format_time (event), almanah_event_get_name (event));

		gtk_list_store_append (priv->event_store, &iter);
		gtk_list_store_set (priv->event_store, &iter,
				    0, event,
				    1, almanah_event_get_icon_name (event),
				    2, type_id,
				    3, almanah_event_format_value (event),
				    4, g_strdup_printf ("<small>%s</small>", event_time),
				    -1);

		events_count++;

		g_object_unref (event);
		g_free (event_time);
	}

	events_text = g_strdup_printf ("%u", events_count);
	gtk_label_set_label (priv->events_count_label, events_text);
	g_free (events_text);

	if (events_count > 0) {
		gtk_widget_set_sensitive (priv->events_expander, TRUE);
	} else {
		gtk_expander_set_expanded (GTK_EXPANDER (priv->events_expander), FALSE);
		gtk_widget_set_sensitive (priv->events_expander, FALSE);
	}

	g_debug ("Finished adding events.");

	g_slist_free (_events);
}

void
mw_calendar_day_selected_cb (AlmanahCalendarButton *calendar_button, AlmanahMainWindow *main_window)
{
	AlmanahApplication *application;
	AlmanahStorageManager *storage_manager;
	AlmanahEventManager *event_manager;
	GDate calendar_date;
#ifdef ENABLE_SPELL_CHECKING
	GtkSpell *gtkspell;
#endif /* ENABLE_SPELL_CHECKING */
	AlmanahMainWindowPrivate *priv = main_window->priv;
	AlmanahEntry *entry;

	/* Set up */
	application = ALMANAH_APPLICATION (gtk_window_get_application (GTK_WINDOW (main_window)));

	/* Save the previous entry */
	almanah_main_window_save_current_entry (main_window, TRUE);

	/* Update the date label */
	almanah_calendar_button_get_date (main_window->priv->calendar_button, &calendar_date);

	/* Update the entry */
	storage_manager = almanah_application_dup_storage_manager (application);
	entry = almanah_storage_manager_get_entry (storage_manager, &calendar_date);
	g_object_unref (storage_manager);

	if (entry == NULL)
		entry = almanah_entry_new (&calendar_date);
	set_current_entry (main_window, entry);
	g_object_unref (entry);

	gtk_text_view_set_editable (priv->entry_view, almanah_entry_get_editability (priv->current_entry) != ALMANAH_ENTRY_FUTURE ? TRUE : FALSE);
	gtk_action_set_sensitive (priv->important_action, almanah_entry_get_editability (priv->current_entry) != ALMANAH_ENTRY_FUTURE ? TRUE : FALSE);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (priv->important_action), almanah_entry_is_important (priv->current_entry));

	/* Prepare for the possibility of failure --- do as much of the general interface changes as possible first */
	gtk_list_store_clear (priv->event_store);

	if (almanah_entry_is_empty (priv->current_entry) == FALSE) {
		GError *error = NULL;

		gtk_text_buffer_set_text (priv->entry_buffer, "", 0);
		if (almanah_entry_get_content (priv->current_entry, priv->entry_buffer, FALSE, &error) == FALSE) {
			GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
								    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
								    _("Entry content could not be loaded"));
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			g_error_free (error);

			/* Make sure the interface is left in a decent state before we return */
			gtk_text_view_set_editable (priv->entry_view, FALSE);

			return;
		}
	} else {
		/* Set the buffer to be empty */
		gtk_text_buffer_set_text (priv->entry_buffer, "", -1);
	}

#ifdef ENABLE_SPELL_CHECKING
	/* Ensure the spell-checking is updated */
	gtkspell = gtkspell_get_from_text_view (priv->entry_view);
	if (gtkspell != NULL) {
		gtkspell_recheck_all (gtkspell);
		gtk_widget_queue_draw (GTK_WIDGET (priv->entry_view));
	}
#endif /* ENABLE_SPELL_CHECKING */

	/* Unset the modification bit on the text buffer last, so that we can be sure it's unset */
	gtk_text_buffer_set_modified (priv->entry_buffer, FALSE);

	/* List the entry's events */
	event_manager = almanah_application_dup_event_manager (application);
	almanah_event_manager_query_events (event_manager, ALMANAH_EVENT_FACTORY_UNKNOWN, &calendar_date);
	g_object_unref (event_manager);
}

void
mw_events_tree_view_row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, AlmanahMainWindow *main_window)
{
	AlmanahEvent *event;
	GtkTreeIter iter;

	gtk_tree_model_get_iter (GTK_TREE_MODEL (main_window->priv->event_store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (main_window->priv->event_store), &iter,
			    0, &event,
			    -1);

	/* NOTE: event types should display their own errors, so one won't be displayed here. */
	almanah_event_view (event, GTK_WINDOW (main_window));
}

static void
mw_menu_button_popup_visible_cb (GtkWidget *menu, GParamSpec *pspec, GtkWidget *button)
{
	/* Set on/off the active menu style when the menu is visible */
	if (gtk_widget_get_visible (menu))
		gtk_style_context_add_class (gtk_widget_get_style_context (button), "active-menu");
	else
		gtk_style_context_remove_class (gtk_widget_get_style_context (button), "active-menu");
}

static void
mw_setup_toolbar (AlmanahMainWindow *main_window, AlmanahApplication *application, GtkToolbar *toolbar, GtkAction *today_action, GtkAction *select_date_action, GtkWidget *menu_popup)
{
	GtkToolItem *calendar_button_item, *separator, *menu_button_item;
	GtkWidget *menu_button, *menu_button_image;
	AlmanahStorageManager *storage_manager;

	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (toolbar)), GTK_STYLE_CLASS_MENUBAR);

	/* Insert a dynamic space between the text style and calendar & important.
	 * This can't be done using the <separator/> in the UI file at the moment
	 */
	separator = gtk_separator_tool_item_new ();
	gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (separator), FALSE);
	gtk_tool_item_set_expand (separator, TRUE);
	gtk_toolbar_insert (toolbar, separator, 2);

	/* Setup the calendar button */
	storage_manager = almanah_application_dup_storage_manager (application);
	main_window->priv->calendar_button = ALMANAH_CALENDAR_BUTTON (almanah_calendar_button_new (storage_manager));
	g_object_unref (storage_manager);
	g_signal_connect (main_window->priv->calendar_button, "day-selected", G_CALLBACK (mw_calendar_day_selected_cb), main_window);
	/* Use the same action for the today button in the dropdown window */
	almanah_calendar_button_set_today_action (main_window->priv->calendar_button, today_action);
	almanah_calendar_button_set_select_date_action (main_window->priv->calendar_button, select_date_action);

	/* Insert the calendar button into the toolbar through a GtkToolItem but button style */
	calendar_button_item = gtk_tool_item_new ();
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (calendar_button_item)), GTK_STYLE_CLASS_RAISED);
	gtk_container_add (GTK_CONTAINER (calendar_button_item), GTK_WIDGET (main_window->priv->calendar_button));
	gtk_toolbar_insert (toolbar, calendar_button_item, 3);

	/* Another dynamic separator */
	separator = gtk_separator_tool_item_new ();
	gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (separator), FALSE);
	gtk_tool_item_set_expand (separator, TRUE);
	gtk_container_add (GTK_CONTAINER (toolbar), GTK_WIDGET (separator));

	/* Menu button with the common GNOME applications style (Nautilus, Epiphany, ...) */
	menu_button_item = gtk_tool_item_new ();
	menu_button = gtk_menu_button_new ();
	gtk_menu_button_set_popup (GTK_MENU_BUTTON (menu_button), menu_popup);
	menu_button_image = gtk_image_new_from_icon_name ("emblem-system-symbolic", gtk_toolbar_get_icon_size (toolbar));
	gtk_button_set_image (GTK_BUTTON (menu_button), menu_button_image);
	gtk_container_add (GTK_CONTAINER (menu_button_item), menu_button);
	gtk_container_add (GTK_CONTAINER (toolbar), GTK_WIDGET (menu_button_item));
	g_signal_connect (menu_popup, "notify::visible", G_CALLBACK (mw_menu_button_popup_visible_cb), menu_button);
}

#ifdef ENABLE_SPELL_CHECKING
static void
spell_checking_enabled_changed_cb (GSettings *settings, gchar *key, AlmanahMainWindow *self)
{
	gboolean enabled = g_settings_get_boolean (settings, "spell-checking-enabled");

	g_debug ("spell_checking_enabled_changed_cb called with %u.", enabled);

	if (enabled == TRUE) {
		GError *error = NULL;

		enable_spell_checking (self, &error);

		if (error != NULL) {
			GtkWidget *dialog = gtk_message_dialog_new (NULL,
			                                            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
			                                            _("Spelling checker could not be initialized"));
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			g_error_free (error);
		}
	} else {
		disable_spell_checking (self);
	}
}

static gboolean
enable_spell_checking (AlmanahMainWindow *self, GError **error)
{
	AlmanahApplication *application;
	GSettings *settings;
	GtkSpell *gtkspell;
	gchar *spelling_language;
	GtkTextTagTable *table;
	GtkTextTag *tag;

	/* Bail out if spell checking's already enabled */
	if (gtkspell_get_from_text_view (self->priv->entry_view) != NULL)
		return TRUE;

	/* If spell checking wasn't already enabled, we have a dummy gtkspell-misspelled text tag to destroy */
	table = gtk_text_buffer_get_tag_table (self->priv->entry_buffer);
	tag = gtk_text_tag_table_lookup (table, "gtkspell-misspelled");
	if (tag != NULL)
		gtk_text_tag_table_remove (table, tag);

	/* Get the spell checking language */
	application = ALMANAH_APPLICATION (gtk_window_get_application (GTK_WINDOW (self)));
	settings = almanah_application_dup_settings (application);
	spelling_language = g_settings_get_string (settings, "spelling-language");
	g_object_unref (settings);

	/* Make sure it's either NULL or a proper locale specifier */
	if (spelling_language != NULL && spelling_language[0] == '\0') {
		g_free (spelling_language);
		spelling_language = NULL;
	}

	gtkspell = gtkspell_new_attach (self->priv->entry_view, spelling_language, error);
	g_free (spelling_language);

	if (gtkspell == NULL)
		return FALSE;
	return TRUE;
}

static void
disable_spell_checking (AlmanahMainWindow *self)
{
	GtkSpell *gtkspell;
	GtkTextTagTable *table;
	GtkTextTag *tag;

	gtkspell = gtkspell_get_from_text_view (self->priv->entry_view);
	if (gtkspell != NULL)
		gtkspell_detach (gtkspell);

	/* Remove the old gtkspell-misspelling text tag */
	table = gtk_text_buffer_get_tag_table (self->priv->entry_buffer);
	tag = gtk_text_tag_table_lookup (table, "gtkspell-misspelled");
	if (tag != NULL)
		gtk_text_tag_table_remove (table, tag);

	/* Create a dummy gtkspell-misspelling text tag */
	gtk_text_buffer_create_tag (self->priv->entry_buffer, "gtkspell-misspelled", NULL);
}
#endif /* ENABLE_SPELL_CHECKING */
