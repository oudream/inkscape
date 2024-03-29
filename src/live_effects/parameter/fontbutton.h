// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_FONT_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_FONT_H

/*
 * Inkscape::LivePathEffectParameters
 *
 * Authors:
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include <glib.h>
#include "live_effects/parameter/parameter.h"

namespace Inkscape {

namespace LivePathEffect {

class FontButtonParam : public Parameter {
public:
    FontButtonParam( const Glib::ustring& label,
               const Glib::ustring& tip,
               const Glib::ustring& key,
               Inkscape::UI::Widget::Registry* wr,
               Effect* effect,
               const Glib::ustring default_value = "Sans 10");
    ~FontButtonParam() override = default;

    Gtk::Widget * param_newWidget() override;
    bool param_readSVGValue(const gchar * strvalue) override;
    void param_update_default(const gchar * default_value) override;
    gchar * param_getSVGValue() const override;
    gchar * param_getDefaultSVGValue() const override;

    void param_setValue(Glib::ustring newvalue);
    
    void param_set_default() override;

    const Glib::ustring get_value() const { return defvalue; };

private:
    FontButtonParam(const FontButtonParam&) = delete;
    FontButtonParam& operator=(const FontButtonParam&) = delete;
    Glib::ustring value;
    Glib::ustring defvalue;

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
