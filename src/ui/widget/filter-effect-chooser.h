// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef __FILTER_EFFECT_CHOOSER_H__
#define __FILTER_EFFECT_CHOOSER_H__

/*
 * Filter effect selection selection widget
 *
 * Author:
 *   Nicholas Bishop <nicholasbishop@gmail.com>
 *   Tavmjong Bah
 *
 * Copyright (C) 2007, 2017 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gtkmm/box.h>
#include <gtkmm/combobox.h>

#include "combo-enums.h"
#include "filter-enums.h"
#include "spin-scale.h"

namespace Inkscape {
namespace UI {
namespace Widget {

/* Allows basic control over feBlend and feGaussianBlur effects as well as opacity.
 *  Common for Object, Layers, and Fill and Stroke dialogs.
*/
class SimpleFilterModifier : public Gtk::VBox
{
public:
    enum Flags {
      NONE   = 0,
      BLUR   = 1,
      OPACITY= 2,
      BLEND  = 4
    };

    SimpleFilterModifier(int flags);

    sigc::signal<void>& signal_blend_changed();
    sigc::signal<void>& signal_blur_changed();
    sigc::signal<void>& signal_opacity_changed();

    const Glib::ustring get_blend_mode();
    // Uses blend mode enum values, or -1 for a complex filter
    void set_blend_mode(const int);

    double get_blur_value() const;
    void   set_blur_value(const double);

    double get_opacity_value() const;
    void   set_opacity_value(const double);

private:
    int _flags;

    Gtk::HBox _hb_blend;
    Gtk::Label _lb_blend;
    ComboBoxEnum<Inkscape::Filters::FilterBlendMode> _blend;
    SpinScale _blur;
    SpinScale _opacity;

    sigc::signal<void> _signal_blend_changed;
    sigc::signal<void> _signal_blur_changed;
    sigc::signal<void> _signal_opacity_changed;
};

}
}
}

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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
