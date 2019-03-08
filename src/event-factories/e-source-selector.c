/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-source-selector.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#include <config.h>

#include <string.h>

#include "e-cell-renderer-color.h"
#include "e-source-selector.h"

#define E_SOURCE_SELECTOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SOURCE_SELECTOR, ESourceSelectorPrivate))

typedef struct _AsyncContext AsyncContext;

struct _ESourceSelectorPrivate {
	ESourceRegistry *registry;
	GHashTable *source_index;
	gchar *extension_name;

	GtkTreeRowReference *saved_primary_selection;

	/* ESource -> GSource */
	GHashTable *pending_writes;
	GMainContext *main_context;

	gboolean toggled_last;
	gboolean select_new;
	gboolean show_colors;
	gboolean show_toggles;
};

struct _AsyncContext {
	ESourceSelector *selector;
	ESource *source;
};

enum {
	PROP_0,
	PROP_EXTENSION_NAME,
	PROP_PRIMARY_SELECTION,
	PROP_REGISTRY,
	PROP_SHOW_COLORS,
	PROP_SHOW_TOGGLES
};

enum {
	SELECTION_CHANGED,
	PRIMARY_SELECTION_CHANGED,
	POPUP_EVENT,
	DATA_DROPPED,
	NUM_SIGNALS
};

enum {
	COLUMN_NAME,
	COLUMN_COLOR,
	COLUMN_ACTIVE,
	COLUMN_SHOW_COLOR,
	COLUMN_SHOW_TOGGLE,
	COLUMN_WEIGHT,
	COLUMN_SOURCE,
	NUM_COLUMNS
};

static guint signals[NUM_SIGNALS];

G_DEFINE_TYPE (ESourceSelector, e_source_selector, GTK_TYPE_TREE_VIEW)

/* ESafeToggleRenderer does not emit 'toggled' signal
 * on 'activate' when mouse is not over the toggle. */

typedef GtkCellRendererToggle ECellRendererSafeToggle;
typedef GtkCellRendererToggleClass ECellRendererSafeToggleClass;

/* Forward Declarations */
GType e_cell_renderer_safe_toggle_get_type (void);

G_DEFINE_TYPE (
	ECellRendererSafeToggle,
	e_cell_renderer_safe_toggle,
	GTK_TYPE_CELL_RENDERER_TOGGLE)

static gboolean
safe_toggle_activate (GtkCellRenderer *cell,
                      GdkEvent *event,
                      GtkWidget *widget,
                      const gchar *path,
                      const GdkRectangle *background_area,
                      const GdkRectangle *cell_area,
                      GtkCellRendererState flags)
{
	gboolean point_in_cell_area = TRUE;

	if (event->type == GDK_BUTTON_PRESS && cell_area != NULL) {
		cairo_region_t *region;

		region = cairo_region_create_rectangle (cell_area);
		point_in_cell_area = cairo_region_contains_point (
			region, event->button.x, event->button.y);
		cairo_region_destroy (region);
	}

	if (!point_in_cell_area)
		return FALSE;

	return GTK_CELL_RENDERER_CLASS (
		e_cell_renderer_safe_toggle_parent_class)->activate (
		cell, event, widget, path, background_area, cell_area, flags);
}

static void
e_cell_renderer_safe_toggle_class_init (ECellRendererSafeToggleClass *class)
{
	GtkCellRendererClass *cell_renderer_class;

	cell_renderer_class = GTK_CELL_RENDERER_CLASS (class);
	cell_renderer_class->activate = safe_toggle_activate;
}

static void
e_cell_renderer_safe_toggle_init (ECellRendererSafeToggle *obj)
{
}

static GtkCellRenderer *
e_cell_renderer_safe_toggle_new (void)
{
	return g_object_new (e_cell_renderer_safe_toggle_get_type (), NULL);
}

static void
clear_saved_primary_selection (ESourceSelector *selector)
{
	gtk_tree_row_reference_free (selector->priv->saved_primary_selection);
	selector->priv->saved_primary_selection = NULL;
}

static void
async_context_free (AsyncContext *async_context)
{
	if (async_context->selector != NULL)
		g_object_unref (async_context->selector);

	if (async_context->source != NULL)
		g_object_unref (async_context->source);

	g_slice_free (AsyncContext, async_context);
}

static void
pending_writes_destroy_source (GSource *source)
{
	g_source_destroy (source);
	g_source_unref (source);
}

static void
source_selector_write_done_cb (GObject *source_object,
                               GAsyncResult *result,
                               gpointer user_data)
{
	ESource *source;
	ESourceSelector *selector;
	GError *error = NULL;

	source = E_SOURCE (source_object);
	selector = E_SOURCE_SELECTOR (user_data);

	e_source_write_finish (source, result, &error);

	/* FIXME Display the error in the selector somehow? */
	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	g_object_unref (selector);
}

static gboolean
source_selector_write_idle_cb (gpointer user_data)
{
	AsyncContext *async_context = user_data;
	GHashTable *pending_writes;

	/* XXX This operation is not cancellable. */
	e_source_write (
		async_context->source, NULL,
		source_selector_write_done_cb,
		g_object_ref (async_context->selector));

	pending_writes = async_context->selector->priv->pending_writes;
	g_hash_table_remove (pending_writes, async_context->source);

	return FALSE;
}

static void
source_selector_cancel_write (ESourceSelector *selector,
                              ESource *source)
{
	GHashTable *pending_writes;

	/* Cancel any pending writes for this ESource so as not
	 * to overwrite whatever change we're being notified of. */
	pending_writes = selector->priv->pending_writes;
	g_hash_table_remove (pending_writes, source);
}

static void
source_selector_update_row (ESourceSelector *selector,
                            ESource *source)
{
	GHashTable *source_index;
	ESourceExtension *extension = NULL;
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	const gchar *extension_name;
	const gchar *display_name;
	gboolean selected;

	source_index = selector->priv->source_index;
	reference = g_hash_table_lookup (source_index, source);

	/* This function runs when ANY ESource in the registry changes.
	 * If the ESource is not in our tree model then return silently. */
	if (reference == NULL)
		return;

	/* If we do have a row reference, it should be valid. */
	g_return_if_fail (gtk_tree_row_reference_valid (reference));

	model = gtk_tree_row_reference_get_model (reference);
	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	display_name = e_source_get_display_name (source);

	extension_name = e_source_selector_get_extension_name (selector);
	selected = e_source_selector_source_is_selected (selector, source);

	if (e_source_has_extension (source, extension_name))
		extension = e_source_get_extension (source, extension_name);

	if (extension != NULL) {
		GdkRGBA color;
		const gchar *color_spec = NULL;
		gboolean show_color = FALSE;
		gboolean show_toggle;

		show_color =
			E_IS_SOURCE_SELECTABLE (extension) &&
			e_source_selector_get_show_colors (selector);

		if (show_color)
			color_spec = e_source_selectable_get_color (
				E_SOURCE_SELECTABLE (extension));

		if (color_spec != NULL && *color_spec != '\0')
			show_color = gdk_rgba_parse (&color, color_spec);

		show_toggle = e_source_selector_get_show_toggles (selector);

		gtk_tree_store_set (
			GTK_TREE_STORE (model), &iter,
			COLUMN_NAME, display_name,
			COLUMN_COLOR, show_color ? &color : NULL,
			COLUMN_ACTIVE, selected,
			COLUMN_SHOW_COLOR, show_color,
			COLUMN_SHOW_TOGGLE, show_toggle,
			COLUMN_WEIGHT, PANGO_WEIGHT_NORMAL,
			COLUMN_SOURCE, source,
			-1);
	} else {
		gtk_tree_store_set (
			GTK_TREE_STORE (model), &iter,
			COLUMN_NAME, display_name,
			COLUMN_COLOR, NULL,
			COLUMN_ACTIVE, FALSE,
			COLUMN_SHOW_COLOR, FALSE,
			COLUMN_SHOW_TOGGLE, FALSE,
			COLUMN_WEIGHT, PANGO_WEIGHT_BOLD,
			COLUMN_SOURCE, source,
			-1);
	}
}

static gboolean
source_selector_traverse (GNode *node,
                          ESourceSelector *selector)
{
	ESource *source;
	GHashTable *source_index;
	GtkTreeRowReference *reference = NULL;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	/* Skip the root node. */
	if (G_NODE_IS_ROOT (node))
		return FALSE;

	source_index = selector->priv->source_index;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));

	if (node->parent != NULL && node->parent->data != NULL)
		reference = g_hash_table_lookup (
			source_index, node->parent->data);

	if (gtk_tree_row_reference_valid (reference)) {
		GtkTreeIter parent;

		path = gtk_tree_row_reference_get_path (reference);
		gtk_tree_model_get_iter (model, &parent, path);
		gtk_tree_path_free (path);

		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent);
	} else
		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);

	source = E_SOURCE (node->data);

	path = gtk_tree_model_get_path (model, &iter);
	reference = gtk_tree_row_reference_new (model, path);
	g_hash_table_insert (source_index, g_object_ref (source), reference);
	gtk_tree_path_free (path);

	source_selector_update_row (selector, source);

	return FALSE;
}

static void
source_selector_save_expanded (GtkTreeView *tree_view,
                               GtkTreePath *path,
                               GQueue *queue)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	ESource *source;

	model = gtk_tree_view_get_model (tree_view);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);
	g_queue_push_tail (queue, source);
}

static void
source_selector_build_model (ESourceSelector *selector)
{
	ESourceRegistry *registry;
	GQueue queue = G_QUEUE_INIT;
	GHashTable *source_index;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	ESource *selected;
	const gchar *extension_name;
	GNode *root;

	tree_view = GTK_TREE_VIEW (selector);

	registry = e_source_selector_get_registry (selector);
	extension_name = e_source_selector_get_extension_name (selector);

	/* Make sure we have what we need to build the model, since
	 * this can get called early in the initialization phase. */
	if (registry == NULL || extension_name == NULL)
		return;

	source_index = selector->priv->source_index;
	selected = e_source_selector_ref_primary_selection (selector);

	/* Save expanded sources to restore later. */
	gtk_tree_view_map_expanded_rows (
		tree_view, (GtkTreeViewMappingFunc)
		source_selector_save_expanded, &queue);

	model = gtk_tree_view_get_model (tree_view);
	gtk_tree_store_clear (GTK_TREE_STORE (model));

	g_hash_table_remove_all (source_index);

	root = e_source_registry_build_display_tree (registry, extension_name);

	g_node_traverse (
		root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
		(GNodeTraverseFunc) source_selector_traverse,
		selector);

	e_source_registry_free_display_tree (root);

	/* Restore previously expanded sources. */
	while (!g_queue_is_empty (&queue)) {
		GtkTreeRowReference *reference;
		ESource *source;

		source = g_queue_pop_head (&queue);
		reference = g_hash_table_lookup (source_index, source);

		if (gtk_tree_row_reference_valid (reference)) {
			GtkTreePath *path;

			path = gtk_tree_row_reference_get_path (reference);
			gtk_tree_view_expand_to_path (tree_view, path);
			gtk_tree_path_free (path);
		}

		g_object_unref (source);
	}

	/* Restore the primary selection. */
	if (selected != NULL) {
		e_source_selector_set_primary_selection (selector, selected);
		g_object_unref (selected);
	}

	/* Make sure we have a primary selection.  If not, pick one. */
	selected = e_source_selector_ref_primary_selection (selector);
	if (selected == NULL) {
		selected = e_source_registry_ref_default_for_extension_name (
			registry, extension_name);
		e_source_selector_set_primary_selection (selector, selected);
	}
	g_object_unref (selected);
}

static void
source_selector_expand_to_source (ESourceSelector *selector,
                                  ESource *source)
{
	GHashTable *source_index;
	GtkTreeRowReference *reference;
	GtkTreePath *path;

	source_index = selector->priv->source_index;
	reference = g_hash_table_lookup (source_index, source);

	/* If the ESource is not in our tree model then return silently. */
	if (reference == NULL)
		return;

	/* If we do have a row reference, it should be valid. */
	g_return_if_fail (gtk_tree_row_reference_valid (reference));

	/* Expand the tree view to the path containing the ESource */
	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_view_expand_to_path (GTK_TREE_VIEW (selector), path);
	gtk_tree_path_free (path);
}

static void
source_selector_source_added_cb (ESourceRegistry *registry,
                                 ESource *source,
                                 ESourceSelector *selector)
{
	source_selector_build_model (selector);

	source_selector_expand_to_source (selector, source);
}

static void
source_selector_source_changed_cb (ESourceRegistry *registry,
                                   ESource *source,
                                   ESourceSelector *selector)
{
	source_selector_cancel_write (selector, source);

	source_selector_update_row (selector, source);
}

static void
source_selector_source_removed_cb (ESourceRegistry *registry,
                                   ESource *source,
                                   ESourceSelector *selector)
{
	source_selector_build_model (selector);
}

static void
source_selector_source_enabled_cb (ESourceRegistry *registry,
                                   ESource *source,
                                   ESourceSelector *selector)
{
	source_selector_build_model (selector);

	source_selector_expand_to_source (selector, source);
}

static void
source_selector_source_disabled_cb (ESourceRegistry *registry,
                                    ESource *source,
                                    ESourceSelector *selector)
{
	source_selector_build_model (selector);
}

static gboolean
same_source_name_exists (ESourceSelector *selector,
                         const gchar *display_name)
{
	GHashTable *source_index;
	GHashTableIter iter;
	gpointer key;

	source_index = selector->priv->source_index;
	g_hash_table_iter_init (&iter, source_index);

	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		ESource *source = E_SOURCE (key);
		const gchar *source_name;

		source_name = e_source_get_display_name (source);
		if (g_strcmp0 (display_name, source_name) == 0)
			return TRUE;
	}

	return FALSE;
}

static gboolean
selection_func (GtkTreeSelection *selection,
                GtkTreeModel *model,
                GtkTreePath *path,
                gboolean path_currently_selected,
                ESourceSelector *selector)
{
	ESource *source;
	GtkTreeIter iter;
	const gchar *extension_name;

	if (selector->priv->toggled_last) {
		selector->priv->toggled_last = FALSE;
		return FALSE;
	}

	if (path_currently_selected)
		return TRUE;

	if (!gtk_tree_model_get_iter (model, &iter, path))
		return FALSE;

	extension_name = e_source_selector_get_extension_name (selector);
	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

	if (!e_source_has_extension (source, extension_name)) {
		g_object_unref (source);
		return FALSE;
	}

	clear_saved_primary_selection (selector);

	g_object_unref (source);

	return TRUE;
}

static void
text_cell_edited_cb (ESourceSelector *selector,
                     const gchar *path_string,
                     const gchar *new_name)
{
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	ESource *source;

	tree_view = GTK_TREE_VIEW (selector);
	model = gtk_tree_view_get_model (tree_view);
	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);
	gtk_tree_path_free (path);

	if (new_name == NULL || *new_name == '\0')
		return;

	if (same_source_name_exists (selector, new_name))
		return;

	e_source_set_display_name (source, new_name);

	e_source_selector_queue_write (selector, source);
}

static void
cell_toggled_callback (GtkCellRendererToggle *renderer,
                       const gchar *path_string,
                       ESourceSelector *selector)
{
	ESource *source;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));
	path = gtk_tree_path_new_from_string (path_string);

	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		return;
	}

	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

	if (e_source_selector_source_is_selected (selector, source))
		e_source_selector_unselect_source (selector, source);
	else
		e_source_selector_select_source (selector, source);

	selector->priv->toggled_last = TRUE;

	gtk_tree_path_free (path);

	g_object_unref (source);
}

static void
selection_changed_callback (GtkTreeSelection *selection,
                            ESourceSelector *selector)
{
	g_signal_emit (selector, signals[PRIMARY_SELECTION_CHANGED], 0);
	g_object_notify (G_OBJECT (selector), "primary-selection");
}

static void
source_selector_set_extension_name (ESourceSelector *selector,
                                    const gchar *extension_name)
{
	g_return_if_fail (extension_name != NULL);
	g_return_if_fail (selector->priv->extension_name == NULL);

	selector->priv->extension_name = g_strdup (extension_name);
}

static void
source_selector_set_registry (ESourceSelector *selector,
                              ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (selector->priv->registry == NULL);

	selector->priv->registry = g_object_ref (registry);
}

static void
source_selector_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXTENSION_NAME:
			source_selector_set_extension_name (
				E_SOURCE_SELECTOR (object),
				g_value_get_string (value));
			return;

		case PROP_PRIMARY_SELECTION:
			e_source_selector_set_primary_selection (
				E_SOURCE_SELECTOR (object),
				g_value_get_object (value));
			return;

		case PROP_REGISTRY:
			source_selector_set_registry (
				E_SOURCE_SELECTOR (object),
				g_value_get_object (value));
			return;

		case PROP_SHOW_COLORS:
			e_source_selector_set_show_colors (
				E_SOURCE_SELECTOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_TOGGLES:
			e_source_selector_set_show_toggles (
				E_SOURCE_SELECTOR (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_selector_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXTENSION_NAME:
			g_value_set_string (
				value,
				e_source_selector_get_extension_name (
				E_SOURCE_SELECTOR (object)));
			return;

		case PROP_PRIMARY_SELECTION:
			g_value_take_object (
				value,
				e_source_selector_ref_primary_selection (
				E_SOURCE_SELECTOR (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_source_selector_get_registry (
				E_SOURCE_SELECTOR (object)));
			return;

		case PROP_SHOW_COLORS:
			g_value_set_boolean (
				value,
				e_source_selector_get_show_colors (
				E_SOURCE_SELECTOR (object)));
			return;

		case PROP_SHOW_TOGGLES:
			g_value_set_boolean (
				value,
				e_source_selector_get_show_toggles (
				E_SOURCE_SELECTOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_selector_dispose (GObject *object)
{
	ESourceSelectorPrivate *priv;

	priv = E_SOURCE_SELECTOR_GET_PRIVATE (object);

	if (priv->registry != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->registry,
			G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->registry);
		priv->registry = NULL;
	}

	g_hash_table_remove_all (priv->source_index);
	g_hash_table_remove_all (priv->pending_writes);

	clear_saved_primary_selection (E_SOURCE_SELECTOR (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_source_selector_parent_class)->dispose (object);
}

static void
source_selector_finalize (GObject *object)
{
	ESourceSelectorPrivate *priv;

	priv = E_SOURCE_SELECTOR_GET_PRIVATE (object);

	g_hash_table_destroy (priv->source_index);
	g_hash_table_destroy (priv->pending_writes);

	g_free (priv->extension_name);

	if (priv->main_context != NULL)
		g_main_context_unref (priv->main_context);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_source_selector_parent_class)->finalize (object);
}

static void
source_selector_constructed (GObject *object)
{
	ESourceRegistry *registry;
	ESourceSelector *selector;

	selector = E_SOURCE_SELECTOR (object);
	registry = e_source_selector_get_registry (selector);

	g_signal_connect (
		registry, "source-added",
		G_CALLBACK (source_selector_source_added_cb), selector);

	g_signal_connect (
		registry, "source-changed",
		G_CALLBACK (source_selector_source_changed_cb), selector);

	g_signal_connect (
		registry, "source-removed",
		G_CALLBACK (source_selector_source_removed_cb), selector);

	g_signal_connect (
		registry, "source-enabled",
		G_CALLBACK (source_selector_source_enabled_cb), selector);

	g_signal_connect (
		registry, "source-disabled",
		G_CALLBACK (source_selector_source_disabled_cb), selector);

	source_selector_build_model (selector);

	gtk_tree_view_expand_all (GTK_TREE_VIEW (selector));
}

static gboolean
source_selector_button_press_event (GtkWidget *widget,
                                    GdkEventButton *event)
{
	ESourceSelector *selector;
	GtkWidgetClass *widget_class;
	GtkTreePath *path;
	ESource *source = NULL;
	ESource *primary;
	gboolean right_click = FALSE;
	gboolean triple_click = FALSE;
	gboolean row_exists;
	gboolean res = FALSE;

	selector = E_SOURCE_SELECTOR (widget);

	selector->priv->toggled_last = FALSE;

	/* Triple-clicking a source selects it exclusively. */

	if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
		right_click = TRUE;
	else if (event->button == 1 && event->type == GDK_3BUTTON_PRESS)
		triple_click = TRUE;
	else
		goto chainup;

	row_exists = gtk_tree_view_get_path_at_pos (
		GTK_TREE_VIEW (widget), event->x, event->y,
		&path, NULL, NULL, NULL);

	/* Get the source/group */
	if (row_exists) {
		GtkTreeModel *model;
		GtkTreeIter iter;

		model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);
	}

	if (source == NULL)
		goto chainup;

	primary = e_source_selector_ref_primary_selection (selector);
	if (source != primary)
		e_source_selector_set_primary_selection (selector, source);
	if (primary != NULL)
		g_object_unref (primary);

	if (right_click)
		g_signal_emit (
			widget, signals[POPUP_EVENT], 0, source, event, &res);

	if (triple_click) {
		e_source_selector_select_exclusive (selector, source);
		res = TRUE;
	}

	g_object_unref (source);

	return res;

chainup:

	/* Chain up to parent's button_press_event() method. */
	widget_class = GTK_WIDGET_CLASS (e_source_selector_parent_class);
	return widget_class->button_press_event (widget, event);
}

static void
source_selector_drag_leave (GtkWidget *widget,
                            GdkDragContext *context,
                            guint time_)
{
	GtkTreeView *tree_view;
	GtkTreeViewDropPosition pos;

	tree_view = GTK_TREE_VIEW (widget);
	pos = GTK_TREE_VIEW_DROP_BEFORE;

	gtk_tree_view_set_drag_dest_row (tree_view, NULL, pos);
}

static gboolean
source_selector_drag_motion (GtkWidget *widget,
                             GdkDragContext *context,
                             gint x,
                             gint y,
                             guint time_)
{
	ESource *source = NULL;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	GtkTreeViewDropPosition pos;
	GdkDragAction action = 0;

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);

	if (!gtk_tree_view_get_dest_row_at_pos (tree_view, x, y, &path, NULL))
		goto exit;

	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto exit;

	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

	if (!e_source_get_writable (source))
		goto exit;

	pos = GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
	gtk_tree_view_set_drag_dest_row (tree_view, path, pos);

	if (gdk_drag_context_get_actions (context) & GDK_ACTION_MOVE)
		action = GDK_ACTION_MOVE;
	else
		action = gdk_drag_context_get_suggested_action (context);

exit:
	if (path != NULL)
		gtk_tree_path_free (path);

	if (source != NULL)
		g_object_unref (source);

	gdk_drag_status (context, action, time_);

	return TRUE;
}

static gboolean
source_selector_drag_drop (GtkWidget *widget,
                           GdkDragContext *context,
                           gint x,
                           gint y,
                           guint time_)
{
	ESource *source;
	ESourceSelector *selector;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	const gchar *extension_name;
	gboolean drop_zone;
	gboolean valid;

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);

	if (!gtk_tree_view_get_path_at_pos (
		tree_view, x, y, &path, NULL, NULL, NULL))
		return FALSE;

	valid = gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);
	g_return_val_if_fail (valid, FALSE);

	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

	selector = E_SOURCE_SELECTOR (widget);
	extension_name = e_source_selector_get_extension_name (selector);
	drop_zone = e_source_has_extension (source, extension_name);

	g_object_unref (source);

	return drop_zone;
}

static void
source_selector_drag_data_received (GtkWidget *widget,
                                    GdkDragContext *context,
                                    gint x,
                                    gint y,
                                    GtkSelectionData *selection_data,
                                    guint info,
                                    guint time_)
{
	ESource *source = NULL;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	GdkDragAction action;
	gboolean delete;
	gboolean success = FALSE;

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);

	action = gdk_drag_context_get_selected_action (context);
	delete = (action == GDK_ACTION_MOVE);

	if (!gtk_tree_view_get_dest_row_at_pos (tree_view, x, y, &path, NULL))
		goto exit;

	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto exit;

	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

	if (!e_source_get_writable (source))
		goto exit;

	g_signal_emit (
		widget, signals[DATA_DROPPED], 0, selection_data,
		source, gdk_drag_context_get_selected_action (context),
		info, &success);

exit:
	if (path != NULL)
		gtk_tree_path_free (path);

	if (source != NULL)
		g_object_unref (source);

	gtk_drag_finish (context, success, delete, time_);
}

static gboolean
source_selector_popup_menu (GtkWidget *widget)
{
	ESourceSelector *selector;
	ESource *source;
	gboolean res = FALSE;

	selector = E_SOURCE_SELECTOR (widget);
	source = e_source_selector_ref_primary_selection (selector);
	g_signal_emit (selector, signals[POPUP_EVENT], 0, source, NULL, &res);

	if (source != NULL)
		g_object_unref (source);

	return res;
}

static gboolean
source_selector_test_collapse_row (GtkTreeView *tree_view,
                                   GtkTreeIter *iter,
                                   GtkTreePath *path)
{
	ESourceSelectorPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter child_iter;

	priv = E_SOURCE_SELECTOR_GET_PRIVATE (tree_view);

	/* Clear this because something else has been clicked on now */
	priv->toggled_last = FALSE;

	if (priv->saved_primary_selection)
		return FALSE;

	selection = gtk_tree_view_get_selection (tree_view);

	if (!gtk_tree_selection_get_selected (selection, &model, &child_iter))
		return FALSE;

	if (gtk_tree_store_is_ancestor (GTK_TREE_STORE (model), iter, &child_iter)) {
		GtkTreeRowReference *reference;
		GtkTreePath *child_path;

		child_path = gtk_tree_model_get_path (model, &child_iter);
		reference = gtk_tree_row_reference_new (model, child_path);
		priv->saved_primary_selection = reference;
		gtk_tree_path_free (child_path);
	}

	return FALSE;
}

static void
source_selector_row_expanded (GtkTreeView *tree_view,
                              GtkTreeIter *iter,
                              GtkTreePath *path)
{
	ESourceSelectorPrivate *priv;
	GtkTreeModel *model;
	GtkTreePath *child_path;
	GtkTreeIter child_iter;

	priv = E_SOURCE_SELECTOR_GET_PRIVATE (tree_view);

	if (!priv->saved_primary_selection)
		return;

	model = gtk_tree_view_get_model (tree_view);

	child_path = gtk_tree_row_reference_get_path (
		priv->saved_primary_selection);
	gtk_tree_model_get_iter (model, &child_iter, child_path);

	if (gtk_tree_store_is_ancestor (GTK_TREE_STORE (model), iter, &child_iter)) {
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection (tree_view);
		gtk_tree_selection_select_iter (selection, &child_iter);

		clear_saved_primary_selection (E_SOURCE_SELECTOR (tree_view));
	}

	gtk_tree_path_free (child_path);
}

static gboolean
source_selector_get_source_selected (ESourceSelector *selector,
                                     ESource *source)
{
	ESourceSelectable *extension;
	const gchar *extension_name;
	gboolean selected = TRUE;

	extension_name = e_source_selector_get_extension_name (selector);

	if (!e_source_has_extension (source, extension_name))
		return FALSE;

	extension = e_source_get_extension (source, extension_name);

	if (E_IS_SOURCE_SELECTABLE (extension))
		selected = e_source_selectable_get_selected (extension);

	return selected;
}

static void
source_selector_set_source_selected (ESourceSelector *selector,
                                     ESource *source,
                                     gboolean selected)
{
	ESourceSelectable *extension;
	const gchar *extension_name;

	extension_name = e_source_selector_get_extension_name (selector);

	if (!e_source_has_extension (source, extension_name))
		return;

	extension = e_source_get_extension (source, extension_name);

	if (!E_IS_SOURCE_SELECTABLE (extension))
		return;

	if (selected != e_source_selectable_get_selected (extension)) {
		e_source_selectable_set_selected (extension, selected);
		e_source_selector_queue_write (selector, source);
	}
}

static gboolean
ess_bool_accumulator (GSignalInvocationHint *ihint,
                      GValue *out,
                      const GValue *in,
                      gpointer data)
{
	gboolean v_boolean;

	v_boolean = g_value_get_boolean (in);
	g_value_set_boolean (out, v_boolean);

	return !v_boolean;
}

static void
e_source_selector_class_init (ESourceSelectorClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkTreeViewClass *tree_view_class;

	g_type_class_add_private (class, sizeof (ESourceSelectorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = source_selector_set_property;
	object_class->get_property = source_selector_get_property;
	object_class->dispose  = source_selector_dispose;
	object_class->finalize = source_selector_finalize;
	object_class->constructed = source_selector_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = source_selector_button_press_event;
	widget_class->drag_leave = source_selector_drag_leave;
	widget_class->drag_motion = source_selector_drag_motion;
	widget_class->drag_drop = source_selector_drag_drop;
	widget_class->drag_data_received = source_selector_drag_data_received;
	widget_class->popup_menu = source_selector_popup_menu;

	tree_view_class = GTK_TREE_VIEW_CLASS (class);
	tree_view_class->test_collapse_row = source_selector_test_collapse_row;
	tree_view_class->row_expanded = source_selector_row_expanded;

	class->get_source_selected = source_selector_get_source_selected;
	class->set_source_selected = source_selector_set_source_selected;

	g_object_class_install_property (
		object_class,
		PROP_EXTENSION_NAME,
		g_param_spec_string (
			"extension-name",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_PRIMARY_SELECTION,
		g_param_spec_object (
			"primary-selection",
			NULL,
			NULL,
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			NULL,
			NULL,
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_COLORS,
		g_param_spec_boolean (
			"show-colors",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_TOGGLES,
		g_param_spec_boolean (
			"show-toggles",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	signals[SELECTION_CHANGED] = g_signal_new (
		"selection-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESourceSelectorClass, selection_changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);

	/* XXX Consider this signal deprecated.  Connect
	 *     to "notify::primary-selection" instead. */
	signals[PRIMARY_SELECTION_CHANGED] = g_signal_new (
		"primary-selection-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESourceSelectorClass, primary_selection_changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);

	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESourceSelectorClass, popup_event),
		ess_bool_accumulator, NULL, NULL,
		G_TYPE_BOOLEAN, 2, G_TYPE_OBJECT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[DATA_DROPPED] = g_signal_new (
		"data-dropped",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESourceSelectorClass, data_dropped),
		NULL, NULL, NULL,
		G_TYPE_BOOLEAN, 4,
		GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		E_TYPE_SOURCE,
		GDK_TYPE_DRAG_ACTION,
		G_TYPE_UINT);
}

static void
e_source_selector_init (ESourceSelector *selector)
{
	GHashTable *pending_writes;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkTreeStore *tree_store;
	GtkTreeView *tree_view;

	pending_writes = g_hash_table_new_full (
		(GHashFunc) g_direct_hash,
		(GEqualFunc) g_direct_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) pending_writes_destroy_source);

	selector->priv = E_SOURCE_SELECTOR_GET_PRIVATE (selector);

	selector->priv->pending_writes = pending_writes;

	selector->priv->main_context = g_main_context_get_thread_default ();
	if (selector->priv->main_context != NULL)
		g_main_context_ref (selector->priv->main_context);

	tree_view = GTK_TREE_VIEW (selector);

	gtk_tree_view_set_search_column (tree_view, COLUMN_SOURCE);
	gtk_tree_view_set_enable_search (tree_view, TRUE);

	selector->priv->toggled_last = FALSE;
	selector->priv->select_new = FALSE;
	selector->priv->show_colors = TRUE;
	selector->priv->show_toggles = TRUE;

	selector->priv->source_index = g_hash_table_new_full (
		(GHashFunc) e_source_hash,
		(GEqualFunc) e_source_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) gtk_tree_row_reference_free);

	tree_store = gtk_tree_store_new (
		NUM_COLUMNS,
		G_TYPE_STRING,		/* COLUMN_NAME */
		GDK_TYPE_COLOR,		/* COLUMN_COLOR */
		G_TYPE_BOOLEAN,		/* COLUMN_ACTIVE */
		G_TYPE_BOOLEAN,		/* COLUMN_SHOW_COLOR */
		G_TYPE_BOOLEAN,		/* COLUMN_SHOW_TOGGLE */
		G_TYPE_INT,		/* COLUMN_WEIGHT */
		E_TYPE_SOURCE);		/* COLUMN_SOURCE */

	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (tree_store));

	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (tree_view, column);

	renderer = e_cell_renderer_color_new ();
	g_object_set (
		G_OBJECT (renderer), "mode",
		GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "color", COLUMN_COLOR);
	gtk_tree_view_column_add_attribute (
		column, renderer, "visible", COLUMN_SHOW_COLOR);

	renderer = e_cell_renderer_safe_toggle_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "active", COLUMN_ACTIVE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "visible", COLUMN_SHOW_TOGGLE);
	g_signal_connect (
		renderer, "toggled",
		G_CALLBACK (cell_toggled_callback), selector);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (
		G_OBJECT (renderer),
		"ellipsize", PANGO_ELLIPSIZE_END, NULL);
	g_signal_connect_swapped (
		renderer, "edited",
		G_CALLBACK (text_cell_edited_cb), selector);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (
		column, renderer,
		"text", COLUMN_NAME,
		"weight", COLUMN_WEIGHT,
		NULL);

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_select_function (
		selection, (GtkTreeSelectionFunc)
		selection_func, selector, NULL);
	g_signal_connect_object (
		selection, "changed",
		G_CALLBACK (selection_changed_callback),
		G_OBJECT (selector), 0);

	gtk_tree_view_set_headers_visible (tree_view, FALSE);
}

/**
 * e_source_selector_new:
 * @registry: an #ESourceRegistry
 * @extension_name: the name of an #ESource extension
 *
 * Displays a list of sources from @registry having an extension named
 * @extension_name.  The sources are grouped by backend or groupware
 * account, which are described by the parent source.
 *
 * Returns: a new #ESourceSelector
 **/
GtkWidget *
e_source_selector_new (ESourceRegistry *registry,
                       const gchar *extension_name)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (extension_name != NULL, NULL);

	return g_object_new (
		E_TYPE_SOURCE_SELECTOR, "registry", registry,
		"extension-name", extension_name, NULL);
}

/**
 * e_source_selector_get_registry:
 * @selector: an #ESourceSelector
 *
 * Returns the #ESourceRegistry that @selector is getting sources from.
 *
 * Returns: an #ESourceRegistry
 *
 * Since: 3.6
 **/
ESourceRegistry *
e_source_selector_get_registry (ESourceSelector *selector)
{
	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), NULL);

	return selector->priv->registry;
}

/**
 * e_source_selector_get_extension_name:
 * @selector: an #ESourceSelector
 *
 * Returns the extension name used to filter which sources are displayed.
 *
 * Returns: the #ESource extension name
 *
 * Since: 3.6
 **/
const gchar *
e_source_selector_get_extension_name (ESourceSelector *selector)
{
	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), NULL);

	return selector->priv->extension_name;
}

/**
 * e_source_selector_get_show_colors:
 * @selector: an #ESourceSelector
 *
 * Returns whether colors are shown next to data sources.
 *
 * Returns: %TRUE if colors are being shown
 *
 * Since: 3.6
 **/
gboolean
e_source_selector_get_show_colors (ESourceSelector *selector)
{
	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), FALSE);

	return selector->priv->show_colors;
}

/**
 * e_source_selector_set_show_colors:
 * @selector: an #ESourceSelector
 * @show_colors: whether to show colors
 *
 * Sets whether to show colors next to data sources.
 *
 * Since: 3.6
 **/
void
e_source_selector_set_show_colors (ESourceSelector *selector,
                                   gboolean show_colors)
{
	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));

	if ((show_colors ? 1 : 0) == (selector->priv->show_colors ? 1 : 0))
		return;

	selector->priv->show_colors = show_colors;

	g_object_notify (G_OBJECT (selector), "show-colors");

	source_selector_build_model (selector);
}

/**
 * e_source_selector_get_show_toggles:
 * @selector: an #ESourceSelector
 *
 * Returns whether toggles are shown next to data sources.
 *
 * Returns: %TRUE if toggles are being shown
 *
 * Since: 3.6
 **/
gboolean
e_source_selector_get_show_toggles (ESourceSelector *selector)
{
	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), FALSE);

	return selector->priv->show_toggles;
}

/**
 * e_source_selector_set_show_toggles:
 * @selector: an #ESourceSelector
 * @show_toggles: whether to show toggles
 *
 * Sets whether to show toggles next to data sources.
 *
 * Since: 3.6
 **/
void
e_source_selector_set_show_toggles (ESourceSelector *selector,
                                   gboolean show_toggles)
{
	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));

	if ((show_toggles ? 1 : 0) == (selector->priv->show_toggles ? 1 : 0))
		return;

	selector->priv->show_toggles = show_toggles;

	g_object_notify (G_OBJECT (selector), "show-toggles");

	source_selector_build_model (selector);
}

/* Helper for e_source_selector_get_selection() */
static gboolean
source_selector_check_selected (GtkTreeModel *model,
                                GtkTreePath *path,
                                GtkTreeIter *iter,
                                gpointer user_data)
{
	ESource *source;

	struct {
		ESourceSelector *selector;
		GSList *list;
	} *closure = user_data;

	gtk_tree_model_get (model, iter, COLUMN_SOURCE, &source, -1);

	if (e_source_selector_source_is_selected (closure->selector, source))
		closure->list = g_slist_prepend (closure->list, source);
	else
		g_object_unref (source);

	return FALSE;
}

/**
 * e_source_selector_get_selection:
 * @selector: an #ESourceSelector
 *
 * Get the list of selected sources, i.e. those that were enabled through the
 * corresponding checkboxes in the tree.
 *
 * Returns: A list of the ESources currently selected.  The sources will
 * be in the same order as they appear on the screen, and the list should be
 * freed using e_source_selector_free_selection().
 **/
GSList *
e_source_selector_get_selection (ESourceSelector *selector)
{
	struct {
		ESourceSelector *selector;
		GSList *list;
	} closure;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), NULL);

	closure.selector = selector;
	closure.list = NULL;

	gtk_tree_model_foreach (
		gtk_tree_view_get_model (GTK_TREE_VIEW (selector)),
		(GtkTreeModelForeachFunc) source_selector_check_selected,
		&closure);

	return g_slist_reverse (closure.list);
}

/**
 * e_source_list_free_selection:
 * @list: A selection list returned by e_source_selector_get_selection().
 *
 * Free the selection list.
 **/
void
e_source_selector_free_selection (GSList *list)
{
	g_slist_foreach (list, (GFunc) g_object_unref, NULL);
	g_slist_free (list);
}

/**
 * e_source_selector_set_select_new:
 * @selector: An #ESourceSelector widget
 * @state: A gboolean
 *
 * Set whether or not to select new sources added to @selector.
 **/
void
e_source_selector_set_select_new (ESourceSelector *selector,
                                  gboolean state)
{
	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));

	selector->priv->select_new = state;
}

/**
 * e_source_selector_select_source:
 * @selector: An #ESourceSelector widget
 * @source: An #ESource.
 *
 * Select @source in @selector.
 **/
void
e_source_selector_select_source (ESourceSelector *selector,
                                 ESource *source)
{
	ESourceSelectorClass *class;
	GtkTreeRowReference *reference;
	GHashTable *source_index;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	/* Make sure the ESource is in our tree model. */
	source_index = selector->priv->source_index;
	reference = g_hash_table_lookup (source_index, source);
	g_return_if_fail (gtk_tree_row_reference_valid (reference));

	class = E_SOURCE_SELECTOR_GET_CLASS (selector);
	g_return_if_fail (class->set_source_selected != NULL);

	class->set_source_selected (selector, source, TRUE);

	g_signal_emit (selector, signals[SELECTION_CHANGED], 0);
}

/**
 * e_source_selector_unselect_source:
 * @selector: An #ESourceSelector widget
 * @source: An #ESource.
 *
 * Unselect @source in @selector.
 **/
void
e_source_selector_unselect_source (ESourceSelector *selector,
                                   ESource *source)
{
	ESourceSelectorClass *class;
	GtkTreeRowReference *reference;
	GHashTable *source_index;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	/* Make sure the ESource is in our tree model. */
	source_index = selector->priv->source_index;
	reference = g_hash_table_lookup (source_index, source);
	g_return_if_fail (gtk_tree_row_reference_valid (reference));

	class = E_SOURCE_SELECTOR_GET_CLASS (selector);
	g_return_if_fail (class->set_source_selected != NULL);

	class->set_source_selected (selector, source, FALSE);

	g_signal_emit (selector, signals[SELECTION_CHANGED], 0);
}

/**
 * e_source_selector_select_exclusive:
 * @selector: An #ESourceSelector widget
 * @source: An #ESource.
 *
 * Select @source in @selector and unselect all others.
 *
 * Since: 2.30
 **/
void
e_source_selector_select_exclusive (ESourceSelector *selector,
                                    ESource *source)
{
	ESourceSelectorClass *class;
	GHashTable *source_index;
	GHashTableIter iter;
	gpointer key;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	class = E_SOURCE_SELECTOR_GET_CLASS (selector);
	g_return_if_fail (class->set_source_selected != NULL);

	source_index = selector->priv->source_index;
	g_hash_table_iter_init (&iter, source_index);

	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		gboolean selected = e_source_equal (key, source);
		class->set_source_selected (selector, key, selected);
	}

	g_signal_emit (selector, signals[SELECTION_CHANGED], 0);
}

/**
 * e_source_selector_source_is_selected:
 * @selector: An #ESourceSelector widget
 * @source: An #ESource.
 *
 * Check whether @source is selected in @selector.
 *
 * Returns: %TRUE if @source is currently selected, %FALSE otherwise.
 **/
gboolean
e_source_selector_source_is_selected (ESourceSelector *selector,
                                      ESource *source)
{
	ESourceSelectorClass *class;
	GtkTreeRowReference *reference;
	GHashTable *source_index;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	/* Make sure the ESource is in our tree model. */
	source_index = selector->priv->source_index;
	reference = g_hash_table_lookup (source_index, source);
	g_return_val_if_fail (gtk_tree_row_reference_valid (reference), FALSE);

	class = E_SOURCE_SELECTOR_GET_CLASS (selector);
	g_return_val_if_fail (class->get_source_selected != NULL, FALSE);

	return class->get_source_selected (selector, source);
}

/**
 * e_source_selector_edit_primary_selection:
 * @selector: An #ESourceSelector widget
 *
 * Allows the user to rename the primary selected source by opening an
 * entry box directly in @selector.
 *
 * Since: 2.26
 **/
void
e_source_selector_edit_primary_selection (ESourceSelector *selector)
{
	GtkTreeRowReference *reference;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	GList *list;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));

	tree_view = GTK_TREE_VIEW (selector);
	column = gtk_tree_view_get_column (tree_view, 0);
	reference = selector->priv->saved_primary_selection;
	selection = gtk_tree_view_get_selection (tree_view);

	if (reference != NULL)
		path = gtk_tree_row_reference_get_path (reference);
	else if (gtk_tree_selection_get_selected (selection, &model, &iter))
		path = gtk_tree_model_get_path (model, &iter);

	if (path == NULL)
		return;

	/* XXX Because we stuff three renderers in a single column,
	 *     we have to manually hunt for the text renderer. */
	renderer = NULL;
	list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	while (list != NULL) {
		renderer = list->data;
		if (GTK_IS_CELL_RENDERER_TEXT (renderer))
			break;
		list = g_list_delete_link (list, list);
	}
	g_list_free (list);

	/* Make the text cell renderer editable, but only temporarily.
	 * We don't want editing to be activated by simply clicking on
	 * the source name.  Too easy for accidental edits to occur. */
	g_object_set (renderer, "editable", TRUE, NULL);
	gtk_tree_view_expand_to_path (tree_view, path);
	gtk_tree_view_set_cursor_on_cell (
		tree_view, path, column, renderer, TRUE);
	g_object_set (renderer, "editable", FALSE, NULL);

	gtk_tree_path_free (path);
}

/**
 * e_source_selector_ref_primary_selection:
 * @selector: An #ESourceSelector widget
 *
 * Get the primary selected source.  The primary selection is the one that is
 * highlighted through the normal #GtkTreeView selection mechanism (as opposed
 * to the "normal" selection, which is the set of source whose checkboxes are
 * checked).
 *
 * The returned #ESource is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: The selected source.
 *
 * Since: 3.6
 **/
ESource *
e_source_selector_ref_primary_selection (ESourceSelector *selector)
{
	ESource *source;
	GtkTreeRowReference *reference;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	const gchar *extension_name;
	gboolean have_iter = FALSE;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), NULL);

	tree_view = GTK_TREE_VIEW (selector);
	model = gtk_tree_view_get_model (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	reference = selector->priv->saved_primary_selection;

	if (gtk_tree_row_reference_valid (reference)) {
		GtkTreePath *path;

		path = gtk_tree_row_reference_get_path (reference);
		have_iter = gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_path_free (path);
	}

	if (!have_iter)
		have_iter = gtk_tree_selection_get_selected (
			selection, NULL, &iter);

	if (!have_iter)
		return NULL;

	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

	extension_name = e_source_selector_get_extension_name (selector);

	if (!e_source_has_extension (source, extension_name)) {
		g_object_unref (source);
		return NULL;
	}

	return source;
}

/**
 * e_source_selector_set_primary_selection:
 * @selector: an #ESourceSelector widget
 * @source: an #ESource to select
 *
 * Highlights @source in @selector.  The highlighted #ESource is called
 * the primary selection.
 *
 * Do not confuse this function with e_source_selector_select_source(),
 * which activates the check box next to an #ESource's display name in
 * @selector.  This function does not alter the check box.
 **/
void
e_source_selector_set_primary_selection (ESourceSelector *selector,
                                         ESource *source)
{
	GHashTable *source_index;
	GtkTreeRowReference *reference;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreePath *child_path;
	GtkTreePath *parent_path;
	const gchar *extension_name;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	tree_view = GTK_TREE_VIEW (selector);
	selection = gtk_tree_view_get_selection (tree_view);

	source_index = selector->priv->source_index;
	reference = g_hash_table_lookup (source_index, source);

	/* XXX Maybe we should return a success/fail boolean? */
	if (!gtk_tree_row_reference_valid (reference))
		return;

	extension_name = e_source_selector_get_extension_name (selector);

	/* Return silently if attempting to select a parent node
	 * lacking the expected extension (e.g. On This Computer). */
	if (!e_source_has_extension (source, extension_name))
		return;

	/* We block the signal because this all needs to be atomic */
	g_signal_handlers_block_matched (
		selection, G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, selection_changed_callback, NULL);
	gtk_tree_selection_unselect_all (selection);
	g_signal_handlers_unblock_matched (
		selection, G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, selection_changed_callback, NULL);

	clear_saved_primary_selection (selector);

	child_path = gtk_tree_row_reference_get_path (reference);

	parent_path = gtk_tree_path_copy (child_path);
	gtk_tree_path_up (parent_path);

	if (gtk_tree_view_row_expanded (tree_view, parent_path)) {
		gtk_tree_selection_select_path (selection, child_path);
	} else {
		selector->priv->saved_primary_selection =
			gtk_tree_row_reference_copy (reference);
		g_signal_emit (selector, signals[PRIMARY_SELECTION_CHANGED], 0);
		g_object_notify (G_OBJECT (selector), "primary-selection");
	}

	gtk_tree_path_free (child_path);
	gtk_tree_path_free (parent_path);
}

/**
 * e_source_selector_ref_source_by_path:
 * @selector: an #ESourceSelector
 * @path: a #GtkTreePath
 *
 * Returns the #ESource object at @path, or %NULL if @path is invalid.
 *
 * The returned #ESource is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: the #ESource object at @path, or %NULL
 *
 * Since: 3.6
 **/
ESource *
e_source_selector_ref_source_by_path (ESourceSelector *selector,
                                      GtkTreePath *path)
{
	ESource *source = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), NULL);
	g_return_val_if_fail (path != NULL, NULL);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));

	if (gtk_tree_model_get_iter (model, &iter, path))
		gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

	return source;
}

/**
 * e_source_selector_queue_write:
 * @selector: an #ESourceSelecetor
 * @source: an #ESource with changes to be written
 *
 * Queues a main loop idle callback to write changes to @source back to
 * the D-Bus registry service.
 *
 * Since: 3.6
 **/
void
e_source_selector_queue_write (ESourceSelector *selector,
                               ESource *source)
{
	GSource *idle_source;
	GHashTable *pending_writes;
	GMainContext *main_context;
	AsyncContext *async_context;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	main_context = selector->priv->main_context;
	pending_writes = selector->priv->pending_writes;

	idle_source = g_hash_table_lookup (pending_writes, source);
	if (idle_source != NULL && !g_source_is_destroyed (idle_source))
		return;

	async_context = g_slice_new0 (AsyncContext);
	async_context->selector = g_object_ref (selector);
	async_context->source = g_object_ref (source);

	/* Set a higher priority so this idle source runs before our
	 * source_selector_cancel_write() signal handler, which will
	 * cancel this idle source.  Cancellation is the right thing
	 * to do when receiving changes from OTHER registry clients,
	 * but we don't want to cancel our own changes.
	 *
	 * XXX This might be an argument for using etags.
	 */
	idle_source = g_idle_source_new ();
	g_hash_table_insert (
		pending_writes,
		g_object_ref (source),
		g_source_ref (idle_source));
	g_source_set_callback (
		idle_source,
		source_selector_write_idle_cb,
		async_context,
		(GDestroyNotify) async_context_free);
	g_source_set_priority (idle_source, G_PRIORITY_HIGH_IDLE);
	g_source_attach (idle_source, main_context);
	g_source_unref (idle_source);
}

