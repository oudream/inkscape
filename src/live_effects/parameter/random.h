// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_RANDOM_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_RANDOM_H

/*
 * Inkscape::LivePathEffectParameters
 *
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "live_effects/parameter/parameter.h"
#include <glibmm/ustring.h>
#include <2geom/point.h>
#include <2geom/path.h>

namespace Inkscape {

namespace LivePathEffect {

class RandomParam : public Parameter {
public:
    RandomParam(  const Glib::ustring& label,
                const Glib::ustring& tip,
                const Glib::ustring& key, 
                Inkscape::UI::Widget::Registry* wr,
                Effect* effect,
                gdouble default_value = 1.0,
                long default_seed = 0);
    ~RandomParam() override;

    bool param_readSVGValue(const gchar * strvalue) override;
    gchar * param_getSVGValue() const override;
    gchar * param_getDefaultSVGValue() const override;
    void param_set_default() override;

    Gtk::Widget * param_newWidget() override;

    void param_set_value(gdouble val, long newseed);
    void param_make_integer(bool yes = true);
    void param_set_range(gdouble min, gdouble max);
    void param_update_default(gdouble default_value);
    void param_update_default(const gchar * default_value) override;
    void resetRandomizer();
    operator gdouble();
    inline gdouble get_value() { return value; } ;

protected:
    long startseed;
    long seed;
    long defseed;

    gdouble value;
    gdouble min;
    gdouble max;
    bool integer;
    gdouble defvalue;

private:
    bool on_button_release(GdkEventButton* button_event);
    long setup_seed(long);
    gdouble rand();

    RandomParam(const RandomParam&) = delete;
    RandomParam& operator=(const RandomParam&) = delete;
};

} //namespace LivePathEffect

} //namespace Inkscape

#endif
