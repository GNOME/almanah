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
#include <gconf/gconf.h>
#ifdef ENABLE_SPELL_CHECKING
#include <gtkspell/gtkspell.h>
#endif /* ENABLE_SPELL_CHECKING */

#include "main-window.h"
#include "main.h"
#include "interface.h"
#include "add-definition-dialog.h"
#include "preferences-dialog.h"
#include "search-dialog.h"
#include "date-entry-dialog.h"
#include "printing.h"
#include "entry.h"
#include "storage-manager.h"
#include "event.h"
#include "definition.h"
#include "definition-manager-window.h"
#include "import-export-dialog.h"
#include "widgets/calendar.h"

static void almanah_main_window_dispose (GObject *object);
static void save_window_state (AlmanahMainWindow *self);
static void restore_window_state (AlmanahMainWindow *self);
static gboolean mw_delete_event_cb (GtkWindow *window, gpointer user_data);
static void mw_entry_buffer_cursor_position_cb (GObject *object, GParamSpec *pspec, AlmanahMainWindow *main_window);
static void mw_entry_buffer_insert_text_cb (GtkTextBuffer *text_buffer, GtkTextIter *start, gchar *text, gint len, AlmanahMainWindow *main_window);
static void mw_entry_buffer_insert_text_after_cb (GtkTextBuffer *text_buffer, GtkTextIter *start, gchar *text, gint len, AlmanahMainWindow *main_window);
static void mw_entry_buffer_has_selection_cb (GObject *object, GParamSpec *pspec, AlmanahMainWindow *main_window);
static void mw_entry_buffer_apply_tag_cb (GtkTextBuffer *buffer, GtkTextTag *tag, GtkTextIter *start_iter, GtkTextIter *end_iter, AlmanahMainWindow *main_window);
static void mw_bold_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window);
static void mw_italic_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window);
static void mw_underline_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window);
static void mw_events_updated_cb (AlmanahEventManager *event_manager, AlmanahEventFactoryType type_id, AlmanahMainWindow *main_window);
static void mw_events_selection_changed_cb (GtkTreeSelection *tree_selection, AlmanahMainWindow *main_window);
static void mw_events_value_data_cb (GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data);
static void mw_definition_removed_cb (AlmanahStorageManager *storage_manager, const gchar *definition_text, AlmanahMainWindow *main_window);

/* GtkBuilder callbacks */
void mw_calendar_day_selected_cb (GtkCalendar *calendar, AlmanahMainWindow *main_window);
void mw_import_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
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
void mw_add_definition_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_remove_definition_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_view_definitions_activate_cb (GtkAction *action, AlmanahMainWindow *main_window);
void mw_events_tree_view_row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, AlmanahMainWindow *main_window);
gboolean mw_entry_view_focus_out_event_cb (GtkWidget *entry_view, GdkEventFocus *event, AlmanahMainWindow *main_window);
void mw_view_button_clicked_cb (GtkButton *button, AlmanahMainWindow *main_window);

struct _AlmanahMainWindowPrivate {
	GtkTextView *entry_view;
	GtkTextBuffer *entry_buffer;
	AlmanahCalendar *calendar;
	GtkLabel *date_label;
	GtkButton *view_button;
	GtkAction *add_action;
	GtkAction *remove_action;
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

	AlmanahEntry *current_entry;
};

G_DEFINE_TYPE (AlmanahMainWindow, almanah_main_window, GTK_TYPE_WINDOW)
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

	if (priv->current_entry != NULL)
		g_object_unref (priv->current_entry);
	priv->current_entry = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_main_window_parent_class)->dispose (object);
}

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
	priv->add_action = GTK_ACTION (gtk_builder_get_object (builder, "almanah_ui_add_definition"));
	priv->remove_action = GTK_ACTION (gtk_builder_get_object (builder, "almanah_ui_remove_definition"));
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
	if (gconf_client_get_bool (almanah->gconf_client, "/apps/almanah/spell_checking_enabled", NULL) == TRUE)
		almanah_main_window_enable_spell_checking (main_window, NULL);
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

	/* Get notified of applied tags so we can check if referenced definitions still exist */
	g_signal_connect_after (priv->entry_buffer, "apply-tag", G_CALLBACK (mw_entry_buffer_apply_tag_cb), main_window);

	/* Connect up the formatting actions */
	g_signal_connect (priv->bold_action, "toggled", G_CALLBACK (mw_bold_toggled_cb), main_window);
	g_signal_connect (priv->italic_action, "toggled", G_CALLBACK (mw_italic_toggled_cb), main_window);
	g_signal_connect (priv->underline_action, "toggled", G_CALLBACK (mw_underline_toggled_cb), main_window);

	/* Notification for event changes */
	g_signal_connect (almanah->event_manager, "events-updated", G_CALLBACK (mw_events_updated_cb), main_window);

	/* Notification for changes to definitions in the database */
	g_signal_connect (almanah->storage_manager, "definition-removed", G_CALLBACK (mw_definition_removed_cb), main_window);

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
save_window_state (AlmanahMainWindow *self)
{
	GdkWindow *window;
	GdkWindowState state;
	gint width, height, x, y;

	window = gtk_widget_get_window (GTK_WIDGET (self));
	state = gdk_window_get_state (window);
	gconf_client_set_bool (almanah->gconf_client, "/apps/almanah/state/main_window_maximized", state & GDK_WINDOW_STATE_MAXIMIZED ? TRUE : FALSE, NULL);

	/* If we're maximised, don't bother saving size/position */
	if (state & GDK_WINDOW_STATE_MAXIMIZED)
		return;

	/* Save the window dimensions */
	gtk_window_get_size (GTK_WINDOW (self), &width, &height);

	gconf_client_set_int (almanah->gconf_client, "/apps/almanah/state/main_window_width", width, NULL);
	gconf_client_set_int (almanah->gconf_client, "/apps/almanah/state/main_window_height", height, NULL);

	/* Save the window position */
	gtk_window_get_position (GTK_WINDOW (self), &x, &y);

	gconf_client_set_int (almanah->gconf_client, "/apps/almanah/state/main_window_x_position", x, NULL);
	gconf_client_set_int (almanah->gconf_client, "/apps/almanah/state/main_window_y_position", y, NULL);
}

static void
restore_window_state (AlmanahMainWindow *self)
{
	gint width, height, x, y;

	width = gconf_client_get_int (almanah->gconf_client, "/apps/almanah/state/main_window_width", NULL);
	height = gconf_client_get_int (almanah->gconf_client, "/apps/almanah/state/main_window_height", NULL);
	x = gconf_client_get_int (almanah->gconf_client, "/apps/almanah/state/main_window_x_position", NULL);
	y = gconf_client_get_int (almanah->gconf_client, "/apps/almanah/state/main_window_y_position", NULL);

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

		gtk_window_set_default_size (GTK_WINDOW (self), width, height);
	}

	gtk_window_move (GTK_WINDOW (self), x, y);

	/* Maximised? */
	if (gconf_client_get_bool (almanah->gconf_client, "/apps/almanah/state/main_window_maximized", NULL) == TRUE)
		gtk_window_maximize (GTK_WINDOW (self));
}

static void
save_current_entry (AlmanahMainWindow *self)
{
	gboolean entry_exists, entry_is_empty;
	GDate date;
	AlmanahMainWindowPrivate *priv = self->priv;
	AlmanahEntryEditability editability;

	g_assert (priv->entry_buffer != NULL);

	/* Don't save if it hasn't been/can't be edited */
	if (priv->current_entry == NULL ||
	    gtk_text_view_get_editable (priv->entry_view) == FALSE ||
	    gtk_text_buffer_get_modified (priv->entry_buffer) == FALSE)
		return;

	/* Save the entry */
	almanah_entry_set_content (priv->current_entry, priv->entry_buffer);
	gtk_text_buffer_set_modified (priv->entry_buffer, FALSE);

	almanah_entry_get_date (priv->current_entry, &date);
	editability = almanah_entry_get_editability (priv->current_entry);
	entry_exists = almanah_storage_manager_entry_exists (almanah->storage_manager, &date);
	entry_is_empty = almanah_entry_is_empty (priv->current_entry);

	/* Make sure they're editable: don't allow entries in the future to be edited,
	 * but allow entries in the past to be added or edited, as long as permission is given.
	 * If an entry is being deleted, permission must be given for that as a priority. */
	if (editability == ALMANAH_ENTRY_FUTURE) {
		/* Can't edit entries for dates in the future */
		return;
	} else if (editability == ALMANAH_ENTRY_PAST && entry_is_empty == FALSE) {
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
	} else if (entry_exists == TRUE && entry_is_empty == TRUE) {
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

	/* Store the entry! */
	almanah_storage_manager_set_entry (almanah->storage_manager, priv->current_entry);

	/* Mark the day on the calendar if the entry was non-empty (and deleted)
	 * and update the state of the add definition action. */
	if (entry_is_empty == TRUE) {
		gtk_calendar_unmark_day (GTK_CALENDAR (priv->calendar), g_date_get_day (&date));

		/* Since the entry is empty, remove all the events from the treeview */
		gtk_list_store_clear (priv->event_store);
	} else {
		gtk_calendar_mark_day (GTK_CALENDAR (priv->calendar), g_date_get_day (&date));
	}
}

static void
add_definition_to_current_entry (AlmanahMainWindow *self)
{
	AlmanahMainWindowPrivate *priv = self->priv;
	GtkTextIter start_iter, end_iter;
	gchar *text;
	AlmanahDefinition *definition;

	g_assert (priv->entry_buffer != NULL);
	g_assert (gtk_text_buffer_get_char_count (priv->entry_buffer) != 0);

	if (gtk_text_buffer_get_selection_bounds (priv->entry_buffer, &start_iter, &end_iter) == FALSE)
		return;

	text = gtk_text_buffer_get_text (priv->entry_buffer, &start_iter, &end_iter, FALSE);

	/* If the definition already exists, don't display the dialogue */
	definition = almanah_storage_manager_get_definition (almanah->storage_manager, text);
	if (definition != NULL) {
		g_object_unref (definition);

		/* Add a GtkTextTag to the GtkTextBuffer to mark the definition */
		gtk_text_buffer_apply_tag_by_name (priv->entry_buffer, "definition", &start_iter, &end_iter);
		gtk_text_buffer_set_modified (priv->entry_buffer, TRUE);

		g_free (text);
		return;
	}

	/* Create the Add Definition dialogue if it doesn't already exist */
	if (almanah->add_definition_dialog == NULL)
		almanah->add_definition_dialog = GTK_WIDGET (almanah_add_definition_dialog_new ());

	/* Ensure that something is selected and its widget's displayed */
	almanah_add_definition_dialog_set_text (ALMANAH_ADD_DEFINITION_DIALOG (almanah->add_definition_dialog), text);
	g_free (text);
	gtk_widget_show_all (almanah->add_definition_dialog);

	if (gtk_dialog_run (GTK_DIALOG (almanah->add_definition_dialog)) == GTK_RESPONSE_OK) {
		definition = almanah_add_definition_dialog_get_definition (ALMANAH_ADD_DEFINITION_DIALOG (almanah->add_definition_dialog));
		if (definition == NULL)
			return;

		/* Add to the DB */
		almanah_storage_manager_add_definition (almanah->storage_manager, definition);

		/* Add a GtkTextTag to the GtkTextBuffer to mark the definition */
		gtk_text_buffer_apply_tag_by_name (priv->entry_buffer, "definition", &start_iter, &end_iter);
		gtk_text_buffer_set_modified (priv->entry_buffer, TRUE);
	}
}

static void
remove_definition_from_current_entry (AlmanahMainWindow *self)
{
	/* We don't actually remove the definition from the database, since other entries may use it;
	 * we simply remove the formatting. */
	AlmanahMainWindowPrivate *priv = self->priv;
	GtkTextIter start_iter, end_iter;
	GtkTextTag *tag;
	GtkTextTagTable *tag_table;

	/* Find the tag in the tag table */
	tag_table = gtk_text_buffer_get_tag_table (priv->entry_buffer);
	tag = gtk_text_tag_table_lookup (tag_table, "definition");
	g_assert (tag != NULL);

	/* Find the boundaries of the tag (ignoring everything except the position
	 * of the start iter of any selection the user's made). */
	gtk_text_buffer_get_selection_bounds (priv->entry_buffer, &start_iter, NULL);
	end_iter = start_iter;

	if (gtk_text_iter_begins_tag (&start_iter, tag) == TRUE) {
		/* We're at the start of the tag */
		gtk_text_iter_forward_to_tag_toggle (&end_iter, tag);
	} else if (gtk_text_iter_ends_tag (&start_iter, tag) == TRUE) {
		/* We're at the end of the tag */
		end_iter = start_iter;
		gtk_text_iter_backward_to_tag_toggle (&start_iter, tag);
	} else {
		/* We're somewhere in the middle of the tag */
		gtk_text_iter_backward_to_tag_toggle (&start_iter, tag);
		end_iter = start_iter;
		gtk_text_iter_forward_to_tag_toggle (&end_iter, tag);
	}

	/* Remove the tag */
	gtk_text_buffer_remove_tag (priv->entry_buffer, tag, &start_iter, &end_iter);
	gtk_text_buffer_set_modified (priv->entry_buffer, TRUE);
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
	gboolean bold_toggled = FALSE, italic_toggled = FALSE, underline_toggled = FALSE, remove_definition_toggled = FALSE;

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

	/* We can only have the "add definition" action sensitive if there's a range selected */
	gtk_action_set_sensitive (priv->add_action, (range_selected == TRUE) ? TRUE : FALSE);

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

		if (strcmp (tag_name, "definition") == 0) {
			/* Deal with definition tags slightly differently --- just toggle the sensitivity of the "remove definition" action */
			gtk_action_set_sensitive (priv->remove_action, TRUE);
			remove_definition_toggled = TRUE;
		} else if (action != NULL) {
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
		if (remove_definition_toggled == FALSE)
			gtk_action_set_sensitive (priv->remove_action, FALSE);
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

static void
mw_entry_buffer_apply_tag_cb (GtkTextBuffer *buffer, GtkTextTag *tag, GtkTextIter *start_iter, GtkTextIter *end_iter, AlmanahMainWindow *main_window)
{
	gchar *name;

	g_object_get (G_OBJECT (tag), "name", &name, NULL);
	if (strcmp (name, "definition") == 0) {
		AlmanahDefinition *definition;
		gchar *definition_text;

		/* Check to see if the definition still exists in the DB */
		definition_text = gtk_text_buffer_get_text (buffer, start_iter, end_iter, FALSE);
		definition = almanah_storage_manager_get_definition (almanah->storage_manager, definition_text);
		g_free (definition_text);

		/* If the definition doesn't exist, remove the tag */
		if (definition == NULL) {
			gtk_text_buffer_remove_tag (buffer, tag, start_iter, end_iter);
			gtk_text_buffer_set_modified (buffer, TRUE);
		} else {
			g_object_unref (definition);
		}
	}

	g_free (name);
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
	if (almanah->import_dialog == NULL)
		almanah->import_dialog = GTK_WIDGET (almanah_import_export_dialog_new (TRUE));

	gtk_widget_show_all (almanah->import_dialog);
	gtk_dialog_run (GTK_DIALOG (almanah->import_dialog));
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
	almanah_entry_set_is_important (main_window->priv->current_entry, gtk_toggle_action_get_active(GTK_TOGGLE_ACTION (action)));
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
	guint entry_count, definition_count;

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

	almanah_storage_manager_get_statistics (almanah->storage_manager, &entry_count, &definition_count);
	description = g_strdup_printf (_("A helpful diary keeper, storing %u entries and %u definitions."),
				      entry_count,
				      definition_count);

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

void
mw_add_definition_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	add_definition_to_current_entry (main_window);
}

void
mw_remove_definition_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	remove_definition_from_current_entry (main_window);
}

void
mw_view_definitions_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	if (almanah->definition_manager_window == NULL)
		almanah->definition_manager_window = GTK_WIDGET (almanah_definition_manager_window_new ());

	gtk_widget_show_all (almanah->definition_manager_window);
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

	/* Update the date label */
	almanah_calendar_get_date (main_window->priv->calendar, &calendar_date);

	/* Translators: This is a strftime()-format string for the date displayed at the top of the main window. */
	g_date_strftime (calendar_string, sizeof (calendar_string), _("%A, %e %B %Y"), &calendar_date);
	gtk_label_set_markup (priv->date_label, calendar_string);

	/* Update the entry */
	if (priv->current_entry != NULL)
		g_object_unref (priv->current_entry);
	priv->current_entry = almanah_storage_manager_get_entry (almanah->storage_manager, &calendar_date);
	if (priv->current_entry == NULL)
		priv->current_entry = almanah_entry_new (&calendar_date);

	gtk_text_view_set_editable (priv->entry_view, almanah_entry_get_editability (priv->current_entry) != ALMANAH_ENTRY_FUTURE ? TRUE : FALSE);
	gtk_text_buffer_set_modified (priv->entry_buffer, FALSE);
	gtk_action_set_sensitive (priv->important_action, almanah_entry_get_editability (priv->current_entry) != ALMANAH_ENTRY_FUTURE ? TRUE : FALSE);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (priv->important_action), almanah_entry_is_important (priv->current_entry));

	/* Prepare for the possibility of failure --- do as much of the general interface changes as possible first */
	gtk_list_store_clear (priv->event_store);
	gtk_action_set_sensitive (priv->add_action, FALSE);
	gtk_action_set_sensitive (priv->remove_action, FALSE); /* Only sensitive if something's selected */
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

gboolean
mw_entry_view_focus_out_event_cb (GtkWidget *entry_view, GdkEventFocus *event, AlmanahMainWindow *main_window)
{
	save_current_entry (main_window);
	return FALSE;
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

static void
mw_definition_removed_cb (AlmanahStorageManager *storage_manager, const gchar *definition_text, AlmanahMainWindow *main_window)
{
	/* We need to remove any definition tags in the current entry which match @definition_text. It's
	 * probably easier to just reload the current entry, though. */
	mw_calendar_day_selected_cb (GTK_CALENDAR (main_window->priv->calendar), main_window);
}

#ifdef ENABLE_SPELL_CHECKING
gboolean
almanah_main_window_enable_spell_checking (AlmanahMainWindow *self, GError **error)
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
	spelling_language = gconf_client_get_string (almanah->gconf_client, "/apps/almanah/spelling_language", NULL);

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

void
almanah_main_window_disable_spell_checking (AlmanahMainWindow *self)
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
