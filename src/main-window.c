/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2008 <philip@tecnocode.co.uk>
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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#ifdef ENABLE_SPELL_CHECKING
#include <gtkspell/gtkspell.h>
#endif /* ENABLE_SPELL_CHECKING */

#include "main-window.h"
#include "main.h"
#include "interface.h"
#include "add-link-dialog.h"
#include "printing.h"
#include "entry.h"
#include "storage-manager.h"

static void almanah_main_window_init (AlmanahMainWindow *self);
static void almanah_main_window_dispose (GObject *object);
static gboolean mw_delete_event_cb (GtkWindow *window, gpointer user_data);
static void mw_entry_buffer_cursor_position_cb (GObject *object, GParamSpec *pspec, AlmanahMainWindow *main_window);
static void mw_entry_buffer_has_selection_cb (GObject *object, GParamSpec *pspec, AlmanahMainWindow *main_window);
static void mw_bold_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window);
static void mw_italic_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window);
static void mw_underline_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window);
void mw_calendar_day_selected_cb (GtkCalendar *calendar, AlmanahMainWindow *main_window);
static void mw_links_selection_changed_cb (GtkTreeSelection *tree_selection, AlmanahMainWindow *main_window);
static void mw_links_value_data_cb (GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data);

struct _AlmanahMainWindowPrivate {
	GtkTextView *entry_view;
	GtkTextBuffer *entry_buffer;
	GtkCalendar *calendar;
	GtkLabel *date_label;
	GtkButton *add_button;
	GtkButton *remove_button;
	GtkButton *view_button;
	GtkAction *add_action;
	GtkAction *remove_action;
	GtkListStore *link_store;
	GtkTreeSelection *links_selection;
	GtkTreeViewColumn *link_value_column;
	GtkCellRendererText *link_value_renderer;
	GtkToggleAction *bold_action;
	GtkToggleAction *italic_action;
	GtkToggleAction *underline_action;
	GtkAction *cut_action;
	GtkAction *copy_action;
	GtkAction *delete_action;

	gboolean updating_formatting_actions;

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

	/* TODO: Free UI objects? */
	if (priv->current_entry != NULL) {
		g_object_unref (priv->current_entry);
		priv->current_entry = NULL;
	}

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
	const gchar *interface_filename = diary_get_interface_filename ();
	const gchar *object_names[] = {
		"dry_main_window",
		"dry_mw_link_store",
		"dry_mw_view_button_image",
		"dry_ui_manager",
		NULL
	};

	builder = gtk_builder_new ();

	if (gtk_builder_add_objects_from_file (builder, interface_filename, (gchar**) object_names, &error) == FALSE) {
		/* Show an error */
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				_("UI file \"%s\" could not be loaded."), interface_filename);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
		g_object_unref (builder);

		return NULL;
	}

	gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	main_window = ALMANAH_MAIN_WINDOW (gtk_builder_get_object (builder, "dry_main_window"));
	gtk_builder_connect_signals (builder, main_window);

	if (main_window == NULL) {
		g_object_unref (builder);
		return NULL;
	}

	priv = ALMANAH_MAIN_WINDOW (main_window)->priv;

	/* Grab our child widgets */
	priv->entry_view = GTK_TEXT_VIEW (gtk_builder_get_object (builder, "dry_mw_entry_view"));
	priv->entry_buffer = gtk_text_view_get_buffer (priv->entry_view);
	priv->calendar = GTK_CALENDAR (gtk_builder_get_object (builder, "dry_mw_calendar"));
	priv->date_label = GTK_LABEL (gtk_builder_get_object (builder, "dry_mw_date_label"));
	priv->add_button = GTK_BUTTON (gtk_builder_get_object (builder, "dry_mw_add_button"));
	priv->remove_button = GTK_BUTTON (gtk_builder_get_object (builder, "dry_mw_remove_button"));
	priv->view_button = GTK_BUTTON (gtk_builder_get_object (builder, "dry_mw_view_button"));
	priv->add_action = GTK_ACTION (gtk_builder_get_object (builder, "dry_ui_add_link"));
	priv->remove_action = GTK_ACTION (gtk_builder_get_object (builder, "dry_ui_remove_link"));
	priv->link_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "dry_mw_link_store"));
	priv->links_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_builder_get_object (builder, "dry_mw_links_tree_view")));
	priv->link_value_column = GTK_TREE_VIEW_COLUMN (gtk_builder_get_object (builder, "dry_mw_link_value_column"));
	priv->link_value_renderer = GTK_CELL_RENDERER_TEXT (gtk_builder_get_object (builder, "dry_mw_link_value_renderer"));
	priv->bold_action = GTK_TOGGLE_ACTION (gtk_builder_get_object (builder, "dry_ui_bold"));;
	priv->italic_action = GTK_TOGGLE_ACTION (gtk_builder_get_object (builder, "dry_ui_italic"));
	priv->underline_action = GTK_TOGGLE_ACTION (gtk_builder_get_object (builder, "dry_ui_underline"));
	priv->cut_action = GTK_ACTION (gtk_builder_get_object (builder, "dry_ui_cut"));
	priv->copy_action = GTK_ACTION (gtk_builder_get_object (builder, "dry_ui_copy"));
	priv->delete_action = GTK_ACTION (gtk_builder_get_object (builder, "dry_ui_delete"));

	/* Set up text formatting */
	gtk_text_buffer_create_tag (priv->entry_buffer, "bold", 
				    "weight", PANGO_WEIGHT_BOLD, 
				    NULL);
	gtk_text_buffer_create_tag (priv->entry_buffer, "italic",
				    "style", PANGO_STYLE_ITALIC,
				    NULL);
	gtk_text_buffer_create_tag (priv->entry_buffer, "underline",
				    "underline", PANGO_UNDERLINE_SINGLE,
				    NULL);

#ifdef ENABLE_SPELL_CHECKING
	/* Set up spell checking */
	if (gtkspell_new_attach (priv->entry_view, NULL, &error) == FALSE) {
		gchar *error_message = g_strdup_printf (_("The spelling checker could not be initialized: %s"), error->message);
		diary_interface_error (error_message, NULL);
		g_free (error_message);
		g_error_free (error);
	}
#endif /* ENABLE_SPELL_CHECKING */

	/* Make sure we're notified if the cursor moves position so we can check the tag stack */
	g_signal_connect (priv->entry_buffer, "notify::cursor-position", G_CALLBACK (mw_entry_buffer_cursor_position_cb), main_window);

	/* Similarly, make sure we're notified when there's a selection so we can change the status of cut/copy/paste actions */
	g_signal_connect (priv->entry_buffer, "notify::has-selection", G_CALLBACK (mw_entry_buffer_has_selection_cb), main_window);

	/* Connect up the formatting actions */
	g_signal_connect (priv->bold_action, "toggled", G_CALLBACK (mw_bold_toggled_cb), main_window);
	g_signal_connect (priv->italic_action, "toggled", G_CALLBACK (mw_italic_toggled_cb), main_window);
	g_signal_connect (priv->underline_action, "toggled", G_CALLBACK (mw_underline_toggled_cb), main_window);

	/* Select the current day and month */
	diary_calendar_month_changed_cb (priv->calendar, NULL);
	mw_calendar_day_selected_cb (priv->calendar, main_window);

	/* Set up the treeview */
	g_signal_connect (priv->links_selection, "changed", G_CALLBACK (mw_links_selection_changed_cb), main_window);
	gtk_tree_view_column_set_cell_data_func (priv->link_value_column, GTK_CELL_RENDERER (priv->link_value_renderer), mw_links_value_data_cb, NULL, NULL);

	/* Prettify the UI */
	diary_interface_embolden_label (GTK_LABEL (gtk_builder_get_object (builder, "dry_mw_calendar_label")));
	diary_interface_embolden_label (GTK_LABEL (gtk_builder_get_object (builder, "dry_mw_attached_links_label")));

	g_object_unref (builder);

	return main_window;
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
	entry_exists = almanah_storage_manager_entry_exists (diary->storage_manager, &date);
	entry_is_empty = almanah_entry_is_empty (priv->current_entry);
	editability = almanah_entry_get_editability (priv->current_entry);

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

		g_date_strftime (date_string, sizeof (date_string), "%A, %e %B %Y", &date);

		dialog = gtk_message_dialog_new (GTK_WINDOW (self),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_QUESTION,
						 GTK_BUTTONS_NONE,
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

		g_date_strftime (date_string, sizeof (date_string), "%A, %e %B %Y", &date);

		dialog = gtk_message_dialog_new (GTK_WINDOW (self),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_QUESTION,
						 GTK_BUTTONS_NONE,
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
	almanah_storage_manager_set_entry (diary->storage_manager, priv->current_entry);

	/* Mark the day on the calendar if the entry was non-empty (and deleted)
	 * and update the state of the add link button. */
	if (entry_is_empty == TRUE) {
		gtk_calendar_unmark_day (priv->calendar, g_date_get_day (&date));

		gtk_widget_set_sensitive (GTK_WIDGET (priv->add_button), FALSE);
		gtk_action_set_sensitive (priv->add_action, FALSE);

		/* Since the entry is empty, remove all the links from the treeview */
		gtk_list_store_clear (priv->link_store);
	} else {
		gtk_calendar_mark_day (priv->calendar, g_date_get_day (&date));

		gtk_widget_set_sensitive (GTK_WIDGET (priv->add_button), TRUE);
		gtk_action_set_sensitive (priv->add_action, TRUE);
	}
}

static void
add_link_to_current_entry (AlmanahMainWindow *self)
{
	GtkTreeIter iter;
	AlmanahMainWindowPrivate *priv = self->priv;

	g_assert (priv->entry_buffer != NULL);
	g_assert (gtk_text_buffer_get_char_count (priv->entry_buffer) != 0);

	/* Ensure that something is selected and its widgets displayed */
	gtk_widget_show_all (diary->add_link_dialog);

	if (gtk_dialog_run (GTK_DIALOG (diary->add_link_dialog)) == GTK_RESPONSE_OK) {
		guint year, month, day;
		GDate date;
		AlmanahLink *link = almanah_add_link_dialog_get_link (ALMANAH_ADD_LINK_DIALOG (diary->add_link_dialog));

		if (link == NULL)
			return;

		/* Add to the DB */
		/* TODO: Clean this date stuff up and separate it out into its own function */
		gtk_calendar_get_date (priv->calendar, &year, &month, &day);
		month++;
		g_date_set_dmy (&date, day, month, year);

		almanah_storage_manager_add_entry_link (diary->storage_manager, &date, link);

		/* Add to the treeview */
		gtk_list_store_append (priv->link_store, &iter);
		gtk_list_store_set (priv->link_store, &iter,
				    0, almanah_link_get_type_id (link),
				    1, almanah_link_get_value (link),
				    2, almanah_link_get_value2 (link),
				    3, almanah_link_get_icon_name (link),
				    -1);

		g_object_unref (link);
	}
}

static void
remove_link_from_current_entry (AlmanahMainWindow *self)
{
	gchar *link_type;
	guint year, month, day;
	GDate date;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GList *links;
	AlmanahMainWindowPrivate *priv = self->priv;

	links = gtk_tree_selection_get_selected_rows (priv->links_selection, &model);
	gtk_calendar_get_date (priv->calendar, &year, &month, &day);
	month++;
	g_date_set_dmy (&date, day, month, year);

	for (; links != NULL; links = links->next) {
		gtk_tree_model_get_iter (model, &iter, (GtkTreePath*) links->data);
		gtk_tree_model_get (model, &iter, 0, &link_type, -1);

		/* Remove it from the DB */
		almanah_storage_manager_remove_entry_link (diary->storage_manager, &date, link_type);

		/* Remove it from the treeview */
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

		gtk_tree_path_free (links->data);
		g_free (link_type);
	}
	g_list_free (links);
}

void
almanah_main_window_select_date (AlmanahMainWindow *self, GDate *date)
{
	gtk_calendar_select_month (self->priv->calendar, g_date_get_month (date) - 1, g_date_get_year (date));
	gtk_calendar_select_day (self->priv->calendar, g_date_get_day (date));
}

static void
mw_entry_buffer_cursor_position_cb (GObject *object, GParamSpec *pspec, AlmanahMainWindow *main_window)
{
	GtkTextIter iter;
	AlmanahMainWindowPrivate *priv = main_window->priv;
	GSList *tag_list = NULL;
	gboolean bold_toggled = FALSE, italic_toggled = FALSE, underline_toggled = FALSE;

	/* Only get the tag list if there's no selection (just an insertion cursor),
	 * since we want the buttons untoggled if there's a selection. */
	if (gtk_text_buffer_get_selection_bounds (priv->entry_buffer, &iter, NULL) == FALSE)
		tag_list = gtk_text_iter_get_tags (&iter);

	/* Block signal handlers for the formatting actions while we're executing,
	 * so formatting doesn't get unwittingly changed. */
	priv->updating_formatting_actions = TRUE;

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

		if (action) {
			/* Force the toggle status on the action */
			gtk_toggle_action_set_active (action, TRUE);
		} else {
			/* Print a warning about the unknown tag */
			g_message (_("Unknown or duplicate text tag \"%s\" in entry. Ignoring."), tag_name);
		}

		g_free (tag_name);
		tag_list = tag_list->next;
	}

	g_slist_free (tag_list);

	/* Untoggle the remaining actions */
	if (bold_toggled == FALSE)
		gtk_toggle_action_set_active (priv->bold_action, FALSE);
	if (italic_toggled == FALSE)
		gtk_toggle_action_set_active (priv->italic_action, FALSE);
	if (underline_toggled == FALSE)
		gtk_toggle_action_set_active (priv->underline_action, FALSE);

	/* Unblock signals */
	priv->updating_formatting_actions = FALSE;
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
	diary_quit ();

	return TRUE;
}

void
mw_print_activate_cb (GtkAction *action, gpointer user_data)
{
	diary_print_entries ();
}

void
mw_quit_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	save_current_entry (main_window);
	diary_quit ();
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
mw_search_activate_cb (GtkAction *action, gpointer user_data)
{
	gtk_widget_show_all (diary->search_dialog);
	gtk_dialog_run (GTK_DIALOG (diary->search_dialog));
}

void
mw_preferences_activate_cb (GtkAction *action, gpointer user_data)
{
	gtk_widget_show_all (diary->preferences_dialog);
	gtk_dialog_run (GTK_DIALOG (diary->preferences_dialog));
}

static void
apply_formatting (AlmanahMainWindow *self, const gchar *tag_name, gboolean applying)
{
	GtkTextIter start, end;

	gtk_text_buffer_get_selection_bounds (self->priv->entry_buffer, &start, &end);
	if (applying == TRUE)
		gtk_text_buffer_apply_tag_by_name (self->priv->entry_buffer, tag_name, &start, &end);
	else
		gtk_text_buffer_remove_tag_by_name (self->priv->entry_buffer, tag_name, &start, &end);
}

static void
mw_bold_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window)
{
	if (main_window->priv->updating_formatting_actions == FALSE)
		apply_formatting (main_window, "bold", gtk_toggle_action_get_active (action));
}

static void
mw_italic_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window)
{
	if (main_window->priv->updating_formatting_actions == FALSE)
		apply_formatting (main_window, "italic", gtk_toggle_action_get_active (action));
}

static void
mw_underline_toggled_cb (GtkToggleAction *action, AlmanahMainWindow *main_window)
{
	if (main_window->priv->updating_formatting_actions == FALSE)
		apply_formatting (main_window, "underline", gtk_toggle_action_get_active (action));
}

void
mw_about_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	gchar *license, *description;
	guint entry_count, link_count;

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

	almanah_storage_manager_get_statistics (diary->storage_manager, &entry_count, &link_count);
	description = g_strdup_printf (_("A helpful diary keeper, storing %u entries and %u links."),
				      entry_count,
				      link_count);

	gtk_show_about_dialog (GTK_WINDOW (main_window),
				"version", VERSION,
				"copyright", _("Copyright \xc2\xa9 2008 Philip Withnall"),
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
				"website", "http://tecnocode.co.uk/projects/almanah",
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
mw_add_link_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	add_link_to_current_entry (main_window);
}

void
mw_remove_link_activate_cb (GtkAction *action, AlmanahMainWindow *main_window)
{
	remove_link_from_current_entry (main_window);
}

void
mw_calendar_day_selected_cb (GtkCalendar *calendar, AlmanahMainWindow *main_window)
{
	GDate calendar_date;
	gchar calendar_string[100];
	guint year, month, day;
	AlmanahLink **links;
	guint i;
	GtkTreeIter iter;
#ifdef ENABLE_SPELL_CHECKING
	GtkSpell *gtkspell;
#endif /* ENABLE_SPELL_CHECKING */
	AlmanahMainWindowPrivate *priv = main_window->priv;

	/* Update the date label */
	gtk_calendar_get_date (calendar, &year, &month, &day);
	month++;
	g_date_set_dmy (&calendar_date, day, month, year);

	/* Translators: This is a strftime()-format string for the date displayed at the top of the main window. */
	g_date_strftime (calendar_string, sizeof (calendar_string), _("%A, %e %B %Y"), &calendar_date);
	gtk_label_set_markup (priv->date_label, calendar_string);
	diary_interface_embolden_label (priv->date_label);

	/* Update the entry */
	if (priv->current_entry != NULL)
		g_object_unref (priv->current_entry);
	priv->current_entry = almanah_storage_manager_get_entry (diary->storage_manager, &calendar_date);
	if (priv->current_entry == NULL)
		priv->current_entry = almanah_entry_new (&calendar_date);

	gtk_text_view_set_editable (priv->entry_view, almanah_entry_get_editability (priv->current_entry) != ALMANAH_ENTRY_FUTURE ? TRUE : FALSE);
	gtk_text_buffer_set_modified (priv->entry_buffer, FALSE);

	/* Prepare for the possibility of failure --- do as much of the general interface changes as possible first */
	gtk_list_store_clear (priv->link_store);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->remove_button), FALSE); /* Only sensitive if something's selected */
	gtk_action_set_sensitive (priv->remove_action, FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->view_button), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->add_button), FALSE);
	gtk_action_set_sensitive (priv->add_action, FALSE);

	if (almanah_entry_is_empty (priv->current_entry) == FALSE) {
		GError *error = NULL;

		gtk_text_buffer_set_text (priv->entry_buffer, "", 0);
		if (almanah_entry_get_content (priv->current_entry, priv->entry_buffer, &error) == FALSE) {
			gchar *error_message = g_strdup_printf (_("The entry content could not be loaded: %s"), error->message);
			diary_interface_error (error_message, NULL);
			g_free (error_message);
			g_error_free (error);

			/* Make sure the interface is left in a decent state before we return */
			gtk_text_view_set_editable (priv->entry_view, FALSE);

			return;
		}

		gtk_widget_set_sensitive (GTK_WIDGET (priv->add_button), TRUE);
		gtk_action_set_sensitive (priv->add_action, TRUE);
	} else {
		/* Set the buffer to be empty */
		gtk_text_buffer_set_text (priv->entry_buffer, "", -1);
	}

#ifdef ENABLE_SPELL_CHECKING
	/* Ensure the spell-checking is updated */
	gtkspell = gtkspell_get_from_text_view (priv->entry_view);
	if (gtkspell)
		gtkspell_recheck_all (gtkspell);
#endif /* ENABLE_SPELL_CHECKING */

	/* List the entry's links */
	links = almanah_storage_manager_get_entry_links (diary->storage_manager, &calendar_date);

	i = 0;
	while (links[i] != NULL) {
		gchar *value, *value2;

		value = almanah_link_get_value (links[i]);
		value2 = almanah_link_get_value2 (links[i]);

		gtk_list_store_append (priv->link_store, &iter);
		gtk_list_store_set (priv->link_store, &iter,
				    0, almanah_link_get_type_id (links[i]),
				    1, value,
				    2, value2,
				    3, almanah_link_get_icon_name (links[i]),
				    -1);

		g_free (value);
		g_free (value2);
		g_object_unref (links[i]);

		i++;
	}

	g_free (links);
}

static void
mw_links_selection_changed_cb (GtkTreeSelection *tree_selection, AlmanahMainWindow *main_window)
{
	AlmanahMainWindowPrivate *priv = main_window->priv;

	if (gtk_tree_selection_count_selected_rows (tree_selection) == 0) {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->remove_button), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->view_button), FALSE);
		gtk_action_set_sensitive (priv->remove_action, FALSE);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->remove_button), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->view_button), TRUE);
		gtk_action_set_sensitive (priv->remove_action, TRUE);
	}
}

static void
mw_links_value_data_cb (GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	gchar *new_value, *value, *value2, *type;
	AlmanahLink *link;

	/* TODO: Should really create a new model to render AlmanahLinks --- or at least attach the approprite AlmanahLink to each tree model row */

	gtk_tree_model_get (model, iter,
			    0, &type,
			    1, &value,
			    2, &value2,
			    -1);

	link = almanah_link_new (type);
	almanah_link_set_value (link, value);
	almanah_link_set_value2 (link, value2);

	new_value = almanah_link_format_value (link);
	g_object_set (renderer, "text", new_value, NULL);
	g_free (new_value);

	g_free (type);
	g_free (value);
	g_free (value2);
	g_object_unref (link);
}

void
mw_links_tree_view_row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, AlmanahMainWindow *main_window)
{
	AlmanahLink *link;
	gchar *value, *value2, *type;
	GtkTreeIter iter;

	gtk_tree_model_get_iter (GTK_TREE_MODEL (main_window->priv->link_store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (main_window->priv->link_store), &iter,
			    0, &type,
			    1, &value,
			    2, &value2,
			    -1);

	link = almanah_link_new (type);
	almanah_link_set_value (link, value);
	almanah_link_set_value2 (link, value2);

	/* NOTE: Link types should display their own errors, so one won't be displayed here. */
	almanah_link_view (link);

	g_free (type);
	g_free (value);
	g_free (value2);
	g_object_unref (link);
}

gboolean
mw_entry_view_focus_out_event_cb (GtkWidget *entry_view, GdkEventFocus *event, AlmanahMainWindow *main_window)
{
	save_current_entry (main_window);
	return FALSE;
}

void
mw_add_button_clicked_cb (GtkButton *button, AlmanahMainWindow *main_window)
{
	add_link_to_current_entry (main_window);
}

void
mw_remove_button_clicked_cb (GtkButton *button, AlmanahMainWindow *main_window)
{
	remove_link_from_current_entry (main_window);
}

void
mw_view_button_clicked_cb (GtkButton *button, AlmanahMainWindow *main_window)
{
	GList *links;
	GtkTreeModel *model;

	links = gtk_tree_selection_get_selected_rows (main_window->priv->links_selection, &model);

	for (; links != NULL; links = links->next) {
		AlmanahLink *link;
		gchar *value, *value2, *type;
		GtkTreeIter iter;

		gtk_tree_model_get_iter (model, &iter, (GtkTreePath*) links->data);
		gtk_tree_model_get (model, &iter,
				    0, &type,
				    1, &value,
				    2, &value2,
				    -1);

		link = almanah_link_new (type);
		almanah_link_set_value (link, value);
		almanah_link_set_value2 (link, value2);

		/* NOTE: Link types should display their own errors, so one won't be displayed here. */
		almanah_link_view (link);

		g_free (type);
		g_free (value);
		g_free (value2);
		g_object_unref (link);

		gtk_tree_path_free (links->data);
	}

	g_list_free (links);
}
