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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "storage-manager.h"
#include "interface.h"
#include "printing.h"
#include "widgets/calendar.h"

#define TITLE_MARGIN_BOTTOM 15 /* margin under the title, in pixels */
#define IMPORTANT_MARGIN_TOP 3 /* margin above the "important" paragraph (and below the title), in pixels */
#define ENTRY_MARGIN_BOTTOM 10 /* margin under the entry, in pixels */
#define MAX_ORPHANS 3 /* maximum number of orphan lines to be forced to the next page */
#define PAGE_MARGIN 20 /* left- and right-hand page margin size, in pixels */

typedef struct {
	AlmanahStorageManager *storage_manager;
	GtkTextBuffer *buffer;
	GDate *start_date;
	GDate *end_date;
	GDate *current_date;
	AlmanahCalendar *start_calendar;
	AlmanahCalendar *end_calendar;
	GtkSpinButton *line_spacing_spin_button;
	gdouble line_spacing;
	guint current_line;
	gboolean paginated;
	gdouble y;
} AlmanahPrintOperation;

/* Adapted from code in GtkSourceView's gtksourceprintcompositor.c, previously LGPL >= 2.1
 * Copyright (C) 2000, 2001 Chema Celorio  
 * Copyright (C) 2003  Gustavo Giráldez
 * Copyright (C) 2004  Red Hat, Inc.
 * Copyright (C) 2001-2007  Paolo Maggi
 * Copyright (C) 2008  Paolo Maggi, Paolo Borelli and Yevgen Muntyan */
static GSList *
get_iter_attrs (GtkTextIter *iter, GtkTextIter *limit)
{
	GSList *attrs = NULL;
	GSList *tags;
	PangoAttribute *bg = NULL, *fg = NULL, *style = NULL, *ul = NULL;
	PangoAttribute *weight = NULL, *st = NULL;

	tags = gtk_text_iter_get_tags (iter);
	gtk_text_iter_forward_to_tag_toggle (iter, NULL);

	if (gtk_text_iter_compare (iter, limit) > 0)
		*iter = *limit;

	while (tags)
	{
		GtkTextTag *tag;
		gboolean bg_set, fg_set, style_set, ul_set, weight_set, st_set;

		tag = tags->data;
		tags = g_slist_delete_link (tags, tags);

		g_object_get (tag,
			     "background-set", &bg_set,
			     "foreground-set", &fg_set,
			     "style-set", &style_set,
			     "underline-set", &ul_set,
			     "weight-set", &weight_set,
			     "strikethrough-set", &st_set,
			     NULL);

		if (bg_set)
		{
			GdkRGBA *color = NULL;
			if (bg) pango_attribute_destroy (bg);
			g_object_get (tag, "background-rgba", &color, NULL);
			bg = pango_attr_background_new (color->red, color->green, color->blue);
			gdk_rgba_free (color);
		}

		if (fg_set)
		{
			GdkRGBA *color = NULL;
			if (fg) pango_attribute_destroy (fg);
			g_object_get (tag, "foreground-rgba", &color, NULL);
			fg = pango_attr_foreground_new (color->red, color->green, color->blue);
			gdk_rgba_free (color);
		}

		if (style_set)
		{
			PangoStyle style_value;
			if (style) pango_attribute_destroy (style);
			g_object_get (tag, "style", &style_value, NULL);
			style = pango_attr_style_new (style_value);
		}

		if (ul_set)
		{
			PangoUnderline underline;
			if (ul) pango_attribute_destroy (ul);
			g_object_get (tag, "underline", &underline, NULL);
			ul = pango_attr_underline_new (underline);
		}

		if (weight_set)
		{
			PangoWeight weight_value;
			if (weight) pango_attribute_destroy (weight);
			g_object_get (tag, "weight", &weight_value, NULL);
			weight = pango_attr_weight_new (weight_value);
		}

		if (st_set)
		{
			gboolean strikethrough;
			if (st) pango_attribute_destroy (st);
			g_object_get (tag, "strikethrough", &strikethrough, NULL);
			st = pango_attr_strikethrough_new (strikethrough);
		}
	}

	if (bg)
		attrs = g_slist_prepend (attrs, bg);
	if (fg)
		attrs = g_slist_prepend (attrs, fg);
	if (style)
		attrs = g_slist_prepend (attrs, style);
	if (ul)
		attrs = g_slist_prepend (attrs, ul);
	if (weight)
		attrs = g_slist_prepend (attrs, weight);
	if (st)
		attrs = g_slist_prepend (attrs, st);

	return attrs;
}

/* Adapted from code in GtkSourceView's gtksourceprintcompositor.c, previously LGPL >= 2.1
 * Copyright (C) 2000, 2001 Chema Celorio  
 * Copyright (C) 2003  Gustavo Giráldez
 * Copyright (C) 2004  Red Hat, Inc.
 * Copyright (C) 2001-2007  Paolo Maggi
 * Copyright (C) 2008  Paolo Maggi, Paolo Borelli and Yevgen Muntyan */
static gboolean
is_empty_line (const gchar *text)
{
	if (*text != '\0') {
		const gchar *p;

		for (p = text; p != NULL; p = g_utf8_next_char (p)) {
			if (!g_unichar_isspace (*p))
				return FALSE;
		}
	}

	return TRUE;
}

/* Adapted from code in GtkSourceView's gtksourceprintcompositor.c, previously LGPL >= 2.1
 * Copyright (C) 2000, 2001 Chema Celorio  
 * Copyright (C) 2003  Gustavo Giráldez
 * Copyright (C) 2004  Red Hat, Inc.
 * Copyright (C) 2001-2007  Paolo Maggi
 * Copyright (C) 2008  Paolo Maggi, Paolo Borelli and Yevgen Muntyan */
static void
lay_out_entry (PangoLayout *layout, GtkTextIter *start, GtkTextIter *end)
{
	gchar *text;
	PangoAttrList *attr_list = NULL;
	GtkTextIter segm_start, segm_end;
	int start_index;

	text = gtk_text_iter_get_slice (start, end);

	/* If it is an empty line (or it just contains tabs) pango has problems:
	 * see for instance comment #22 and #23 on bug #143874 and bug #457990.
	 * We just hack around it by inserting a space... not elegant but
	 * works :-) */
	if (gtk_text_iter_ends_line (start) || is_empty_line (text)) {
		pango_layout_set_text (layout, " ", 1);
		g_free (text);
		return;
	}

	pango_layout_set_text (layout, text, -1);
	g_free (text);

	segm_start = *start;
	start_index = gtk_text_iter_get_line_index (start);

	while (gtk_text_iter_compare (&segm_start, end) < 0) {
		GSList *attrs;
		int si, ei;

		segm_end = segm_start;
		attrs = get_iter_attrs (&segm_end, end);
		if (attrs) {
			si = gtk_text_iter_get_line_index (&segm_start) - start_index;
			ei = gtk_text_iter_get_line_index (&segm_end) - start_index;
		}

		while (attrs) {
			PangoAttribute *a = attrs->data;

			a->start_index = si;
			a->end_index = ei;

			if (!attr_list)
				attr_list = pango_attr_list_new ();

			pango_attr_list_insert (attr_list, a);

			attrs = g_slist_delete_link (attrs, attrs);
		}

		segm_start = segm_end;
	}

	pango_layout_set_attributes (layout, attr_list);

	if (attr_list)
		pango_attr_list_unref (attr_list);
}

/* TRUE if the entry was printed OK on the current page, FALSE if it needs to be moved to a new page/is split across pages */
static gboolean
print_entry (GtkPrintOperation *operation, GtkPrintContext *context, AlmanahPrintOperation *almanah_operation)
{
	AlmanahEntry *entry;
	gchar title[100], *markup;
	PangoLayout *title_layout = NULL, *important_layout = NULL, *entry_layout;
	PangoLayoutLine *entry_line;
	PangoRectangle logical_extents;
	gint height, line_height;
	guint i;
	gdouble title_y = 0, important_y = 0, entry_y;
	cairo_t *cr = NULL;

	entry = almanah_storage_manager_get_entry (almanah_operation->storage_manager, almanah_operation->current_date);

	if (almanah_operation->current_line == 0) {
		/* Set up the title layout */
		title_layout = gtk_print_context_create_pango_layout (context);
		pango_layout_set_width (title_layout, (gtk_print_context_get_width (context) - 2 * PAGE_MARGIN) * PANGO_SCALE);

		/* Translators: This is a strftime()-format string for the date displayed above each printed entry. */
		g_date_strftime (title, sizeof (title), _("%A, %e %B %Y"), almanah_operation->current_date);
		markup = g_strdup_printf ("<b>%s</b>", title);
		pango_layout_set_markup (title_layout, markup, -1);
		g_free (markup);

		title_y = almanah_operation->y;
		pango_layout_get_pixel_size (title_layout, NULL, &height);
		almanah_operation->y += height;

		if (entry != NULL && almanah_entry_is_important (entry) == TRUE) {
			/* State that it's an important entry */
			important_layout = gtk_print_context_create_pango_layout (context);
			pango_layout_set_width (title_layout, (gtk_print_context_get_width (context) - 2 * PAGE_MARGIN) * PANGO_SCALE);

			markup = g_strdup_printf ("<i>%s</i>", _("This entry is marked as important."));
			pango_layout_set_markup (important_layout, markup, -1);
			g_free (markup);

			/* If the important paragraph is displayed, it's displayed IMPORTANT_MARGIN_TOP pixels below the bottom of the title
			 * paragraph, and then the entry content is displayed TITLE_MARGIN_BOTTOM pixels below the bottom of the important paragraph. */
			almanah_operation->y += IMPORTANT_MARGIN_TOP;
			important_y = almanah_operation->y;
			pango_layout_get_pixel_size (important_layout, NULL, &height);
			almanah_operation->y += height;
		}

		almanah_operation->y += TITLE_MARGIN_BOTTOM;
	}

	/* Set up the entry layout */
	entry_layout = gtk_print_context_create_pango_layout (context);
	pango_layout_set_width (entry_layout, (gtk_print_context_get_width (context) - 2 * PAGE_MARGIN) * PANGO_SCALE);
	pango_layout_set_ellipsize (entry_layout, PANGO_ELLIPSIZE_NONE);

	if (entry == NULL || almanah_entry_is_empty (entry)) {
		gchar *entry_text = g_strdup_printf ("<i>%s</i>", _("No entry for this date."));
		pango_layout_set_markup (entry_layout, entry_text, -1);
	} else {
		GtkTextIter start, end;

		gtk_text_buffer_set_text (almanah_operation->buffer, "", 0);
		if (almanah_entry_get_content (entry, almanah_operation->buffer, FALSE, NULL) == TRUE) {
			gtk_text_buffer_get_bounds (almanah_operation->buffer, &start, &end);
			lay_out_entry (entry_layout, &start, &end);
		}
	}

	if (entry != NULL)
		g_object_unref (entry);

	/* Check we're not orphaning things */
	entry_line = pango_layout_get_line_readonly (entry_layout, MIN ((guint) pango_layout_get_line_count (entry_layout) - 1, almanah_operation->current_line + MAX_ORPHANS));
	pango_layout_line_get_pixel_extents (entry_line, NULL, &logical_extents);
	line_height = pango_layout_get_spacing (entry_layout) / PANGO_SCALE + (gint) (almanah_operation->line_spacing * ((gdouble) logical_extents.height));

	if (almanah_operation->y + (MIN (pango_layout_get_line_count (entry_layout), MAX_ORPHANS) * line_height) - pango_layout_get_spacing (entry_layout) / PANGO_SCALE + (gint) ((almanah_operation->line_spacing - 1.0) * ((gdouble) logical_extents.height)) > gtk_print_context_get_height (context)) {
		/* Not enough space on the page to contain the title and orphaned lines */
		if (title_layout != NULL)
			g_object_unref (title_layout);
		if (important_layout != NULL)
			g_object_unref (important_layout);
		g_object_unref (entry_layout);

		almanah_operation->current_line = 0;

		return FALSE;
	}

	if (almanah_operation->paginated == TRUE) {
		cr = gtk_print_context_get_cairo_context (context);

		if (almanah_operation->current_line == 0) {
			/* Draw the title */
			cairo_move_to (cr, PAGE_MARGIN, title_y);
			pango_cairo_show_layout (cr, title_layout);

			if (important_layout != NULL) {
				/* Draw the important paragraph */
				cairo_move_to (cr, PAGE_MARGIN, important_y);
				pango_cairo_show_layout (cr, important_layout);
			}
		}
	}

	entry_y = almanah_operation->y;

	/* Draw the lines in the entry */
	for (i = almanah_operation->current_line; (gint) i < pango_layout_get_line_count (entry_layout); i++) {
		entry_line = pango_layout_get_line_readonly (entry_layout, i);
		pango_layout_line_get_pixel_extents (entry_line, NULL, &logical_extents);
		line_height = pango_layout_get_spacing (entry_layout) / PANGO_SCALE + (gint) (almanah_operation->line_spacing * ((gdouble) logical_extents.height));

		/* Check we're not going to overflow the page. We don't use the line_height here, as we can ignore any double spacing under the
		 * bottom line on a page. */
		if (entry_y + logical_extents.height > gtk_print_context_get_height (context)) {
			/* Paint the rest on the next page */
			if (title_layout != NULL)
				g_object_unref (title_layout);
			if (important_layout != NULL)
				g_object_unref (important_layout);
			g_object_unref (entry_layout);

			almanah_operation->current_line = i;

			return FALSE;
		}

		if (almanah_operation->paginated == TRUE) {
			/* Draw the entry line */
			cairo_move_to (cr, PAGE_MARGIN, entry_y);
			pango_cairo_show_layout_line (cr, entry_line);
		}

		entry_y += line_height;
	}

	/* Finish off the entry with a bottom margin */
	almanah_operation->y = entry_y + ENTRY_MARGIN_BOTTOM;

	if (title_layout != NULL)
		g_object_unref (title_layout);
	if (important_layout != NULL)
		g_object_unref (important_layout);
	g_object_unref (entry_layout);

	almanah_operation->current_line = 0;

	return TRUE;
}

static gboolean
paginate_cb (GtkPrintOperation *operation, GtkPrintContext *context, AlmanahPrintOperation *almanah_operation)
{
	/* Calculate the number of pages by laying everything out :( */
	while (g_date_compare (almanah_operation->current_date, almanah_operation->end_date) <= 0 &&
	       print_entry (operation, context, almanah_operation) == TRUE) {
		g_date_add_days (almanah_operation->current_date, 1);
	}

	if (g_date_compare (almanah_operation->current_date, almanah_operation->end_date) > 0) {
		/* We're done paginating */
		return TRUE;
	} else {
		/* More pages! */
		gint pages;
		g_object_get (operation, "n-pages", &pages, NULL);
		gtk_print_operation_set_n_pages (operation, pages + 1);

		almanah_operation->y = 0;

		return FALSE;
	}
}

static void
draw_page_cb (GtkPrintOperation *operation, GtkPrintContext *context, gint page_number, AlmanahPrintOperation *almanah_operation)
{
	if (almanah_operation->paginated == FALSE) {
		/* Reset things before we start printing */
		almanah_operation->paginated = TRUE;
		almanah_operation->current_line = 0;
		g_date_set_dmy (almanah_operation->current_date,
				g_date_get_day (almanah_operation->start_date),
				g_date_get_month (almanah_operation->start_date),
				g_date_get_year (almanah_operation->start_date));
	}

	almanah_operation->y = 0;

	while (g_date_compare (almanah_operation->current_date, almanah_operation->end_date) <= 0 &&
	       print_entry (operation, context, almanah_operation) == TRUE) {
		g_date_add_days (almanah_operation->current_date, 1);
	}
}

static GtkWidget *
create_custom_widget_cb (GtkPrintOperation *operation, AlmanahPrintOperation *almanah_operation)
{
	GtkWidget *start_calendar, *end_calendar, *line_spacing_spin_button;
	GtkLabel *start_label, *end_label, *line_spacing_label;
	GtkGrid *grid;
	GtkBox *vbox, *hbox;

	/* Start and end calendars */
	start_calendar = almanah_calendar_new (almanah_operation->storage_manager);
	gtk_widget_set_hexpand (start_calendar, TRUE);
	gtk_widget_set_vexpand (start_calendar, TRUE);
	end_calendar = almanah_calendar_new (almanah_operation->storage_manager);
	gtk_widget_set_hexpand (end_calendar, TRUE);
	gtk_widget_set_vexpand (end_calendar, TRUE);

	g_object_set (G_OBJECT (start_calendar), "show-details", FALSE, NULL);
	g_object_set (G_OBJECT (end_calendar), "show-details", FALSE, NULL);

	start_label = GTK_LABEL (gtk_label_new (_("Start date:")));
	gtk_widget_set_halign (GTK_WIDGET (start_label), GTK_ALIGN_START);
	end_label = GTK_LABEL (gtk_label_new (_("End date:")));
	gtk_widget_set_halign (GTK_WIDGET (end_label), GTK_ALIGN_START);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 6);
	gtk_grid_set_column_spacing (grid, 6);
	gtk_container_set_border_width (GTK_CONTAINER (grid), 6);
	gtk_grid_attach (grid, GTK_WIDGET (start_label), 0, 0, 1, 1);
	gtk_grid_attach (grid, start_calendar, 0, 1, 1, 1);
	gtk_grid_attach (grid, GTK_WIDGET (end_label), 1, 0, 1, 1);
	gtk_grid_attach (grid, end_calendar, 1, 1, 1, 1);

	almanah_operation->start_calendar = ALMANAH_CALENDAR (start_calendar);
	almanah_operation->end_calendar = ALMANAH_CALENDAR (end_calendar);

	/* Line spacing */
	line_spacing_label = GTK_LABEL (gtk_label_new (_("Line spacing:")));
	line_spacing_spin_button = gtk_spin_button_new_with_range (1.0, 3.0, 0.5);

	almanah_operation->line_spacing_spin_button = GTK_SPIN_BUTTON (line_spacing_spin_button);

	hbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6));
	gtk_box_pack_start (hbox, GTK_WIDGET (line_spacing_label), FALSE, TRUE, 6);
	gtk_box_pack_start (hbox, line_spacing_spin_button, TRUE, TRUE, 6);

	vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 12));
	gtk_box_pack_start (vbox, GTK_WIDGET (grid), TRUE, TRUE, 6);
	gtk_box_pack_start (vbox, GTK_WIDGET (hbox), FALSE, TRUE, 6);

	gtk_widget_show_all (GTK_WIDGET (vbox));

	/* Make sure they have the dates with entries marked */
	almanah_calendar_select_today (almanah_operation->start_calendar);
	almanah_calendar_select_today (almanah_operation->end_calendar);

	return GTK_WIDGET (vbox);
}

static void
custom_widget_apply_cb (GtkPrintOperation *operation, GtkWidget *widget, AlmanahPrintOperation *almanah_operation)
{
	/* Start date */
	almanah_calendar_get_date (almanah_operation->start_calendar, almanah_operation->start_date);
	almanah_calendar_get_date (almanah_operation->start_calendar, almanah_operation->current_date);

	/* End date */
	almanah_calendar_get_date (almanah_operation->end_calendar, almanah_operation->end_date);

	/* Ensure they're in order */
	if (g_date_compare (almanah_operation->start_date, almanah_operation->end_date) > 0) {
		GDate *temp;
		temp = almanah_operation->start_date;
		almanah_operation->start_date = almanah_operation->end_date;
		almanah_operation->end_date = temp;
	}

	/* Line spacing */
	almanah_operation->line_spacing = gtk_spin_button_get_value (almanah_operation->line_spacing_spin_button);
}

void
almanah_print_entries (gboolean print_preview, GtkWindow *parent_window, GtkPageSetup **page_setup, GtkPrintSettings **print_settings,
                       AlmanahStorageManager *storage_manager)
{
	GtkPrintOperation *operation;
	GtkPrintOperationResult res;
	AlmanahPrintOperation almanah_operation;

	operation = gtk_print_operation_new ();
	almanah_operation.current_date = NULL;
	almanah_operation.paginated = FALSE;
	almanah_operation.y = 0;
	almanah_operation.current_line = 0;
	almanah_operation.storage_manager = storage_manager;

	almanah_operation.buffer = gtk_text_buffer_new (NULL);
	almanah_interface_create_text_tags (almanah_operation.buffer, FALSE);

	/* Set up default dates here for print previews */
	almanah_operation.start_date = g_date_new ();
	g_date_set_time_t (almanah_operation.start_date, time (NULL));
	g_date_subtract_months (almanah_operation.start_date, 1);

	almanah_operation.end_date = g_date_new ();
	g_date_set_time_t (almanah_operation.end_date, time (NULL));

	almanah_operation.current_date = g_memdup2 (almanah_operation.start_date, sizeof (*(almanah_operation.start_date)));

	if (*print_settings != NULL)
		gtk_print_operation_set_print_settings (operation, *print_settings);
	if (*page_setup != NULL)
		gtk_print_operation_set_default_page_setup (operation, *page_setup);

	gtk_print_operation_set_n_pages (operation, 1);

	g_signal_connect (operation, "paginate", G_CALLBACK (paginate_cb), &almanah_operation);
	g_signal_connect (operation, "draw-page", G_CALLBACK (draw_page_cb), &almanah_operation);
	g_signal_connect (operation, "create-custom-widget", G_CALLBACK (create_custom_widget_cb), &almanah_operation);
	g_signal_connect (operation, "custom-widget-apply", G_CALLBACK (custom_widget_apply_cb), &almanah_operation);

	res = gtk_print_operation_run (operation,
				       (print_preview == TRUE) ? GTK_PRINT_OPERATION_ACTION_PREVIEW : GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				       parent_window, NULL);

	if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		if (*print_settings != NULL)
			g_object_unref (*print_settings);
		*print_settings = g_object_ref (gtk_print_operation_get_print_settings (operation));

		if (*page_setup != NULL)
			g_object_unref (*page_setup);
		*page_setup = g_object_ref (gtk_print_operation_get_default_page_setup (operation));
	}

	if (almanah_operation.current_date != NULL) {
		g_object_unref (almanah_operation.buffer);
		g_date_free (almanah_operation.current_date);
		g_date_free (almanah_operation.start_date);
		g_date_free (almanah_operation.end_date);
	}
	g_object_unref (operation);
}
