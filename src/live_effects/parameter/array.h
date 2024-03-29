// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_ARRAY_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_ARRAY_H

/*
 * Inkscape::LivePathEffectParameters
 *
 * Copyright (C) Johan Engelen 2008 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <vector>

#include <glib.h>

#include "live_effects/parameter/parameter.h"

#include "helper/geom-satellite.h"
#include "svg/svg.h"
#include "svg/stringstream.h"

namespace Inkscape {

namespace LivePathEffect {

template <typename StorageType>
class ArrayParam : public Parameter {
public:
    ArrayParam( const Glib::ustring& label,
                const Glib::ustring& tip,
                const Glib::ustring& key,
                Inkscape::UI::Widget::Registry* wr,
                Effect* effect,
                size_t n = 0 )
        : Parameter(label, tip, key, wr, effect), _vector(n), _default_size(n)
    {

    }

    ~ArrayParam() override = default;;

    std::vector<StorageType> const & data() const {
        return _vector;
    }

    Gtk::Widget * param_newWidget() override {
        return nullptr;
    }

    bool param_readSVGValue(const gchar * strvalue) override {
        _vector.clear();
        gchar ** strarray = g_strsplit(strvalue, "|", 0);
        gchar ** iter = strarray;
        while (*iter != nullptr) {
            _vector.push_back( readsvg(*iter) );
            iter++;
        }
        g_strfreev (strarray);
        return true;
    }
    void param_update_default(const gchar * default_value) override{};
    gchar * param_getSVGValue() const override {
        Inkscape::SVGOStringStream os;
        writesvg(os, _vector);
        return g_strdup(os.str().c_str());
    }
    
    gchar * param_getDefaultSVGValue() const override {
        return g_strdup("");
    }

    void param_setValue(std::vector<StorageType> const &new_vector) {
        _vector = new_vector;
    }

    void param_set_default() override {
        param_setValue( std::vector<StorageType>(_default_size) );
    }

    void param_set_and_write_new_value(std::vector<StorageType> const &new_vector) {
        Inkscape::SVGOStringStream os;
        writesvg(os, new_vector);
        gchar * str = g_strdup(os.str().c_str());
        param_write_to_repr(str);
        g_free(str);
    }

protected:
    std::vector<StorageType> _vector;
    size_t _default_size;

    void writesvg(SVGOStringStream &str, std::vector<StorageType> const &vector) const {
        for (unsigned int i = 0; i < vector.size(); ++i) {
            if (i != 0) {
                // separate items with pipe symbol
                str << " | ";
            }
            writesvgData(str,vector[i]);
        }
    }
    
    void writesvgData(SVGOStringStream &str, float const &vector_data) const {
        str << vector_data;
    }

    void writesvgData(SVGOStringStream &str, double const &vector_data) const {
        str << vector_data;
    }

    void writesvgData(SVGOStringStream &str, Geom::Point const &vector_data) const {
        str << vector_data;
    }

    void writesvgData(SVGOStringStream &str, std::vector<Satellite> const &vector_data) const {
        for (size_t i = 0; i < vector_data.size(); ++i) {
            if (i != 0) {
                // separate items with @ symbol ¿Any other?
                str << " @ ";
            }
            str << vector_data[i].getSatelliteTypeGchar();
            str << ",";
            str << vector_data[i].is_time;
            str << ",";
            str << vector_data[i].selected;
            str << ",";
            str << vector_data[i].has_mirror;
            str << ",";
            str << vector_data[i].hidden;
            str << ",";
            str << vector_data[i].amount;
            str << ",";
            str << vector_data[i].angle;
            str << ",";
            str << static_cast<int>(vector_data[i].steps);
        }
    }

    StorageType readsvg(const gchar * str);

private:
    ArrayParam(const ArrayParam&);
    ArrayParam& operator=(const ArrayParam&);
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
