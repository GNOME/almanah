/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Diary
 * Copyright (C) Philip Withnall 2008 <philip@tecnocode.co.uk>
 * 
 * Diary is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Diary is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Diary.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "storage-manager.h"
#include "interface.h"
#include "main.h"

#define TITLE_MARGIN_BOTTOM 15 /* margin under the title, in pixels */
#define ENTRY_MARGIN_BOTTOM 10 /* margin under the entry, in pixels */
#define MAX_ORPHANS 3 /* maximum number of orphan lines to be forced to the next page */

typedef struct {
	GDate *start_date;
	GDate *end_date;
	GDate *current_date;
	GtkCalendar *start_calendar;
	GtkCalendar *end_calendar;
	guint current_line;
	gboolean paginated;
	gdouble y;
} DiaryPrintOperation;

/* TRUE if the entry was printed OK on the current page, FALSE if it needs to be moved to a new page/is split across pages */
static gboolean
print_entry (GtkPrintOperation *operation, GtkPrintContext *context, DiaryPrintOperation *diary_operation)
{
	gchar *entry, title[100];
	PangoLayout *title_layout = NULL, *entry_layout;
	PangoLayoutLine *entry_line;
	PangoRectangle logical_extents;
	gint height;
	guint i;
	gdouble title_y = 0, entry_y;
	cairo_t *cr = NULL;

	entry = diary_storage_manager_get_entry (diary->storage_manager, g_date_get_year (diary_operation->current_date),
									 g_date_get_month (diary_operation->current_date),
									 g_date_get_day (diary_operation->current_date));

	if (diary_operation->current_line == 0) {
		/* Set up the title layout */
		title_layout = gtk_print_context_create_pango_layout (context);
		pango_layout_set_width (title_layout, gtk_print_context_get_width (context) * PANGO_SCALE);

		/* Translators: This is a strftime()-format string for the date displayed above each printed entry. */
		g_date_strftime (title, sizeof (title), _("<b>%A, %e %B %Y</b>"), diary_operation->current_date);
		pango_layout_set_markup (title_layout, title, -1);

		title_y = diary_operation->y;
		pango_layout_get_pixel_size (title_layout, NULL, &height);
		diary_operation->y += height + TITLE_MARGIN_BOTTOM;
	}

	/* Set up the entry layout */
	entry_layout = gtk_print_context_create_pango_layout (context);
	pango_layout_set_width (entry_layout, gtk_print_context_get_width (context) * PANGO_SCALE);
	pango_layout_set_ellipsize (entry_layout, PANGO_ELLIPSIZE_NONE);

	if (entry == NULL)
		pango_layout_set_markup (entry_layout, _("<i>No entry for this date.</i>"), -1);
	else
		pango_layout_set_text (entry_layout, entry, -1);

	/* Check we're not orphaning things */
	entry_line = pango_layout_get_line_readonly (entry_layout, MIN (pango_layout_get_line_count (entry_layout), diary_operation->current_line + MAX_ORPHANS) - 1);
	pango_layout_line_get_pixel_extents (entry_line, NULL, &logical_extents);

	if (diary_operation->y + (MIN (pango_layout_get_line_count (entry_layout), MAX_ORPHANS) * logical_extents.height) > gtk_print_context_get_height (context)) {
		/* Not enough space on the page to contain the title and orphaned lines */
		if (title_layout != NULL)
			g_object_unref (title_layout);
		g_object_unref (entry_layout);
		g_free (entry);

		diary_operation->current_line = 0;

		return FALSE;
	}

	if (diary_operation->paginated == TRUE) {
		cr = gtk_print_context_get_cairo_context (context);

		if (diary_operation->current_line == 0) {
			/* Draw the title */
			cairo_move_to (cr, 0, title_y);
			pango_cairo_show_layout (cr, title_layout);
		}
	}

	entry_y = diary_operation->y;

	/* Draw the lines in the entry */
	for (i = diary_operation->current_line; i < pango_layout_get_line_count (entry_layout); i++) {
		entry_line = pango_layout_get_line_readonly (entry_layout, i);
		pango_layout_line_get_pixel_extents (entry_line, NULL, &logical_extents);

		/* Check we're not going to overflow the page */
		if (entry_y + logical_extents.height > gtk_print_context_get_height (context)) {
			/* Paint the rest on the next page */
			if (title_layout != NULL)
				g_object_unref (title_layout);
			g_object_unref (entry_layout);
			g_free (entry);

			diary_operation->current_line = i;

			return FALSE;
		}

		if (diary_operation->paginated == TRUE) {
			/* Draw the entry line */
			cairo_move_to (cr, 0, entry_y);
			pango_cairo_show_layout_line (cr, entry_line);
		}

		entry_y += logical_extents.height;
	}

	/* Add the entry's height to the amount printed for this page */
	pango_layout_get_pixel_size (entry_layout, NULL, &height);
	diary_operation->y = entry_y + ENTRY_MARGIN_BOTTOM;

	if (title_layout != NULL)
		g_object_unref (title_layout);
	g_object_unref (entry_layout);
	g_free (entry);

	diary_operation->current_line = 0;

	return TRUE;
}

static gboolean
paginate_cb (GtkPrintOperation *operation, GtkPrintContext *context, DiaryPrintOperation *diary_operation)
{
	/* Calculate the number of pages by laying everything out :( */
	while (g_date_compare (diary_operation->current_date, diary_operation->end_date) <= 0 &&
	       print_entry (operation, context, diary_operation) == TRUE) {
		g_date_add_days (diary_operation->current_date, 1);
	}

	if (g_date_compare (diary_operation->current_date, diary_operation->end_date) > 0) {
		/* We're done paginating */
		return TRUE;
	} else {
		/* More pages! */
		gint pages;
		g_object_get (operation, "n-pages", &pages, NULL);
		gtk_print_operation_set_n_pages (operation, pages + 1);

		diary_operation->y = 0;

		return FALSE;
	}
}

static void
draw_page_cb (GtkPrintOperation *operation, GtkPrintContext *context, gint page_number, DiaryPrintOperation *diary_operation)
{
	if (diary_operation->paginated == FALSE) {
		/* Reset things before we start printing */
		diary_operation->paginated = TRUE;
		diary_operation->current_line = 0;
		g_date_set_dmy (diary_operation->current_date,
				g_date_get_day (diary_operation->start_date),
				g_date_get_month (diary_operation->start_date),
				g_date_get_year (diary_operation->start_date));
	}

	diary_operation->y = 0;

	while (g_date_compare (diary_operation->current_date, diary_operation->end_date) <= 0 &&
	       print_entry (operation, context, diary_operation) == TRUE) {
		g_date_add_days (diary_operation->current_date, 1);
	}
}

static GtkWidget *
create_custom_widget_cb (GtkPrintOperation *operation, DiaryPrintOperation *diary_operation)
{
	GtkWidget *start_calendar, *end_calendar, *start_label, *end_label;
	GtkTable *table;

	start_calendar = gtk_calendar_new ();
	g_signal_connect (start_calendar, "month-changed", G_CALLBACK (diary_calendar_month_changed_cb), NULL);
	end_calendar = gtk_calendar_new ();
	g_signal_connect (end_calendar, "month-changed", G_CALLBACK (diary_calendar_month_changed_cb), NULL);

	start_label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (start_label), _("<b>Start Date</b>"));
	end_label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (end_label), _("<b>End Date</b>"));

	table = GTK_TABLE (gtk_table_new (2, 2, FALSE));
	gtk_table_attach (table, start_calendar, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 5, 5);
	gtk_table_attach (table, end_calendar, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 5, 5);
	gtk_table_attach (table, start_label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 5);
	gtk_table_attach (table, end_label, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 5);

	diary_operation->start_calendar = GTK_CALENDAR (start_calendar);
	diary_operation->end_calendar = GTK_CALENDAR (end_calendar);

	gtk_widget_show_all (GTK_WIDGET (table));

	/* Make sure they have the dates with entries marked */
	diary_calendar_month_changed_cb (GTK_CALENDAR (start_calendar), NULL);
	diary_calendar_month_changed_cb (GTK_CALENDAR (end_calendar), NULL);

	return GTK_WIDGET (table);
}

static void
custom_widget_apply_cb (GtkPrintOperation *operation, GtkWidget *widget, DiaryPrintOperation *diary_operation)
{
	guint year, month, day;

	/* Start date */
	gtk_calendar_get_date (diary_operation->start_calendar, &year, &month, &day);
	diary_operation->start_date = g_date_new_dmy (day, month + 1, year);
	diary_operation->current_date = g_memdup (diary_operation->start_date, sizeof (*diary_operation->start_date));

	/* End date */
	gtk_calendar_get_date (diary_operation->end_calendar, &year, &month, &day);
	diary_operation->end_date = g_date_new_dmy (day, month + 1, year);
}

void
diary_print_entries (void)
{
	GtkPrintOperation *operation;
	GtkPrintOperationResult res;
	static GtkPrintSettings *settings;
	DiaryPrintOperation diary_operation;

	operation = gtk_print_operation_new ();
	diary_operation.current_date = NULL;
	diary_operation.paginated = FALSE;
	diary_operation.y = 0;
	diary_operation.current_line = 0;

	if (settings != NULL) 
		gtk_print_operation_set_print_settings (operation, settings);

	gtk_print_operation_set_n_pages (operation, 1);

	g_signal_connect (operation, "paginate", G_CALLBACK (paginate_cb), &diary_operation);
	g_signal_connect (operation, "draw-page", G_CALLBACK (draw_page_cb), &diary_operation);
	g_signal_connect (operation, "create-custom-widget", G_CALLBACK (create_custom_widget_cb), &diary_operation);
	g_signal_connect (operation, "custom-widget-apply", G_CALLBACK (custom_widget_apply_cb), &diary_operation);

	res = gtk_print_operation_run (operation, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				       GTK_WINDOW (diary->main_window), NULL);

	if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		if (settings != NULL)
			g_object_unref (settings);
		settings = g_object_ref (gtk_print_operation_get_print_settings (operation));
	}

	if (diary_operation.current_date != NULL) {
		g_date_free (diary_operation.current_date);
		g_date_free (diary_operation.start_date);
		g_date_free (diary_operation.end_date);
	}
	g_object_unref (operation);
}

