// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_PATH_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_PATH_H

/*
 * Inkscape::LivePathEffectParameters
 *
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glib.h>
#include <2geom/path.h>

#include "live_effects/parameter/parameter.h"
#include "live_effects/parameter/path-reference.h"
#include <cstddef>
#include <sigc++/sigc++.h>

namespace Inkscape {

namespace LivePathEffect {

class PathParam : public Parameter {
public:
    PathParam ( const Glib::ustring& label,
                const Glib::ustring& tip,
                const Glib::ustring& key,
                Inkscape::UI::Widget::Registry* wr,
                Effect* effect,
                const gchar * default_value = "M0,0 L1,1");
    ~PathParam() override;

    Geom::PathVector const & get_pathvector() const;
    Geom::Piecewise<Geom::D2<Geom::SBasis> > const & get_pwd2();

    Gtk::Widget * param_newWidget() override;

    bool param_readSVGValue(const gchar * strvalue) override;
    gchar * param_getSVGValue() const override;
    gchar * param_getDefaultSVGValue() const override;

    void param_set_default() override;
    void param_update_default(const gchar * default_value) override;
    void param_set_and_write_default();
    void set_new_value (Geom::PathVector const &newpath, bool write_to_svg);
    void set_new_value (Geom::Piecewise<Geom::D2<Geom::SBasis> > const &newpath, bool write_to_svg);
    void set_buttons(bool edit_button, bool copy_button, bool paste_button, bool link_button);
    void param_editOncanvas(SPItem * item, SPDesktop * dt) override;
    void param_setup_nodepath(Inkscape::NodePath::Path *np) override;
    void addCanvasIndicators(SPLPEItem const* lpeitem, std::vector<Geom::PathVector> &hp_vec) override;

    void param_transform_multiply(Geom::Affine const& /*postmul*/, bool /*set*/) override;
    void setFromOriginalD(bool from_original_d){ _from_original_d = from_original_d; };

    sigc::signal <void> signal_path_pasted;
    sigc::signal <void> signal_path_changed;
    bool changed; /* this gets set whenever the path is changed (this is set to true, and then the signal_path_changed signal is emitted).
                   * the user must set it back to false if she wants to use it sensibly */

    void paste_param_path(const char *svgd);
    void on_paste_button_click();

protected:
    Geom::PathVector _pathvector;   // this is primary data storage, since it is closest to SVG.

    Geom::Piecewise<Geom::D2<Geom::SBasis> > _pwd2; // secondary, hence the bool must_recalculate_pwd2
    bool must_recalculate_pwd2; // set when _pathvector was updated, but _pwd2 not
    void ensure_pwd2();  // ensures _pwd2 is up to date

    gchar * href;     // contains link to other object, e.g. "#path2428", NULL if PathParam contains pathdata itself
    PathReference ref;
    sigc::connection ref_changed_connection;
    sigc::connection linked_delete_connection;
    sigc::connection linked_modified_connection;
    sigc::connection linked_transformed_connection;
    void ref_changed(SPObject *old_ref, SPObject *new_ref);
    void remove_link();
    void start_listening(SPObject * to);
    void quit_listening();
    void linked_delete(SPObject *deleted);
    void linked_modified(SPObject *linked_obj, guint flags);
    void linked_transformed(Geom::Affine const *rel_transf, SPItem *moved_item);
    virtual void linked_modified_callback(SPObject *linked_obj, guint flags);
    virtual void linked_transformed_callback(Geom::Affine const * /*rel_transf*/, SPItem * /*moved_item*/) {};

    void on_edit_button_click();
    void on_copy_button_click();
    void on_link_button_click();

    void emit_changed();

    gchar * defvalue;

private:
    bool _from_original_d;
    bool _edit_button;
    bool _copy_button;
    bool _paste_button;
    bool _link_button;
    PathParam(const PathParam&) = delete;
    PathParam& operator=(const PathParam&) = delete;
};


} //namespace LivePathEffect

} //namespace Inkscape

#endif
