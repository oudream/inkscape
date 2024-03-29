// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Filter Effects dialog
 */
/* Authors:
 *   Nicholas Bishop <nicholasbishop@gmail.com>
 *   Rodrigo Kumpera <kumpera@gmail.com>
 *   insaner
 *
 * Copyright (C) 2007 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DIALOG_FILTER_EFFECTS_H
#define INKSCAPE_UI_DIALOG_FILTER_EFFECTS_H

#include <memory>

#include <gtkmm/notebook.h>

#include <gtkmm/paned.h>
#include <gtkmm/scrolledwindow.h>

#include "attributes.h"

#include "ui/dialog/desktop-tracker.h"
#include "ui/widget/combo-enums.h"
#include "ui/widget/panel.h"
#include "ui/widget/spin-scale.h"

#include "xml/helper-observer.h"

class SPFilter;

namespace Inkscape {
namespace UI {
namespace Dialog {

class EntryAttr;
//class SpinButtonAttr;
class DualSpinButton;
class MultiSpinButton;
class FilterEffectsDialog : public UI::Widget::Panel {
public:

    FilterEffectsDialog();
    ~FilterEffectsDialog() override;

    static FilterEffectsDialog &getInstance()
    { return *new FilterEffectsDialog(); }

    void set_attrs_locked(const bool);
protected:
    void show_all_vfunc() override;
private:

    class FilterModifier : public Gtk::VBox
    {
    public:
        FilterModifier(FilterEffectsDialog&);
        ~FilterModifier() override;

        SPFilter* get_selected_filter();
        void select_filter(const SPFilter*);

        sigc::signal<void>& signal_filter_changed()
        {
            return _signal_filter_changed;
        }
    private:
        class Columns : public Gtk::TreeModel::ColumnRecord
        {
        public:
            Columns()
            {
                add(filter);
                add(label);
                add(sel);
                add(count);
            }

            Gtk::TreeModelColumn<SPFilter*> filter;
            Gtk::TreeModelColumn<Glib::ustring> label;
            Gtk::TreeModelColumn<int> sel;
            Gtk::TreeModelColumn<int> count;
        };

        void setTargetDesktop(SPDesktop *desktop);
       
        void on_document_replaced(SPDesktop *desktop, SPDocument *document);
        void on_change_selection();
        void on_modified_selection( guint flags );
        
        void update_selection(Selection *);
        void on_filter_selection_changed();

        void on_name_edited(const Glib::ustring&, const Glib::ustring&);
        bool on_filter_move(const Glib::RefPtr<Gdk::DragContext>& /*context*/, int x, int y, guint /*time*/);
        void on_selection_toggled(const Glib::ustring&);

        void update_counts();
        void update_filters();
        void filter_list_button_release(GdkEventButton*);
        void add_filter();
        void remove_filter();
        void duplicate_filter();
        void rename_filter();

        /**
         * Stores the current desktop.
         */
        SPDesktop *_desktop;

        /**
         * Auxiliary widget to keep track of desktop changes for the floating dialog.
         */
        DesktopTracker _deskTrack;

        /**
         * Link to callback function for a change in desktop (window).
         */
        sigc::connection desktopChangeConn;
        sigc::connection _selectChangedConn;
        sigc::connection _selectModifiedConn;
        sigc::connection _doc_replaced;
        sigc::connection _resource_changed;

        FilterEffectsDialog& _dialog;
        Gtk::TreeView _list;
        Glib::RefPtr<Gtk::ListStore> _model;
        Columns _columns;
        Gtk::CellRendererToggle _cell_toggle;
        Gtk::Button _add;
        Gtk::Menu   *_menu;
        sigc::signal<void> _signal_filter_changed;
        std::unique_ptr<Inkscape::XML::SignalObserver> _observer;
    };

    class PrimitiveColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        PrimitiveColumns()
        {
            add(primitive);
            add(type_id);
            add(type);
            add(id);
        }

        Gtk::TreeModelColumn<SPFilterPrimitive*> primitive;
        Gtk::TreeModelColumn<Inkscape::Filters::FilterPrimitiveType> type_id;
        Gtk::TreeModelColumn<Glib::ustring> type;
        Gtk::TreeModelColumn<Glib::ustring> id;
    };

    class CellRendererConnection : public Gtk::CellRenderer
    {
    public:
        CellRendererConnection();
        Glib::PropertyProxy<void*> property_primitive();

        static const int size = 24;
        
    protected:
        void get_preferred_width_vfunc(Gtk::Widget& widget,
                                               int& minimum_width,
                                               int& natural_width) const override;
        
        void get_preferred_width_for_height_vfunc(Gtk::Widget& widget,
                                                          int height,
                                                          int& minimum_width,
                                                          int& natural_width) const override;

        void get_preferred_height_vfunc(Gtk::Widget& widget,
                                                int& minimum_height,
                                                int& natural_height) const override;
        
        void get_preferred_height_for_width_vfunc(Gtk::Widget& widget,
                                                          int width,
                                                          int& minimum_height,
                                                          int& natural_height) const override;
    private:
        // void* should be SPFilterPrimitive*, some weirdness with properties prevents this
        Glib::Property<void*> _primitive;
        int _text_width;
    };

    class PrimitiveList : public Gtk::TreeView
    {
    public:
        PrimitiveList(FilterEffectsDialog&);

        sigc::signal<void>& signal_primitive_changed();

        void update();
        void set_menu(Gtk::Widget      &parent,
                      sigc::slot<void>  dup,
                      sigc::slot<void>  rem);

        SPFilterPrimitive* get_selected();
        void select(SPFilterPrimitive *prim);
        void remove_selected();

        int primitive_count() const;
        int get_input_type_width() const;

    protected:
        bool on_draw_signal(const Cairo::RefPtr<Cairo::Context> &cr);


        bool on_button_press_event(GdkEventButton*) override;
        bool on_motion_notify_event(GdkEventMotion*) override;
        bool on_button_release_event(GdkEventButton*) override;
        void on_drag_end(const Glib::RefPtr<Gdk::DragContext>&) override;
    private:
        void init_text();

	void draw_connection_node(const Cairo::RefPtr<Cairo::Context>& cr,
                                  const std::vector<Gdk::Point>& points,
				  const bool fill);

	bool do_connection_node(const Gtk::TreeIter& row, const int input, std::vector<Gdk::Point>& points,
                                const int ix, const int iy);

        const Gtk::TreeIter find_result(const Gtk::TreeIter& start, const int attr, int& src_id);
        int find_index(const Gtk::TreeIter& target);
        void draw_connection(const Cairo::RefPtr<Cairo::Context>& cr,
                             const Gtk::TreeIter&, const int attr, const int text_start_x,
                             const int x1, const int y1, const int row_count);
        void sanitize_connections(const Gtk::TreeIter& prim_iter);
        void on_primitive_selection_changed();
        bool on_scroll_timeout();

        FilterEffectsDialog& _dialog;
        Glib::RefPtr<Gtk::ListStore> _model;
        PrimitiveColumns _columns;
        CellRendererConnection _connection_cell;
        Gtk::Menu *_primitive_menu;
        Glib::RefPtr<Pango::Layout> _vertical_layout;
        int _in_drag;
        SPFilterPrimitive* _drag_prim;
        sigc::signal<void> _signal_primitive_changed;
        sigc::connection _scroll_connection;
        int _autoscroll_y;
        int _autoscroll_x;
        std::unique_ptr<Inkscape::XML::SignalObserver> _observer;
        int _input_type_width;
        int _input_type_height;
    };

    void init_settings_widgets();

    // Handlers
    void add_primitive();
    void remove_primitive();
    void duplicate_primitive();
    void convolve_order_changed();
    void image_x_changed();
    void image_y_changed();

    void set_attr_direct(const UI::Widget::AttrWidget*);
    void set_child_attr_direct(const UI::Widget::AttrWidget*);
    void set_filternode_attr(const UI::Widget::AttrWidget*);
    void set_attr(SPObject*, const SPAttributeEnum, const gchar* val);
    void update_settings_view();
    void update_filter_general_settings_view();
    void update_settings_sensitivity();
    void update_color_matrix();
    void update_primitive_infobox();

    // Primitives Info Box  
    Gtk::Label _infobox_desc;
    Gtk::Image _infobox_icon;
    Gtk::ScrolledWindow* _sw_infobox;

    // View/add primitives
    Gtk::Paned* _primitive_box;
    
    UI::Widget::ComboBoxEnum<Inkscape::Filters::FilterPrimitiveType> _add_primitive_type;
    Gtk::Button _add_primitive;

    // Bottom pane (filter effect primitive settings)
    Gtk::Notebook _settings_tabs;
    Gtk::VBox _settings_tab2;
    Gtk::VBox _settings_tab1;
    Gtk::Label _empty_settings;
    Gtk::Label _no_filter_selected;
    bool _settings_initialized;

    class Settings;
    class MatrixAttr;
    class ColorMatrixValues;
    class ComponentTransferValues;
    class LightSourceControl;
    Settings* _settings;
    Settings* _filter_general_settings;

    // Color Matrix
    ColorMatrixValues* _color_matrix_values;

    // Component Transfer
    ComponentTransferValues* _component_transfer_values;

    // Convolve Matrix
    MatrixAttr* _convolve_matrix;
    DualSpinButton* _convolve_order;
    MultiSpinButton* _convolve_target;

    // Image
    EntryAttr* _image_x;
    EntryAttr* _image_y;
    
    // For controlling setting sensitivity
    Gtk::Widget* _k1, *_k2, *_k3, *_k4;

    // To prevent unwanted signals
    bool _locked;
    bool _attr_lock;

    // These go last since they depend on the prior initialization of
    // other FilterEffectsDialog members
    FilterModifier _filter_modifier;
    PrimitiveList _primitive_list;

    FilterEffectsDialog(FilterEffectsDialog const &d);
    FilterEffectsDialog& operator=(FilterEffectsDialog const &d);
};

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_DIALOG_FILTER_EFFECTS_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
