/*
 * Almanah
 * Copyright (C) Philip Withnall 2009 <philip@tecnocode.co.uk>
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

#include "date-entry-dialog.h"

static void almanah_date_entry_dialog_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void almanah_date_entry_dialog_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

/* GtkBuilder callbacks */
G_MODULE_EXPORT void ded_date_entry_notify_text_cb (GObject *gobject, GParamSpec *pspec, AlmanahDateEntryDialog *self);

struct _AlmanahDateEntryDialog {
	GtkDialog parent;

	GDate date;
	GtkWidget *ok_button;
	GtkEntry *date_entry;
};

enum {
	PROP_DATE = 1
};

G_DEFINE_TYPE (AlmanahDateEntryDialog, almanah_date_entry_dialog, GTK_TYPE_DIALOG)

static void
almanah_date_entry_dialog_class_init (AlmanahDateEntryDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->set_property = almanah_date_entry_dialog_set_property;
	gobject_class->get_property = almanah_date_entry_dialog_get_property;

	g_object_class_install_property (gobject_class, PROP_DATE,
	                                 g_param_spec_boxed ("date",
	                                                     "Date", "The current date selected by the dialog.",
	                                                     G_TYPE_DATE,
	                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Almanah/ui/date-entry-dialog.ui");

	gtk_widget_class_bind_template_child (widget_class, AlmanahDateEntryDialog, ok_button);
	gtk_widget_class_bind_template_child (widget_class, AlmanahDateEntryDialog, date_entry);
}

static void
almanah_date_entry_dialog_init (AlmanahDateEntryDialog *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));

	g_date_clear (&(self->date), 1);
	g_signal_connect (self, "response", G_CALLBACK (gtk_widget_hide), self);
	gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
	gtk_window_set_title (GTK_WINDOW (self), _ ("Select Date"));
}

static void
almanah_date_entry_dialog_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AlmanahDateEntryDialog *self = ALMANAH_DATE_ENTRY_DIALOG (object);

	switch (property_id) {
		case PROP_DATE:
			g_value_set_boxed (value, &(self->date));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
almanah_date_entry_dialog_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AlmanahDateEntryDialog *self = ALMANAH_DATE_ENTRY_DIALOG (object);

	switch (property_id) {
		case PROP_DATE:
			almanah_date_entry_dialog_set_date (self, g_value_get_boxed (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

AlmanahDateEntryDialog *
almanah_date_entry_dialog_new (void)
{
	AlmanahDateEntryDialog *date_entry_dialog;

	date_entry_dialog = g_object_new (ALMANAH_TYPE_DATE_ENTRY_DIALOG, NULL);

	return date_entry_dialog;
}

G_MODULE_EXPORT void
ded_date_entry_notify_text_cb (GObject *gobject, GParamSpec *param_spec, AlmanahDateEntryDialog *self)
{
	GDate new_date;

	/* Enable/Disable the OK button based on whether the current date is valid */
	g_date_set_parse (&new_date, gtk_editable_get_text (GTK_EDITABLE (self->date_entry)));

	if (g_date_valid (&new_date) == TRUE) {
		/* The date was parsed successfully; update self->date and enable the OK button */
		self->date = new_date;
		gtk_widget_set_sensitive (self->ok_button, TRUE);
	} else {
		/* Failure due to the date entered being invalid; disable the OK button */
		gtk_widget_set_sensitive (self->ok_button, FALSE);
	}
}

void
almanah_date_entry_dialog_get_date (AlmanahDateEntryDialog *self, GDate *date)
{
	*date = self->date;
}

void
almanah_date_entry_dialog_set_date (AlmanahDateEntryDialog *self, GDate *date)
{
	self->date = *date;
	g_object_notify (G_OBJECT (self), "date");
}
