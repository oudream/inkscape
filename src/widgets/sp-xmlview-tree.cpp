// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Specialization of GtkTreeView for the XML tree view
 *
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *
 * Copyright (C) 2002 MenTaLguY
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cstring>

#include "xml/node-event-vector.h"
#include "sp-xmlview-tree.h"

struct NodeData {
	SPXMLViewTree * tree;
	GtkTreeRowReference  *rowref;
	Inkscape::XML::Node * repr;
};

enum { STORE_TEXT_COL = 0, STORE_DATA_COL, STORE_REPR_COL, STORE_N_COLS };

static void sp_xmlview_tree_destroy(GtkWidget * object);

static NodeData * node_data_new (SPXMLViewTree * tree, GtkTreeIter * node, GtkTreeRowReference  *rowref, Inkscape::XML::Node * repr);

static GtkTreeRowReference * add_node (SPXMLViewTree * tree, GtkTreeIter * parent, GtkTreeIter * before, Inkscape::XML::Node * repr);

static void element_child_added (Inkscape::XML::Node * repr, Inkscape::XML::Node * child, Inkscape::XML::Node * ref, gpointer data);
static void element_attr_changed (Inkscape::XML::Node * repr, const gchar * key, const gchar * old_value, const gchar * new_value, bool is_interactive, gpointer data);
static void element_child_removed (Inkscape::XML::Node * repr, Inkscape::XML::Node * child, Inkscape::XML::Node * ref, gpointer data);
static void element_order_changed (Inkscape::XML::Node * repr, Inkscape::XML::Node * child, Inkscape::XML::Node * oldref, Inkscape::XML::Node * newref, gpointer data);
static void element_name_changed (Inkscape::XML::Node* repr, gchar const* oldname, gchar const* newname, gpointer data);
static void element_attr_or_name_change_update(Inkscape::XML::Node* repr, NodeData* data);

static void text_content_changed (Inkscape::XML::Node * repr, const gchar * old_content, const gchar * new_content, gpointer data);
static void comment_content_changed (Inkscape::XML::Node * repr, const gchar * old_content, const gchar * new_content, gpointer data);
static void pi_content_changed (Inkscape::XML::Node * repr, const gchar * old_content, const gchar * new_content, gpointer data);

static gboolean ref_to_sibling (NodeData *node, Inkscape::XML::Node * ref, GtkTreeIter *);
static gboolean repr_to_child (NodeData *node, Inkscape::XML::Node * repr, GtkTreeIter *);
static gboolean tree_model_iter_compare(GtkTreeModel* store, GtkTreeIter * iter1, GtkTreeIter * iter2);
GtkTreeRowReference  *tree_iter_to_ref (SPXMLViewTree * tree, GtkTreeIter* iter);
static gboolean tree_ref_to_iter (SPXMLViewTree * tree, GtkTreeIter* iter, GtkTreeRowReference  *ref);

gboolean search_equal_func (GtkTreeModel *model, gint column, const gchar *key, GtkTreeIter *iter, gpointer search_data);
gboolean foreach_func (GtkTreeModel *model, GtkTreePath  *path, GtkTreeIter  *iter, gpointer user_data);

void on_row_changed(GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data);
void on_drag_data_received(GtkWidget *wgt, GdkDragContext *context, int x, int y, GtkSelectionData *seldata, guint info, guint time, gpointer userdata);
gboolean do_drag_motion(GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, gpointer user_data);

static const Inkscape::XML::NodeEventVector element_repr_events = {
        element_child_added,
        element_child_removed,
        element_attr_changed,
        nullptr, /* content_changed */
        element_order_changed,
        element_name_changed
};

static const Inkscape::XML::NodeEventVector text_repr_events = {
        nullptr, /* child_added */
        nullptr, /* child_removed */
        nullptr, /* attr_changed */
        text_content_changed,
        nullptr  /* order_changed */,
        nullptr  /* element_name_changed */
};

static const Inkscape::XML::NodeEventVector comment_repr_events = {
        nullptr, /* child_added */
        nullptr, /* child_removed */
        nullptr, /* attr_changed */
        comment_content_changed,
        nullptr  /* order_changed */,
        nullptr  /* element_name_changed */
};

static const Inkscape::XML::NodeEventVector pi_repr_events = {
        nullptr, /* child_added */
        nullptr, /* child_removed */
        nullptr, /* attr_changed */
        pi_content_changed,
        nullptr  /* order_changed */,
        nullptr  /* element_name_changed */
};

GtkWidget *sp_xmlview_tree_new(Inkscape::XML::Node * repr, void * /*factory*/, void * /*data*/)
{
    SPXMLViewTree *tree = SP_XMLVIEW_TREE(g_object_new (SP_TYPE_XMLVIEW_TREE, nullptr));

    tree->store = gtk_tree_store_new (STORE_N_COLS, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_POINTER);

    // Detach the model from the view until all the data is loaded
    g_object_ref(tree->store);
    gtk_tree_view_set_model(GTK_TREE_VIEW(tree), nullptr);

    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(tree), FALSE);
    gtk_tree_view_set_reorderable (GTK_TREE_VIEW(tree), TRUE);
    gtk_tree_view_set_enable_search (GTK_TREE_VIEW(tree), TRUE);
    gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW(tree), search_equal_func, nullptr, nullptr);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes ("", renderer, "text", STORE_TEXT_COL, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
    gtk_cell_renderer_set_padding (renderer, 2, 0);
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

    sp_xmlview_tree_set_repr (tree, repr);

    g_signal_connect(G_OBJECT(tree->store), "row-changed", G_CALLBACK(on_row_changed), tree);
    g_signal_connect(GTK_TREE_VIEW(tree), "drag_data_received",  G_CALLBACK(on_drag_data_received), tree);
    g_signal_connect(GTK_TREE_VIEW(tree), "drag-motion",  G_CALLBACK(do_drag_motion), tree);

    return GTK_WIDGET(tree);
}

G_DEFINE_TYPE(SPXMLViewTree, sp_xmlview_tree, GTK_TYPE_TREE_VIEW);

void sp_xmlview_tree_class_init(SPXMLViewTreeClass * klass)
{
    auto widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->destroy = sp_xmlview_tree_destroy;
    
    // Signal for when a tree drag and drop has completed
    g_signal_new (  "tree_move",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_FIRST,
        0,
        nullptr, nullptr,
        g_cclosure_marshal_VOID__UINT,
        G_TYPE_NONE, 1,
        G_TYPE_UINT);
}

void
sp_xmlview_tree_init (SPXMLViewTree * tree)
{
	tree->repr = nullptr;
	tree->blocked = 0;
	tree->dndactive = FALSE;
}

void sp_xmlview_tree_destroy(GtkWidget * object)
{
	SPXMLViewTree * tree = SP_XMLVIEW_TREE (object);

	sp_xmlview_tree_set_repr (tree, nullptr);

	GTK_WIDGET_CLASS(sp_xmlview_tree_parent_class)->destroy (object);
}

/*
 * Add a new row to the tree
 */
GtkTreeRowReference *
add_node (SPXMLViewTree * tree, GtkTreeIter *parent, GtkTreeIter *before, Inkscape::XML::Node * repr)
{
	NodeData * data = nullptr;
	const Inkscape::XML::NodeEventVector * vec;
	static const gchar *default_text[] = { "???" };

	g_assert (tree != nullptr);
	g_assert (repr != nullptr);

    if (before && !gtk_tree_store_iter_is_valid(tree->store, before)) {
        before = nullptr;
    }

	GtkTreeIter iter;
    gtk_tree_store_insert_before (tree->store, &iter, parent, before);

    if (!gtk_tree_store_iter_is_valid(tree->store, &iter)) {
        return nullptr;
    }

    GtkTreeRowReference  *rowref = tree_iter_to_ref (tree, &iter);

    data = node_data_new (tree, &iter, rowref, repr);

    g_assert (data != nullptr);

	gtk_tree_store_set (tree->store, &iter, STORE_TEXT_COL, default_text, STORE_DATA_COL, data, STORE_REPR_COL, repr, -1);

	if ( repr->type() == Inkscape::XML::TEXT_NODE ) {
		vec = &text_repr_events;
	} else if ( repr->type() == Inkscape::XML::COMMENT_NODE ) {
		vec = &comment_repr_events;
	} else if ( repr->type() == Inkscape::XML::PI_NODE ) {
		vec = &pi_repr_events;
	} else if ( repr->type() == Inkscape::XML::ELEMENT_NODE ) {
		vec = &element_repr_events;
	} else {
		vec = nullptr;
	}

	if (vec) {
		/* cheat a little to get the id updated properly */
		if (repr->type() == Inkscape::XML::ELEMENT_NODE) {
			element_attr_changed (repr, "id", nullptr, nullptr, false, data);
		}
		sp_repr_add_listener (repr, vec, data);
		sp_repr_synthesize_events (repr, vec, data);
	}

	return rowref;
}

NodeData *node_data_new(SPXMLViewTree * tree, GtkTreeIter * /*node*/, GtkTreeRowReference  *rowref, Inkscape::XML::Node *repr)
{
    NodeData *data = g_new(NodeData, 1);
    data->tree = tree;
    data->rowref = rowref;
    data->repr = repr;
    Inkscape::GC::anchor(repr);
    return data;
}

void element_child_added (Inkscape::XML::Node * /*repr*/, Inkscape::XML::Node * child, Inkscape::XML::Node * ref, gpointer ptr)
{
    NodeData *data = static_cast<NodeData *>(ptr);
    GtkTreeIter before;

    if (data->tree->blocked) return;

    if (!ref_to_sibling (data, ref, &before)) {
        return;
    }

    GtkTreeIter data_iter;
    tree_ref_to_iter(data->tree, &data_iter,  data->rowref);
    add_node (data->tree, &data_iter, &before, child);
}

void element_attr_changed(
        Inkscape::XML::Node* repr, gchar const* key,
        gchar const* /*old_value*/, gchar const* /*new_value*/, bool /*is_interactive*/,
        gpointer ptr)
{
    if (0 != strcmp (key, "id") && 0 != strcmp (key, "inkscape:label"))
        return;
    element_attr_or_name_change_update(repr, static_cast<NodeData*>(ptr));
}

void element_name_changed(
        Inkscape::XML::Node* repr,
        gchar const* /*oldname*/, gchar const* /*newname*/, gpointer ptr)
{
    element_attr_or_name_change_update(repr, static_cast<NodeData*>(ptr));
}

void element_attr_or_name_change_update(Inkscape::XML::Node* repr, NodeData* data)
{
    if (data->tree->blocked) {
        return;
    }

    gchar const* node_name = repr->name();
    gchar const* id_value = repr->attribute("id");
    gchar const* label_value = repr->attribute("inkscape:label");
    gchar* display_text;

    if (id_value && label_value) {
        display_text = g_strdup_printf ("<%s id=\"%s\" inkscape:label=\"%s\">", node_name, id_value, label_value);
    } else if (id_value) {
        display_text = g_strdup_printf ("<%s id=\"%s\">", node_name, id_value);
    } else if (label_value) {
        display_text = g_strdup_printf ("<%s inkscape:label=\"%s\">", node_name, label_value);
    } else {
        display_text = g_strdup_printf ("<%s>", node_name);
    }

    GtkTreeIter iter;
    if (tree_ref_to_iter(data->tree, &iter,  data->rowref)) {
        gtk_tree_store_set (GTK_TREE_STORE(data->tree->store), &iter, STORE_TEXT_COL, display_text, -1);
    }

    g_free(display_text);
}

void element_child_removed(Inkscape::XML::Node * /*repr*/, Inkscape::XML::Node * child, Inkscape::XML::Node * /*ref*/, gpointer ptr)
{
    NodeData *data = static_cast<NodeData *>(ptr);

    if (data->tree->blocked) return;

    GtkTreeIter iter;
    if (repr_to_child (data, child, &iter)) {
        gtk_tree_store_remove (GTK_TREE_STORE(data->tree->store), &iter);
    }
}

void element_order_changed(Inkscape::XML::Node * /*repr*/, Inkscape::XML::Node * child, Inkscape::XML::Node * /*oldref*/, Inkscape::XML::Node * newref, gpointer ptr)
{
    NodeData *data = static_cast<NodeData *>(ptr);
    GtkTreeIter before, node;

    if (data->tree->blocked) return;

    ref_to_sibling (data, newref, &before);
    repr_to_child (data, child, &node);

    if (gtk_tree_store_iter_is_valid(data->tree->store, &before)) {
        gtk_tree_store_move_before (data->tree->store, &node, &before);
    } else {
        repr_to_child (data, newref, &before);
        gtk_tree_store_move_after (data->tree->store, &node, &before);
    }
}

Glib::ustring sp_remove_newlines_and_tabs(Glib::ustring val)
{
    int pos = 0;
    Glib::ustring newlinesign = "␤";
    Glib::ustring tabsign = "⇥";
    while ((pos = val.find("\r\n")) != std::string::npos) {
        val.erase(pos, 2);
        val.insert(pos, newlinesign);
    }
    pos = 0;
    while ((pos = val.find('\n')) != std::string::npos) {
        val.erase(pos, 1);
        val.insert(pos, newlinesign);
    }
    pos = 0;
    while ((pos = val.find('\t')) != std::string::npos) {
        val.erase(pos, 1);
        val.insert(pos, tabsign);
    }
    return val;
}

void text_content_changed(Inkscape::XML::Node * /*repr*/, const gchar * /*old_content*/, const gchar * new_content, gpointer ptr)
{
    NodeData *data = static_cast<NodeData *>(ptr);

    if (data->tree->blocked) return;

    gchar *label = g_strdup_printf ("\"%s\"", new_content);
    Glib::ustring nolinecontent = label;
    nolinecontent = sp_remove_newlines_and_tabs(nolinecontent);

    GtkTreeIter iter;
    if (tree_ref_to_iter(data->tree, &iter,  data->rowref)) {
        gtk_tree_store_set(GTK_TREE_STORE(data->tree->store), &iter, STORE_TEXT_COL, nolinecontent.c_str(), -1);
    }

    g_free (label);
}

void comment_content_changed(Inkscape::XML::Node * /*repr*/, const gchar * /*old_content*/, const gchar *new_content, gpointer ptr)
{
    NodeData *data = static_cast<NodeData*>(ptr);

    if (data->tree->blocked) return;

    gchar *label = g_strdup_printf ("<!--%s-->", new_content);
    Glib::ustring nolinecontent = label;
    nolinecontent = sp_remove_newlines_and_tabs(nolinecontent);

    GtkTreeIter iter;
    if (tree_ref_to_iter(data->tree, &iter,  data->rowref)) {
        gtk_tree_store_set(GTK_TREE_STORE(data->tree->store), &iter, STORE_TEXT_COL, nolinecontent.c_str(), -1);
    }
    g_free (label);
}

void pi_content_changed(Inkscape::XML::Node *repr, const gchar * /*old_content*/, const gchar *new_content, gpointer ptr)
{
    NodeData *data = static_cast<NodeData *>(ptr);

    if (data->tree->blocked) return;

    gchar *label = g_strdup_printf ("<?%s %s?>", repr->name(), new_content);
    Glib::ustring nolinecontent = label;
    nolinecontent = sp_remove_newlines_and_tabs(nolinecontent);

    GtkTreeIter iter;
    if (tree_ref_to_iter(data->tree, &iter,  data->rowref)) {
        gtk_tree_store_set(GTK_TREE_STORE(data->tree->store), &iter, STORE_TEXT_COL, nolinecontent.c_str(), -1);
    }
    g_free (label);
}

/*
 * Save the source path on drag start, will need it in on_row_changed() when moving a row
 */
void on_drag_data_received(GtkWidget * /*wgt*/, GdkDragContext * /*context*/, int /*x*/, int /*y*/,
			   GtkSelectionData * /*seldata*/, guint /*info*/, guint /*time*/,
			   gpointer userdata)
{
    SPXMLViewTree *tree = static_cast<SPXMLViewTree *>(userdata);
    if (!tree) {
        return;
    }

    GtkTreeModel *model = nullptr;
    GtkTreeIter iter;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {

        tree->dndactive = TRUE;

        GtkTreeIter parent_iter;
        GtkTreeRowReference  *parent_ref = nullptr;
        if (gtk_tree_model_iter_parent(model, &parent_iter, &iter)) {
            parent_ref = tree_iter_to_ref (tree, &parent_iter);
        }

        g_object_set_data(G_OBJECT(tree), "drag-src-path", parent_ref);
    }
}

/*
 * Main drag & drop function
 * Get the old and new paths, and change the Inkscape::XML::Node repr's
 */
void on_row_changed(GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
    SPXMLViewTree *tree = SP_XMLVIEW_TREE(user_data);

    if (!tree->dndactive) {
        return;
    }
    tree->dndactive = FALSE;

    Inkscape::XML::Node *repr = sp_xmlview_tree_node_get_repr(tree_model, iter);
    GtkTreeIter new_parent;
    if (!gtk_tree_model_iter_parent(tree_model, &new_parent, iter)) {
        //No parent of drop location
        return;
    }

    GtkTreeRowReference  *old_parent_ref = static_cast<GtkTreeRowReference *>(g_object_get_data (G_OBJECT (tree), "drag-src-path"));
    if (!old_parent_ref) {
        //No drag source location
        g_signal_emit_by_name(G_OBJECT (tree), "tree_move", GUINT_TO_POINTER(0) );
        return;
    }

    GtkTreeIter old_parent;
    if (!tree_ref_to_iter(tree, &old_parent,  old_parent_ref)) {
        //Drag source parent is not valid
        g_signal_emit_by_name(G_OBJECT (tree), "tree_move", GUINT_TO_POINTER(0) );
        return;
    }

    // Find the sibling node before iter
    GtkTreeIter before_iter;
    Inkscape::XML::Node *before_repr = nullptr;
    GtkTreeIter tmp_iter;

    gboolean valid = gtk_tree_model_iter_children(tree_model, &tmp_iter, &new_parent);
    while (valid && tree_model_iter_compare (tree_model, &tmp_iter, iter)) {
        before_iter = tmp_iter;
        valid = gtk_tree_model_iter_next(tree_model, &tmp_iter);
    }

    // If before_iter is invalid, before_repr stays as NULL which is ok
    if (gtk_tree_store_iter_is_valid(GTK_TREE_STORE(tree_model), &before_iter)) {
        gtk_tree_model_get (tree_model, &before_iter, STORE_REPR_COL, &before_repr, -1);
    }

    // Drop onto oneself causes assert in changeOrder() below, ignore
    if (repr == before_repr)
        return;

    SP_XMLVIEW_TREE (tree)->blocked++;
    if (!tree_model_iter_compare (tree_model, &new_parent, &old_parent)) {
        sp_xmlview_tree_node_get_repr (tree_model, &old_parent)->changeOrder(repr, before_repr);
    } else {
        sp_xmlview_tree_node_get_repr (tree_model, &old_parent)->removeChild(repr);
        sp_xmlview_tree_node_get_repr (tree_model, &new_parent)->addChild(repr, before_repr);
    }
    SP_XMLVIEW_TREE (tree)->blocked--;

    // Reselect the dragged row
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    gtk_tree_view_expand_to_path (GTK_TREE_VIEW(tree), path);
    //gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW(tree), path, NULL, true, 0.66, 0.0);
    gtk_tree_selection_select_iter(selection, iter);

    // Signal that a drag and drop has completed successfully
    g_signal_emit_by_name(G_OBJECT (tree), "tree_move", GUINT_TO_POINTER(1) );
}

/*
 * Set iter to ref or node data's child with the same repr or first child
 */
gboolean ref_to_sibling (NodeData *data, Inkscape::XML::Node *repr, GtkTreeIter *iter)
{
	if (repr) {
		if (!repr_to_child (data, repr, iter)) {
            return false;
        }
        gtk_tree_model_iter_next (GTK_TREE_MODEL(data->tree->store), iter);
	} else {
	    GtkTreeIter data_iter;
	    if (!tree_ref_to_iter(data->tree, &data_iter,  data->rowref)) {
	        return false;
	    }
	    gtk_tree_model_iter_children(GTK_TREE_MODEL(data->tree->store), iter, &data_iter);
	}
	return true;
}

/*
 * Set iter to the node data's child with the same repr
 */
gboolean repr_to_child (NodeData *data, Inkscape::XML::Node * repr, GtkTreeIter *iter)
{
    GtkTreeIter data_iter;
    tree_ref_to_iter(data->tree, &data_iter,  data->rowref);
    GtkTreeModel *model = GTK_TREE_MODEL(data->tree->store);
    gboolean valid = false;

    if (!gtk_tree_store_iter_is_valid(data->tree->store, &data_iter)) {
        return false;
    }

    /*
     * The node we are looking for is likely to be the last one, so check it first.
     */
    gint n_children = gtk_tree_model_iter_n_children (model, &data_iter);
    if (n_children > 1) {
        valid = gtk_tree_model_iter_nth_child (model, iter, &data_iter, n_children-1);
        if (valid && sp_xmlview_tree_node_get_repr (model, iter) == repr) {
            //g_message("repr_to_child hit %d", n_children);
            return valid;
        }
    }

    valid = gtk_tree_model_iter_children(model, iter, &data_iter);
    while (valid && sp_xmlview_tree_node_get_repr (model, iter) != repr) {
        valid = gtk_tree_model_iter_next(model, iter);
	}

    return valid;
}

/*
 * Get a matching GtkTreeRowReference for a GtkTreeIter
 */
GtkTreeRowReference  *tree_iter_to_ref (SPXMLViewTree * tree, GtkTreeIter* iter)
{
    GtkTreePath* path = gtk_tree_model_get_path(GTK_TREE_MODEL(tree->store), iter);
    GtkTreeRowReference  *ref = gtk_tree_row_reference_new(GTK_TREE_MODEL(tree->store), path);
    gtk_tree_path_free(path);

    return ref;
}

/*
 * Get a matching GtkTreeIter for a GtkTreeRowReference
 */
gboolean tree_ref_to_iter (SPXMLViewTree * tree, GtkTreeIter* iter, GtkTreeRowReference  *ref)
{
    GtkTreePath* path = gtk_tree_row_reference_get_path(ref);
    if (!path) {
        return false;
    }
    gtk_tree_model_get_iter(GTK_TREE_MODEL(tree->store), iter, path);

    return gtk_tree_store_iter_is_valid(GTK_TREE_STORE(tree->store), iter);
}

/*
 * Compare 2 GtkTreeIter and return 0 if they are equal
 */
gboolean tree_model_iter_compare(GtkTreeModel* store, GtkTreeIter * iter1, GtkTreeIter * iter2)
{
    GtkTreePath *path1 = gtk_tree_model_get_path(store, iter1);
    GtkTreePath *path2 = gtk_tree_model_get_path(store, iter2);

    gboolean result = gtk_tree_path_compare( path1, path2);

    gtk_tree_path_free(path1);
    gtk_tree_path_free(path2);

    return result;

}

/*
 * Disable drag and drop target on : root node and non-element nodes
 */
gboolean do_drag_motion(GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, gpointer user_data)
{
    GtkTreePath *path = nullptr;
    GtkTreeViewDropPosition pos;
    gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW(widget), x, y, &path, &pos);

    int action = 0;

    if (path) {
        SPXMLViewTree *tree = SP_XMLVIEW_TREE(user_data);
        GtkTreeIter iter;
        gtk_tree_model_get_iter(GTK_TREE_MODEL(tree->store), &iter, path);

        // 1. only xml elements can be dragged
        if (sp_xmlview_tree_node_get_repr (GTK_TREE_MODEL(tree->store), &iter)->type() == Inkscape::XML::ELEMENT_NODE) {
            // 2. new roots cannot be created eg. by dragging a node off into space
            if (gtk_tree_path_get_depth(path) > 0) {
                // 3. elements must be at least children of the root <svg:svg> element
                if (gtk_tree_path_up(path) && gtk_tree_path_up(path)) {
                    action = GDK_ACTION_MOVE;
                }
            }
        }
    }

    gtk_tree_path_free(path);
    gdk_drag_status (context, (GdkDragAction)action, time);

    return (action == 0);
}

/*
 * Set the tree selection and scroll to the row with the given repr
 */
void
sp_xmlview_tree_set_repr (SPXMLViewTree * tree, Inkscape::XML::Node * repr)
{
    if ( tree->repr == repr ) return;
    if (tree->repr) {
        /*
         *  Would like to simple call gtk_tree_store_clear here,
         *  but it is extremely slow on large data sets.
         *  Instead just unref the old and create a new store.
         */
        //gtk_tree_store_clear(tree->store);
        gtk_tree_view_set_model(GTK_TREE_VIEW(tree), nullptr);
        g_object_unref(tree->store);
        tree->store = gtk_tree_store_new (STORE_N_COLS, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_POINTER);
        gtk_tree_view_set_model (GTK_TREE_VIEW(tree), GTK_TREE_MODEL(tree->store));

        Inkscape::GC::release(tree->repr);
    }
    tree->repr = repr;
    if (repr) {
        GtkTreeRowReference * rowref;
        Inkscape::GC::anchor(repr);
        rowref = add_node (tree, nullptr, nullptr, repr);

        // Set the tree model here, after all data is inserted
        gtk_tree_view_set_model (GTK_TREE_VIEW(tree), GTK_TREE_MODEL(tree->store));
        g_object_unref(tree->store);

        GtkTreePath *path = gtk_tree_row_reference_get_path(rowref);
        gtk_tree_view_expand_to_path (GTK_TREE_VIEW(tree), path);
        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW(tree), path, nullptr, true, 0.5, 0.0);
        gtk_tree_path_free(path);
    }
}

/*
 * Return the repr at a given GtkTreeIter position
 */
Inkscape::XML::Node *
sp_xmlview_tree_node_get_repr (GtkTreeModel *model, GtkTreeIter * iter)
{
    Inkscape::XML::Node *repr;
    gtk_tree_model_get (model, iter, STORE_REPR_COL, &repr, -1);

    return repr;
}

/*
 * Find a GtkTreeIter position in the tree by repr
 */
gboolean
sp_xmlview_tree_get_repr_node (SPXMLViewTree * tree, Inkscape::XML::Node * repr, GtkTreeIter *iter)
{
    /*
     * Use a NodeData here to pass in the repr to the foreach function and store a rowref if found,
     * if found we can return the iter
     */
    NodeData anode;
    anode.tree = tree;
    anode.repr = repr;
    anode.rowref = nullptr;

    gtk_tree_model_foreach(GTK_TREE_MODEL(tree->store), foreach_func, &anode);
    if (anode.rowref != nullptr) {
        tree_ref_to_iter(tree, iter, anode.rowref);
        return TRUE;
    }

    return FALSE;
}

gboolean foreach_func(GtkTreeModel *model, GtkTreePath * /*path*/, GtkTreeIter *iter, gpointer user_data)
{
    NodeData *anode = static_cast<NodeData *>(user_data);
    Inkscape::XML::Node *iter_repr;
    gtk_tree_model_get (model, iter, STORE_REPR_COL, &iter_repr, -1);
    if (anode->repr == iter_repr) {
        GtkTreeRowReference  *rowref = tree_iter_to_ref (anode->tree, iter);
        anode->rowref = rowref;
        return TRUE;
    }

    return FALSE;
}

/*
 * Callback function for string searches in the tree
 * Return a match on any substring
 */
gboolean search_equal_func(GtkTreeModel *model, gint /*column*/, const gchar *key, GtkTreeIter *iter, gpointer /*search_data*/)
{
    gchar *text = nullptr;
    gtk_tree_model_get(model, iter, STORE_TEXT_COL, &text, -1);

    gboolean match = (strstr(text, key) != nullptr);

    g_free(text);

    return !match;
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
