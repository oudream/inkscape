// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_ORIGINALITEMARRAY_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_ORIGINALITEMARRAY_H

/*
 * Inkscape::LivePathEffectParameters
 *
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <vector>

#include <gtkmm/box.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/scrolledwindow.h>

#include "live_effects/parameter/parameter.h"
#include "live_effects/parameter/item-reference.h"

#include "svg/svg.h"
#include "svg/stringstream.h"
#include "item-reference.h"

class SPObject;

namespace Inkscape {

namespace LivePathEffect {

class ItemAndActive {
public:
    ItemAndActive(SPObject *owner)
    : href(nullptr),
    ref(owner),
    actived(true)
    {
        
    }
    gchar *href;
    URIReference ref;
    bool actived;
    
    sigc::connection linked_changed_connection;
    sigc::connection linked_delete_connection;
    sigc::connection linked_modified_connection;
    sigc::connection linked_transformed_connection;
};
    
class OriginalItemArrayParam : public Parameter {
public:
    class ModelColumns;
    
    OriginalItemArrayParam( const Glib::ustring& label,
                const Glib::ustring& tip,
                const Glib::ustring& key,
                Inkscape::UI::Widget::Registry* wr,
                Effect* effect);

    ~OriginalItemArrayParam() override;

    Gtk::Widget * param_newWidget() override;
    bool param_readSVGValue(const gchar * strvalue) override;
    gchar * param_getSVGValue() const override;
    gchar * param_getDefaultSVGValue() const override;
    void param_set_default() override;
    void param_update_default(const gchar * default_value) override{};
    /** Disable the canvas indicators of parent class by overriding this method */
    void param_editOncanvas(SPItem * /*item*/, SPDesktop * /*dt*/) override {};
    /** Disable the canvas indicators of parent class by overriding this method */
    void addCanvasIndicators(SPLPEItem const* /*lpeitem*/, std::vector<Geom::PathVector> & /*hp_vec*/) override {};

    std::vector<ItemAndActive*> _vector;
    
protected:
    bool _updateLink(const Gtk::TreeIter& iter, ItemAndActive* pd);
    bool _selectIndex(const Gtk::TreeIter& iter, int* i);
    void unlink(ItemAndActive* to);
    void remove_link(ItemAndActive* to);
    void setItem(SPObject *linked_obj, guint flags, ItemAndActive* to);
    
    void linked_changed(SPObject *old_obj, SPObject *new_obj, ItemAndActive* to);
    void linked_modified(SPObject *linked_obj, guint flags, ItemAndActive* to);
    void linked_transformed(Geom::Affine const *, SPItem *, ItemAndActive*) {}
    void linked_delete(SPObject *deleted, ItemAndActive* to);
    
    ModelColumns *_model;
    Glib::RefPtr<Gtk::TreeStore> _store;
    Gtk::TreeView _tree;
    Gtk::CellRendererText *_text_renderer;
    Gtk::CellRendererToggle *_toggle_active;
    Gtk::TreeView::Column *_name_column;
    Gtk::ScrolledWindow _scroller;
    
    void on_link_button_click();
    void on_remove_button_click();
    void on_up_button_click();
    void on_down_button_click();
    void on_active_toggled(const Glib::ustring& item);
    
private:
    void update();
    OriginalItemArrayParam(const OriginalItemArrayParam&);
    OriginalItemArrayParam& operator=(const OriginalItemArrayParam&);
};

} //namespace LivePathEffect

} //namespace Inkscape

#endif

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
