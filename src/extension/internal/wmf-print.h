// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Windows Metafile printing - implementation
 */
/* Author:
 *   Ulf Erikson <ulferikson@users.sf.net>
 *
 * Copyright (C) 2006-2008 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_INKSCAPE_EXTENSION_INTERNAL_WMF_PRINT_H
#define SEEN_INKSCAPE_EXTENSION_INTERNAL_WMF_PRINT_H

#include <3rdparty/libuemf/uwmf.h>
#include "extension/internal/metafile-print.h"

#include "splivarot.h"             // pieces for union on shapes
#include "display/canvas-bpath.h"  // for SPWindRule

namespace Inkscape {
namespace Extension {
namespace Internal {

class PrintWmf : public PrintMetafile
{
    uint32_t hbrush, hpen, hbrush_null, hpen_null;
    uint32_t hmiterlimit;               // used to minimize redundant records that set this

    unsigned int print_pathv (Geom::PathVector const &pathv, const Geom::Affine &transform);
    bool print_simple_shape (Geom::PathVector const &pathv, const Geom::Affine &transform);

public:
    PrintWmf();

    /* Print functions */
    unsigned int setup (Inkscape::Extension::Print * module) override;

    unsigned int begin (Inkscape::Extension::Print * module, SPDocument *doc) override;
    unsigned int finish (Inkscape::Extension::Print * module) override;

    /* Rendering methods */
    unsigned int fill (Inkscape::Extension::Print *module,
                               Geom::PathVector const &pathv,
                               Geom::Affine const &ctm, SPStyle const *style,
                               Geom::OptRect const &pbox, Geom::OptRect const &dbox,
                               Geom::OptRect const &bbox) override;
    unsigned int stroke (Inkscape::Extension::Print * module,
                                 Geom::PathVector const &pathv,
                                 Geom::Affine const &ctm, SPStyle const *style,
                                 Geom::OptRect const &pbox, Geom::OptRect const &dbox,
                                 Geom::OptRect const &bbox) override;
    unsigned int image(Inkscape::Extension::Print *module,
                           unsigned char *px,
                           unsigned int w,
                           unsigned int h,
                           unsigned int rs,
                           Geom::Affine const &transform,
                           SPStyle const *style) override;
    unsigned int comment(Inkscape::Extension::Print *module, const char * comment) override;
    unsigned int text(Inkscape::Extension::Print *module, char const *text,
                              Geom::Point const &p, SPStyle const *style) override;

    static void init ();
protected:
    static void  smuggle_adxky_out(const char *string, int16_t **adx, double *ky, int *rtl, int *ndx, float scale);

    int create_brush(SPStyle const *style, PU_COLORREF fcolor) override;
    void destroy_brush() override;
    int create_pen(SPStyle const *style, const Geom::Affine &transform) override;
    void destroy_pen() override;
};

}  /* namespace Internal */
}  /* namespace Extension */
}  /* namespace Inkscape */


#endif /* __INKSCAPE_EXTENSION_INTERNAL_PRINT_WMF_H__ */

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
