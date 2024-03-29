// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Taper Stroke path effect (meant as a replacement for using Power Strokes for tapering)
 */
/* Authors:
 *   Liam P White <inkscapebrony@gmail.com>
 * Copyright (C) 2014 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_LPE_TAPERSTROKE_H
#define INKSCAPE_LPE_TAPERSTROKE_H

#include "live_effects/parameter/enum.h"
#include "live_effects/effect.h"
#include "live_effects/parameter/parameter.h"
#include "live_effects/parameter/vector.h"

namespace Inkscape {
namespace LivePathEffect {

namespace TpS {
// we need a separate namespace to avoid clashes with other LPEs
class KnotHolderEntityAttachBegin;
class KnotHolderEntityAttachEnd;
}

class LPETaperStroke : public Effect {
public:
    LPETaperStroke(LivePathEffectObject *lpeobject);
    ~LPETaperStroke() override = default;

    void doOnApply(SPLPEItem const* lpeitem) override;
    void doOnRemove(SPLPEItem const* lpeitem) override;

    Geom::PathVector doEffect_path (Geom::PathVector const& path_in) override;
    Geom::PathVector doEffect_simplePath(Geom::PathVector const& path_in);

    void addKnotHolderEntities(KnotHolder * knotholder, SPItem * item) override;

    friend class TpS::KnotHolderEntityAttachBegin;
    friend class TpS::KnotHolderEntityAttachEnd;
private:
    ScalarParam line_width;
    ScalarParam attach_start;
    ScalarParam attach_end;
    ScalarParam smoothing;
    EnumParam<unsigned> join_type;
    ScalarParam miter_limit;

    Geom::Point start_attach_point;
    Geom::Point end_attach_point;

    LPETaperStroke(const LPETaperStroke&) = delete;
    LPETaperStroke& operator=(const LPETaperStroke&) = delete;
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
