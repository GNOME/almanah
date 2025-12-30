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

#pragma once

#include <gtk/gtk.h>

#include "storage-manager.h"

G_BEGIN_DECLS

#define ALMANAH_TYPE_TAG_ENTRY (almanah_tag_entry_get_type ())

G_DECLARE_FINAL_TYPE (AlmanahTagEntry, almanah_tag_entry, ALMANAH, TAG_ENTRY, GtkEntry)

void almanah_tag_entry_set_storage_manager (AlmanahTagEntry *tag_entry, AlmanahStorageManager *storage_manager);

G_END_DECLS
