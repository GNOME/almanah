/*
 * Almanah
 * Copyright (C) Jan Tojnar 2025 <jtojnar@gmail.com>
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

#include <glib-object.h>

G_BEGIN_DECLS

enum {
	SECRET_KEYS_STORE_COLUMN_ID,
	SECRET_KEYS_STORE_COLUMN_LABEL,
	SECRET_KEYS_STORE_N_COLUMNS,
};

#define ALMANAH_TYPE_SECRET_KEYS_STORE almanah_secret_keys_store_get_type ()
G_DECLARE_FINAL_TYPE (AlmanahSecretKeysStore, almanah_secret_keys_store, ALMANAH, SECRET_KEYS_STORE, GtkListStore)

AlmanahSecretKeysStore *almanah_secret_keys_store_new (void);

G_END_DECLS
