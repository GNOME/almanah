/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Álvaro Peña 2013 <alvaropg@gmail.com>
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

#ifndef ALMANAH_ENTRY_TAGS_AREA_H
#define ALMANAH_ENTRY_TAGS_AREA_H

#include <gtk/gtk.h>
#include "entry.h"
#include "storage-manager.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_ENTRY_TAGS_AREA         (almanah_entry_tags_area_get_type ())

G_DECLARE_FINAL_TYPE (AlmanahEntryTagsArea, almanah_entry_tags_area, ALMANAH, ENTRY_TAGS_AREA, GtkFlowBox)

void  almanah_entry_tags_area_set_entry (AlmanahEntryTagsArea *entry_tags_area, AlmanahEntry *entry);
void  almanah_entry_tags_area_set_storage_manager (AlmanahEntryTagsArea *entry_tags_area, AlmanahStorageManager *storage_manager);
void  almanah_entry_tags_area_set_back_widget (AlmanahEntryTagsArea *entry_tags_area, GtkWidget *back_widget);

G_END_DECLS

#endif /* !ALMANAH_ENTRY_TAGS_AREA_H */
