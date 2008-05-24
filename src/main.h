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

#include <gtk/gtk.h>
#include <glib.h>
#include <gconf/gconf-client.h>

#include "storage-manager.h"

#ifndef DIARY_MAIN_H
#define DIARY_MAIN_H

G_BEGIN_DECLS

typedef struct {
	DiaryStorageManager *storage_manager;
	GConfClient *gconf_client;

	GtkWidget *main_window;
	GtkTextView *entry_view;
	GtkTextBuffer *entry_buffer;
	GtkCalendar *calendar;
	GtkLabel *date_label;
	GtkButton *add_button;
	GtkButton *remove_button;
	GtkButton *view_button;
	GtkAction *add_action;
	GtkAction *remove_action;
	GtkListStore *links_store;
	GtkTreeSelection *links_selection;
	GtkTreeViewColumn *link_value_column;
	GtkCellRendererText *link_value_renderer;

	GtkWidget *add_link_dialog;
	GtkComboBox *ald_type_combo_box;
	GtkTable *ald_table;
	GtkListStore *ald_type_store;

	gboolean debug;
	gboolean quitting;
} Diary;

Diary *diary;

void diary_quit (void);
void diary_quit_real (void);

G_END_DECLS

#endif /* DIARY_MAIN_H */
