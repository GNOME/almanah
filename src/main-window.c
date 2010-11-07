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
#include "main.h"
#include "interface.h"
#include "preferences-dialog.h"
#include "search-dialog.h"
#include "date-entry-dialog.h"
#include "printing.h"
#include "entry.h"
#include "storage-manager.h"
#include "event.h"
#include "import-export-dialog.h"
#include "widgets/calendar.h"

static void almanah_main_window_dispose (GObject *object);
#ifdef ENABLE_SPELL_CHECKING
static void almanah_main_window_finalize (GObject *object);
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
static void mw_events_updated_cb (AlmanahEventManager *event_manager, AlmanahEventFactoryType type_id, AlmanahMainWindow *main_window);
static void mw_events_selection_changed_cb (GtkTreeSelection *tree_selection, AlmanahMainWindow *main_window);
static void mw_events_value_data_cb (GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data);

/* GtkBuilder callbacks */
void mw_calendar_day_selected_cb (GtkCalendar *calendar, AlmanahMainWindow *main_window);
void mw_import_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_export_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_page_setup_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_print_preview_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_print_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_quit_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_cut_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_copy_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_paste_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_delete_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_insert_time_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_important_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_select_date_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_search_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_preferences_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_about_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_jump_to_today_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_events_tree_view_row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, AlmanahMainWindow *main_window);
void mw_view_button_clicked_cb (GtkButton *button, AlmanahMainWindow *main_window);

struct _AlmanahMainWindowPrivate {
	GtkTextView *entry_view;
	GtkTextBuffer *entry_buffer;
	AlmanahCalendar *calendar;
	GtkLabel *date_label;
	GtkButton *view_button;
	GtkListStore *event_store;
	GtkTreeSelection *events_selection;
	GtkTreeViewColumn *event_value_column;
	GtkCellRendererText *event_value_renderer;
	GtkToggleAction *bold_action;
	GtkToggleAction *italic_action;
	GtkToggleAction *underline_action;
	GtkAction *cut_action;
	GtkAction *copy_action;
	GtkAction *delete_action;
	GtkAction *important_action;

	gboolean updating_formatting;
	gboolean pending_bold_active;
	gboolean pending_italic_active;
	gboolean pending_underline_active;

	AlmanahEntry *current_entry; /* whether it's been modified is stored as gtk_text_buffer_get_modified (priv->entry_buffer) */
	gulong current_entry_notify_id; /* signal handler for current_entry::notify */

#ifdef ENABLE_SPELL_CHECKING
	gulong spell_checking_enabled_changed_id; /* signal handler for almanah->settings::changed::spell-checking-enabled */
#endif /* ENABLE_SPELL_CHECKING */
};

G_DEFINE_TYPE (AlmanahMainWindow, almanah_main_window, GTK_TYPE_WINDOW)
#define ALMANAH_MAIN_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_MAIN_WINDOW, AlmanahMainWindowPrivate))

static void
almanah_main_window_class_init (AlmanahMainWindowClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	g_type_class_add_private (klass, sizeof (AlmanahMainWindowPrivate));
	gobject_class->dispose = almanah_main_window_dispose;
#ifdef ENABLE_SPELL_CHECKING
	gobject_class->finalize = almanah_main_window_finalize;
#endif /* ENABLE_SPELL_CHECKING */
}

static void
almanah_main_window_init (AlmanahMainWindow *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_MAIN_WINDOW, AlmanahMainWindowPrivate);

	gtk_window_set_title (GTK_WINDOW (self), _("Almanah Diary"));
	g_signal_connect (self, "delete-event", G_CALLBACK (mw_delete_event_cb), NULL);

#ifdef ENABLE_SPELL_CHECKING
	/* We don't use g_settings_bind() because enabling spell checking could fail, and we need to show an error dialogue */
	self->priv->spell_checking_enabled_changed_id = g_signal_connect (almanah->settings, "changed::spell-checking-enabled",
	                                                                  (GCallback) spell_checking_enabled_changed_cb, self);
#endif /* ENABLE_SPELL_CHECKING */
}

static void
almanah_main_window_dispose (GObject *object)
{
	set_current_entry (ALMANAH_MAIN_WINDOW (object), NULL);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_main_window_parent_class)->dispose (object);
}

#ifdef ENABLE_SPELL_CHECKING
static void
almanah_main_window_finalize (GObject *object)
{
	AlmanahMainWindowPrivate *priv = ALMANAH_MAIN_WINDOW (object)->priv;

	g_signal_handler_disconnect (object, priv->spell_checking_enabled_changed_id);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_main_window_parent_class)->finalize (object);
}
#endif /* ENABLE_SPELL_CHECKING */

AlmanahMainWindow *
almanah_main_window_new (void)
{
	GtkBuilder *builder;
	AlmanahMainWindow *main_window;
	AlmanahMainWindowPrivate *priv;
	GError *error = NULL;
	const gchar *interface_filename = almanah_get_interface_filename ();
	const gchar *object_names[] = {
		"almanah_main_window",
		"almanah_mw_event_store",
		"almanah_mw_view_button_image",
		"almanah_ui_manager",
		NULL
	};

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

	priv = ALMANAH_MAIN_WINDOW (main_window)->priv;

	/* Grab our child widgets */
	priv->entry_view = GTK_TEXT_VIEW (gtk_builder_get_object (builder, "almanah_mw_entry_view"));
	priv->entry_buffer = gtk_text_view_get_buffer (priv->entry_view);
	priv->calendar = ALMANAH_CALENDAR (gtk_builder_get_object (builder, "almanah_mw_calendar"));
	priv->date_label = GTK_LABEL (gtk_builder_get_object (builder, "almanah_mw_date_label"));
	priv->view_button = GTK_BUTTON (gtk_builder_get_object (builder, "almanah_mw_view_button"));
	priv->event_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "almanah_mw_event_store"));
	priv->events_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_builder_get_object (builder, "almanah_mw_events_tree_view")));
	priv->event_value_column = GTK_TREE_VIEW_COLUMN (gtk_builder_get_object (builder, "almanah_mw_event_value_column"));
	priv->event_value_renderer = GTK_CELL_RENDERER_TEXT (gtk_builder_get_object (builder, "almanah_mw_event_value_renderer"));
	priv->bold_action = GTK_TOGGLE_ACTION (gtk_builder_get_object (builder, "almanah_ui_bold"));;
	priv->italic_action = GTK_TOGGLE_ACTION (gtk_builder_get_object (builder, "almanah_ui_italic"));
	priv->underline_action = GTK_TOGGLE_ACTION (gtk_builder_get_object (builder, "almanah_ui_underline"));
	priv->cut_action = GTK_ACTION (gtk_builder_get_object (builder, "almanah_ui_cut"));
	priv->copy_action = GTK_ACTION (gtk_builder_get_object (builder, "almanah_ui_copy"));
	priv->delete_action = GTK_ACTION (gtk_builder_get_object (builder, "almanah_ui_delete"));
	priv->important_action = GTK_ACTION (gtk_builder_get_object (builder, "almanah_ui_important"));

#ifdef ENABLE_SPELL_CHECKING
	/* Set up spell checking, if it's enabled */
	if (g_settings_get_boolean (almanah->settings, "spell-checking-enabled") == TRUE)
		enable_spell_checking (main_window, NULL);
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

	/* Notification for event changes */
	g_signal_connect (almanah->event_manager, "events-updated", G_CALLBACK (mw_events_updated_cb), main_window);

	/* Select the current day and month */
	mw_jump_to_today_activate_cb (NULL, main_window);

	/* Set up the treeview */
	g_signal_connect (priv->events_selection, "changed", G_CALLBACK (mw_events_selection_changed_cb), main_window);
	gtk_tree_view_column_set_cell_data_func (priv->event_value_column, GTK_CELL_RENDERER (priv->event_value_renderer), mw_events_value_data_cb, NULL, NULL);

#ifndef ENABLE_ENCRYPTION
#ifndef ENABLE_SPELL_CHECKING
	/* Remove the "Preferences" entry from the menu */
	gtk_action_set_visible (GTK_ACTION (gtk_builder_get_object (builder, "almanah_ui_preferences")), FALSE);
#endif /* !ENABLE_SPELL_CHECKING */
#endif /* !ENABLE_ENCRYPTION */

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

static void
save_current_entry (AlmanahMainWindow *self)
{
	gboolean entry_exists, existing_entry_is_empty, entry_is_empty;
	GDate date, last_edited;
	AlmanahMainWindowPrivate *priv = self->priv;
	AlmanahEntryEditability editability;

	g_assert (priv->entry_buffer != NULL);

	/* Don't save if it hasn't been/can't be edited */
	if (priv->current_entry == NULL ||
	    gtk_text_view_get_editable (priv->entry_view) == FALSE ||
	    gtk_text_buffer_get_modified (priv->entry_buffer) == FALSE)
		return;

	almanah_entry_get_date (priv->current_entry, &date);
	editability = almanah_entry_get_editability (priv->current_entry);
	entry_exists = almanah_storage_manager_entry_exists (almanah->storage_manager, &date);
	existing_entry_is_empty = almanah_entry_is_empty (priv->current_entry);
	entry_is_empty = (gtk_text_buffer_get_char_count (priv->entry_buffer) == 0) ? TRUE : FALSE;

	/* Make sure they're editable: don't allow entries in the future to be edited,
	 * but allow entries in the past to be added or edited, as long as permission is given.
	 * If an entry is being deleted, permission must be given for that as a priority. */
	if (editability == ALMANAH_ENTRY_FUTURE) {
		/* Can't edit entries for dates in the future */
		return;
	} else if (editability == ALMANAH_ENTRY_PAST && (existing_entry_is_empty == FALSE || entry_is_empty == FALSE)) {
		/* Attempting to edit an existing entry in the past */
		gchar date_string[100];
		GtkWidget *dialog;

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
			return;
		}

		gtk_widget_destroy (dialog);
	} else if (entry_exists == TRUE && existing_entry_is_empty == FALSE && entry_is_empty == TRUE) {
		/* Deleting an existing entry */
		gchar date_string[100];
		GtkWidget *dialog;

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
			return;
		}

		gtk_widget_destroy (dialog);
	}

	/* Save the entry */
	almanah_entry_set_content (priv->current_entry, priv->entry_buffer);
	gtk_text_buffer_set_modified (priv->entry_buffer, FALSE);

	g_date_set_time_t (&last_edited, time (NULL));
	almanah_entry_set_last_edited (priv->current_entry, &last_edited);

	/* Store the entry! */
	almanah_storage_manager_set_entry (almanah->storage_manager, priv->current_entry);

	if (entry_is_empty == TRUE) {
		/* Since the entry is empty, remove all the events from the treeview */
		gtk_list_store_clear (priv->event_store);
	}
}

void
almanah_main_window_select_date (AlmanahMainWindow *self, GDate *date)
{
	almanah_calendar_select_date (self->priv->calendar, date);
}

static void
mw_entry_buffer_cursor_position_cb (GObject *object, GParamSpec *pspec, AlmanahMainWindow *main_window)
{
	GtkTextIter iter;
	AlmanahMainWindowPrivate *priv = main_window->priv;
	GSList *_tag_list = NULL, *tag_list = NULL;
	gboolean range_selected = FALSE;
	gboolean bold_toggled = FALSE, italic_toggled = FALSE, underline_toggled = FALSE;

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
		gchar *tag_name;
		GtkToggleAction *action = NULL;

		g_object_get (tag_list->data, "name", &tag_name, NULL);

		/* See if we can do anything with the tag */
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

		if (action != NULL) {
			/* Force the toggle status on the action */
			gtk_toggle_action_set_active (action, TRUE);
		} else if (strcmp (tag_name, "gtkspell-misspelled") != 0) {
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
	save_current_entry (ALMANAH_MAIN_WINDOW (window));
	save_window_state (ALMANAH_MAIN_WINDOW (window));

	almanah_quit ();

	return TRUE;
}

void
mw_import_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	GtkWidget *dialog = GTK_WIDGET (almanah_import_export_dialog_new (TRUE));
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (main_window));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	/* The dialog destroys itself once done */
	gtk_widget_show_all (dialog);
}

void
mw_export_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	GtkWidget *dialog = GTK_WIDGET (almanah_import_export_dialog_new (FALSE));
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (main_window));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	/* The dialog destroys itself once done */
	gtk_widget_show_all (dialog);
}

void
mw_page_setup_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	almanah_print_page_setup ();
}

void
mw_print_preview_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	almanah_print_entries (TRUE);
}

void
mw_print_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	almanah_print_entries (FALSE);
}

void
mw_quit_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	save_current_entry (main_window);
	almanah_quit ();
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
	if (almanah->date_entry_dialog == NULL)
		almanah->date_entry_dialog = GTK_WIDGET (almanah_date_entry_dialog_new ());

	gtk_widget_show_all (almanah->date_entry_dialog);
	if (almanah_date_entry_dialog_run (ALMANAH_DATE_ENTRY_DIALOG (almanah->date_entry_dialog)) == TRUE) {
		GDate new_date;

		/* Switch to the specified date */
		almanah_date_entry_dialog_get_date (ALMANAH_DATE_ENTRY_DIALOG (almanah->date_entry_dialog), &new_date);
		almanah_main_window_select_date (main_window, &new_date);
	}
}

void
mw_search_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	if (almanah->search_dialog == NULL)
		almanah->search_dialog = GTK_WIDGET (almanah_search_dialog_new ());

	gtk_widget_show_all (almanah->search_dialog);
	gtk_dialog_run (GTK_DIALOG (almanah->search_dialog));
}

void
mw_preferences_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
#ifdef ENABLE_ENCRYPTION
	if (almanah->preferences_dialog == NULL)
		almanah->preferences_dialog = GTK_WIDGET (almanah_preferences_dialog_new ());

	gtk_widget_show_all (almanah->preferences_dialog);
	gtk_dialog_run (GTK_DIALOG (almanah->preferences_dialog));
#endif /* ENABLE_ENCRYPTION */
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

void
mw_about_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
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

	almanah_storage_manager_get_statistics (almanah->storage_manager, &entry_count);
	description = g_strdup_printf (_("A helpful diary keeper, storing %u entries."), entry_count);

	gtk_show_about_dialog (GTK_WINDOW (main_window),
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

void
mw_jump_to_today_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	GDate current_date;
	g_date_set_time_t (&current_date, time (NULL));
	almanah_main_window_select_date (main_window, &current_date);
}

static void
clear_factory_events (AlmanahMainWindow *self, AlmanahEventFactoryType type_id)
{
	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL (self->priv->event_store);

	if (almanah->debug == TRUE)
		g_debug ("Removing events belonging to factory %u from the list store...", type_id);

	if (gtk_tree_model_get_iter_first (model, &iter) == FALSE)
		return;

	while (TRUE) {
		AlmanahEventFactoryType row_type_id;

		gtk_tree_model_get (model, &iter, 2, &row_type_id, -1);

		if (row_type_id == type_id) {
			if (almanah->debug == TRUE) {
				AlmanahEvent *event;
				gtk_tree_model_get (model, &iter, 0, &event, -1);
				g_debug ("\t%s", almanah_event_format_value (event));
			}

			if (gtk_list_store_remove (GTK_LIST_STORE (model), &iter) == FALSE)
				break;
		} else if (gtk_tree_model_iter_next (model, &iter) == FALSE) {
			/* Come to the end of the list */
			break;
		}
	}

	if (almanah->debug == TRUE)
		g_debug ("Finished removing events.");
}

static void
mw_events_updated_cb (AlmanahEventManager *event_manager, AlmanahEventFactoryType type_id, AlmanahMainWindow *main_window)
{
	AlmanahMainWindowPrivate *priv = main_window->priv;
	GSList *_events, *events;
	GDate date;

	almanah_calendar_get_date (main_window->priv->calendar, &date);
	_events = almanah_event_manager_get_events (event_manager, type_id, &date);

	/* Clear all the events generated by this factory out of the list store first */
	clear_factory_events (main_window, type_id);

	if (almanah->debug == TRUE)
		g_debug ("Adding events from factory %u to the list store...", type_id);

	for (events = _events; events != NULL; events = g_slist_next (events)) {
		GtkTreeIter iter;
		AlmanahEvent *event = events->data;

		if (almanah->debug == TRUE)
			g_debug ("\t%s", almanah_event_format_value (event));

		gtk_list_store_append (priv->event_store, &iter);
		gtk_list_store_set (priv->event_store, &iter,
				    0, event,
				    1, almanah_event_get_icon_name (event),
				    2, type_id,
				    -1);

		g_object_unref (event);
	}

	if (almanah->debug == TRUE)
		g_debug ("Finished adding events.");

	g_slist_free (_events);
}

void
mw_calendar_day_selected_cb (GtkCalendar *calendar, AlmanahMainWindow *main_window)
{
	GDate calendar_date;
	gchar calendar_string[100];
#ifdef ENABLE_SPELL_CHECKING
	GtkSpell *gtkspell;
#endif /* ENABLE_SPELL_CHECKING */
	AlmanahMainWindowPrivate *priv = main_window->priv;
	AlmanahEntry *entry;

	/* Save the previous entry */
	save_current_entry (main_window);

	/* Update the date label */
	almanah_calendar_get_date (main_window->priv->calendar, &calendar_date);

	/* Translators: This is a strftime()-format string for the date displayed at the top of the main window. */
	g_date_strftime (calendar_string, sizeof (calendar_string), _("%A, %e %B %Y"), &calendar_date);
	gtk_label_set_markup (priv->date_label, calendar_string);

	/* Update the entry */
	entry = almanah_storage_manager_get_entry (almanah->storage_manager, &calendar_date);
	if (entry == NULL)
		entry = almanah_entry_new (&calendar_date);
	set_current_entry (main_window, entry);
	g_object_unref (entry);

	gtk_text_view_set_editable (priv->entry_view, almanah_entry_get_editability (priv->current_entry) != ALMANAH_ENTRY_FUTURE ? TRUE : FALSE);
	gtk_action_set_sensitive (priv->important_action, almanah_entry_get_editability (priv->current_entry) != ALMANAH_ENTRY_FUTURE ? TRUE : FALSE);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (priv->important_action), almanah_entry_is_important (priv->current_entry));

	/* Prepare for the possibility of failure --- do as much of the general interface changes as possible first */
	gtk_list_store_clear (priv->event_store);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->view_button), FALSE);

	if (almanah_entry_is_empty (priv->current_entry) == FALSE) {
		GError *error = NULL;

		gtk_text_buffer_set_text (priv->entry_buffer, "", 0);
		if (almanah_entry_get_content (priv->current_entry, priv->entry_buffer, FALSE, &error) == FALSE) {
			GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (almanah->main_window),
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
	almanah_event_manager_query_events (almanah->event_manager, ALMANAH_EVENT_FACTORY_UNKNOWN, &calendar_date);
}

static void
mw_events_selection_changed_cb (GtkTreeSelection *tree_selection, AlmanahMainWindow *main_window)
{
	AlmanahMainWindowPrivate *priv = main_window->priv;
	gtk_widget_set_sensitive (GTK_WIDGET (priv->view_button), (gtk_tree_selection_count_selected_rows (tree_selection) == 0) ? FALSE : TRUE);
}

static void
mw_events_value_data_cb (GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	const gchar *new_value;
	AlmanahEvent *event;

	/* TODO: Should really create a new model to render AlmanahEvents */

	gtk_tree_model_get (model, iter,
			    0, &event,
			    -1);

	new_value = almanah_event_format_value (event);
	g_object_set (renderer, "text", new_value, NULL);
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
	almanah_event_view (event);
}

void
mw_view_button_clicked_cb (GtkButton *button, AlmanahMainWindow *main_window)
{
	GList *events;
	GtkTreeModel *model;

	events = gtk_tree_selection_get_selected_rows (main_window->priv->events_selection, &model);

	for (; events != NULL; events = events->next) {
		AlmanahEvent *event;
		GtkTreeIter iter;

		gtk_tree_model_get_iter (model, &iter, (GtkTreePath*) events->data);
		gtk_tree_model_get (model, &iter,
				    0, &event,
				    -1);

		/* NOTE: event types should display their own errors, so one won't be displayed here. */
		almanah_event_view (event);

		gtk_tree_path_free (events->data);
	}

	g_list_free (events);
}

#ifdef ENABLE_SPELL_CHECKING
static void
spell_checking_enabled_changed_cb (GSettings *settings, gchar *key, AlmanahMainWindow *self)
{
	gboolean enabled = g_settings_get_boolean (settings, "spell-checking-enabled");

	if (almanah->debug)
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
	spelling_language = g_settings_get_string (almanah->settings, "spelling-language");

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
