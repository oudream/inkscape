// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PowerStroke LPE implementation. Creates curves with modifiable stroke width.
 */
/* Authors:
 *   Johan Engelen <j.b.c.engelen@alumnus.utwente.nl>
 *
 * Copyright (C) 2010-2012 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "live_effects/lpe-powerstroke.h"
#include "live_effects/lpe-powerstroke-interpolators.h"
#include "live_effects/lpe-simplify.h"
#include "live_effects/lpeobject.h"

#include "svg/svg-color.h"
#include "desktop-style.h"
#include "svg/css-ostringstream.h"
#include "display/curve.h"

#include <2geom/elliptical-arc.h>
#include <2geom/path-sink.h>
#include <2geom/path-intersection.h>
#include <2geom/circle.h>
#include "helper/geom.h"

#include "object/sp-shape.h"
#include "style.h"

// TODO due to internal breakage in glibmm headers, this must be last:
#include <glibmm/i18n.h>

namespace Geom {
// should all be moved to 2geom at some point

/** Find the point where two straight lines cross.
*/
static boost::optional<Point> intersection_point( Point const & origin_a, Point const & vector_a,
                                           Point const & origin_b, Point const & vector_b)
{
    Coord denom = cross(vector_a, vector_b);
    if (!are_near(denom,0.)){
        Coord t = (cross(vector_b, origin_a) + cross(origin_b, vector_b)) / denom;
        return origin_a + t * vector_a;
    }
    return boost::none;
}

static Geom::CubicBezier sbasis_to_cubicbezier(Geom::D2<Geom::SBasis> const & sbasis_in)
{
    std::vector<Geom::Point> temp;
    sbasis_to_bezier(temp, sbasis_in, 4);
    return Geom::CubicBezier( temp );
}

/**
 * document this!
 * very quick: this finds the ellipse with minimum eccentricity
   passing through point P and Q, with tangent PO at P and QO at Q
   http://mathforum.org/kb/message.jspa?messageID=7471596&tstart=0
 */
static Ellipse find_ellipse(Point P, Point Q, Point O)
{
    Point p = P - O;
    Point q = Q - O;
    Coord K = 4 * dot(p,q) / (L2sq(p) + L2sq(q));

    double cross = p[Y]*q[X] - p[X]*q[Y];
    double a = -q[Y]/cross;
    double b = q[X]/cross;
    double c = (O[X]*q[Y] - O[Y]*q[X])/cross;

    double d = p[Y]/cross;
    double e = -p[X]/cross;
    double f = (-O[X]*p[Y] + O[Y]*p[X])/cross;

    // Ax^2 + Bxy + Cy^2 + Dx + Ey + F = 0
    double A = (a*d*K+d*d+a*a);
    double B = (a*e*K+b*d*K+2*d*e+2*a*b);
    double C = (b*e*K+e*e+b*b);
    double D = (a*f*K+c*d*K+2*d*f-2*d+2*a*c-2*a);
    double E = (b*f*K+c*e*K+2*e*f-2*e+2*b*c-2*b);
    double F = c*f*K+f*f-2*f+c*c-2*c+1;

    return Ellipse(A, B, C, D, E, F);
}

/**
 * Find circle that touches inside of the curve, with radius matching the curvature, at time value \c t.
 * Because this method internally uses unitTangentAt, t should be smaller than 1.0 (see unitTangentAt).
 */
static Circle touching_circle( D2<SBasis> const &curve, double t, double tol=0.01 )
{
    //Piecewise<SBasis> k = curvature(curve, tol);
    D2<SBasis> dM=derivative(curve);
    if ( are_near(L2sq(dM(t)),0.) && (dM[0].size() > 1) && (dM[1].size() > 1) ) {
        dM=derivative(dM);
    }
    if ( are_near(L2sq(dM(t)),0.) && (dM[0].size() > 1) && (dM[1].size() > 1) ) {   // try second time
        dM=derivative(dM);
    }
    if ( dM.isZero(tol) || (are_near(L2sq(dM(t)),0.) && (dM[0].size() > 1) && (dM[1].size() > 1) )) {   // admit defeat
        return Geom::Circle(Geom::Point(0., 0.), 0.);
    }
    Piecewise<D2<SBasis> > unitv = unitVector(dM,tol);
    if (unitv.empty()) {   // admit defeat
        return Geom::Circle(Geom::Point(0., 0.), 0.);
    }
    Piecewise<SBasis> dMlength = dot(Piecewise<D2<SBasis> >(dM),unitv);
    Piecewise<SBasis> k = cross(derivative(unitv),unitv);
    k = divide(k,dMlength,tol,3);
    double curv = k(t); // note that this value is signed

    Geom::Point normal = unitTangentAt(curve, t).cw();
    double radius = 1/curv;
    Geom::Point center = curve(t) + radius*normal;
    return Geom::Circle(center, fabs(radius));
}

} // namespace Geom

namespace Inkscape {
namespace LivePathEffect {

static const Util::EnumData<unsigned> InterpolatorTypeData[] = {
    {Geom::Interpolate::INTERP_CUBICBEZIER_SMOOTH,  N_("CubicBezierSmooth"), "CubicBezierSmooth"},
    {Geom::Interpolate::INTERP_LINEAR          , N_("Linear"), "Linear"},
    {Geom::Interpolate::INTERP_CUBICBEZIER          , N_("CubicBezierFit"), "CubicBezierFit"},
    {Geom::Interpolate::INTERP_CUBICBEZIER_JOHAN     , N_("CubicBezierJohan"), "CubicBezierJohan"},
    {Geom::Interpolate::INTERP_SPIRO  , N_("SpiroInterpolator"), "SpiroInterpolator"},
    {Geom::Interpolate::INTERP_CENTRIPETAL_CATMULLROM, N_("Centripetal Catmull-Rom"), "CentripetalCatmullRom"}
};
static const Util::EnumDataConverter<unsigned> InterpolatorTypeConverter(InterpolatorTypeData, sizeof(InterpolatorTypeData)/sizeof(*InterpolatorTypeData));

enum LineJoinType {
  LINEJOIN_BEVEL,
  LINEJOIN_ROUND,
  LINEJOIN_EXTRP_MITER,
  LINEJOIN_MITER,
  LINEJOIN_SPIRO,
  LINEJOIN_EXTRP_MITER_ARC
};
static const Util::EnumData<unsigned> LineJoinTypeData[] = {
    {LINEJOIN_BEVEL, N_("Beveled"),   "bevel"},
    {LINEJOIN_ROUND, N_("Rounded"),   "round"},
//    {LINEJOIN_EXTRP_MITER,  N_("Extrapolated"),      "extrapolated"}, // disabled because doesn't work well
    {LINEJOIN_EXTRP_MITER_ARC, N_("Extrapolated arc"),     "extrp_arc"}, 
    {LINEJOIN_MITER, N_("Miter"),     "miter"},
    {LINEJOIN_SPIRO, N_("Spiro"),     "spiro"},
};
static const Util::EnumDataConverter<unsigned> LineJoinTypeConverter(LineJoinTypeData, sizeof(LineJoinTypeData)/sizeof(*LineJoinTypeData));

LPEPowerStroke::LPEPowerStroke(LivePathEffectObject *lpeobject) :
    Effect(lpeobject),
    offset_points(_("Offset points"), _("Offset points"), "offset_points", &wr, this),
    sort_points(_("Sort points"), _("Sort offset points according to their time value along the curve"), "sort_points", &wr, this, true),
    interpolator_type(_("Interpolator type:"), _("Determines which kind of interpolator will be used to interpolate between stroke width along the path"), "interpolator_type", InterpolatorTypeConverter, &wr, this, Geom::Interpolate::INTERP_CENTRIPETAL_CATMULLROM),
    interpolator_beta(_("Smoothness:"), _("Sets the smoothness for the CubicBezierJohan interpolator; 0 = linear interpolation, 1 = smooth"), "interpolator_beta", &wr, this, 0.2),
    scale_width(_("Width scale:"), _("Width scale all points"), "scale_width", &wr, this, 1.0),
    start_linecap_type(_("Start cap:"), _("Determines the shape of the path's start"), "start_linecap_type", LineCapTypeConverter, &wr, this, LINECAP_ZERO_WIDTH),
    linejoin_type(_("Join:"), _("Determines the shape of the path's corners"), "linejoin_type", LineJoinTypeConverter, &wr, this, LINEJOIN_ROUND),
    miter_limit(_("Miter limit:"), _("Maximum length of the miter (in units of stroke width)"), "miter_limit", &wr, this, 4.),
    end_linecap_type(_("End cap:"), _("Determines the shape of the path's end"), "end_linecap_type", LineCapTypeConverter, &wr, this, LINECAP_ZERO_WIDTH)
{
    show_orig_path = true;

    /// @todo offset_points are initialized with empty path, is that bug-save?

    interpolator_beta.addSlider(true);
    interpolator_beta.param_set_range(0.,1.);

    registerParameter(&offset_points);
    registerParameter(&sort_points);
    registerParameter(&interpolator_type);
    registerParameter(&interpolator_beta);
    registerParameter(&start_linecap_type);
    registerParameter(&linejoin_type);
    registerParameter(&miter_limit);
    registerParameter(&scale_width);
    registerParameter(&end_linecap_type);
    scale_width.param_set_range(0.0, Geom::infinity());
    scale_width.param_set_increments(0.1, 0.1);
    scale_width.param_set_digits(4);
    recusion_limit = 0;
    has_recursion = false;
}

LPEPowerStroke::~LPEPowerStroke() = default;

void 
LPEPowerStroke::doBeforeEffect(SPLPEItem const *lpeItem)
{
    offset_points.set_scale_width(scale_width);
    if (has_recursion) {
        has_recursion = false;
        adjustForNewPath(pathvector_before_effect);
    }
}

void LPEPowerStroke::applyStyle(SPLPEItem *lpeitem)
{
    SPCSSAttr *css = sp_repr_css_attr_new();
    if (lpeitem->style) {
        if (lpeitem->style->stroke.isPaintserver()) {
            SPPaintServer *server = lpeitem->style->getStrokePaintServer();
            if (server) {
                Glib::ustring str;
                str += "url(#";
                str += server->getId();
                str += ")";
                sp_repr_css_set_property(css, "fill", str.c_str());
            }
        } else if (lpeitem->style->stroke.isColor()) {
            gchar c[64];
            sp_svg_write_color(
                c, sizeof(c),
                lpeitem->style->stroke.value.color.toRGBA32(SP_SCALE24_TO_FLOAT(lpeitem->style->stroke_opacity.value)));
            sp_repr_css_set_property(css, "fill", c);
        } else {
            sp_repr_css_set_property(css, "fill", "none");
        }
    } else {
        sp_repr_css_unset_property(css, "fill");
    }

    sp_repr_css_set_property(css, "fill-rule", "nonzero");
    sp_repr_css_set_property(css, "stroke", "none");

    sp_desktop_apply_css_recursive(lpeitem, css, true);
    sp_repr_css_attr_unref(css);
}

void
LPEPowerStroke::doOnApply(SPLPEItem const* lpeitem)
{
    if (SP_IS_SHAPE(lpeitem)) {
        SPLPEItem* item = const_cast<SPLPEItem*>(lpeitem);
        std::vector<Geom::Point> points;
        Geom::PathVector const &pathv = pathv_to_linear_and_cubic_beziers(SP_SHAPE(lpeitem)->_curve->get_pathvector());
        double width = (lpeitem && lpeitem->style) ? lpeitem->style->stroke_width.computed / 2 : 1.;
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        Glib::ustring pref_path_pp = "/live_effects/powerstroke/powerpencil";
        bool powerpencil = prefs->getBool(pref_path_pp, false);
        if (!powerpencil) {
            applyStyle(item);
            item->updateRepr();
            if (pathv.empty()) {
                points.emplace_back(0.2,width );
                points.emplace_back(0.5, width);
                points.emplace_back(0.8, width);
            } else {
                Geom::Path const &path = pathv.front();
                Geom::Path::size_type const size = path.size_default();
                if (!path.closed()) {
                    points.emplace_back(0.2, width);
                }
                points.emplace_back(0.5 * size, width);
                if (!path.closed()) {
                    points.emplace_back(size - 0.2, width);
                }
            }
            offset_points.param_set_and_write_new_value(points);
        }
        offset_points.set_scale_width(scale_width);
    } else {
        if (!SP_IS_SHAPE(lpeitem)) {
            g_warning("LPE Powerstroke can only be applied to shapes (not groups).");
        }
    }
}

void LPEPowerStroke::doOnRemove(SPLPEItem const* lpeitem)
{
    if (SP_IS_SHAPE(lpeitem) && !keep_paths) {
        SPLPEItem *item = const_cast<SPLPEItem*>(lpeitem);
        SPCSSAttr *css = sp_repr_css_attr_new ();
        if (lpeitem->style->fill.isPaintserver()) {
            SPPaintServer * server = lpeitem->style->getFillPaintServer();
            if (server) {
                Glib::ustring str;
                str += "url(#";
                str += server->getId();
                str += ")";
                sp_repr_css_set_property (css, "stroke", str.c_str());
            }
        } else if (lpeitem->style->fill.isColor()) {
            char c[64] = {0};
            sp_svg_write_color (c, sizeof(c), lpeitem->style->fill.value.color.toRGBA32(SP_SCALE24_TO_FLOAT(lpeitem->style->fill_opacity.value)));
            sp_repr_css_set_property (css, "stroke", c);
        } else {
            sp_repr_css_set_property (css, "stroke", "none");
        }

        Inkscape::CSSOStringStream os;
        os << std::abs(offset_points.median_width()) * 2;
        sp_repr_css_set_property (css, "stroke-width", os.str().c_str());

        sp_repr_css_set_property(css, "fill", "none");

        sp_desktop_apply_css_recursive(item, css, true);
        sp_repr_css_attr_unref (css);

        item->updateRepr();
    }
}

void
LPEPowerStroke::adjustForNewPath(Geom::PathVector const & path_in)
{
    if (!path_in.empty()) {
        offset_points.recalculate_controlpoints_for_new_pwd2(path_in[0].toPwSb());
    }
}

static bool compare_offsets (Geom::Point first, Geom::Point second)
{
    return first[Geom::X] < second[Geom::X];
}

static Geom::Path path_from_piecewise_fix_cusps( Geom::Piecewise<Geom::D2<Geom::SBasis> > const & B,
                                                 Geom::Piecewise<Geom::SBasis> const & y, // width path
                                                 LineJoinType jointype,
                                                 double miter_limit,
                                                 double tol=Geom::EPSILON)
{
/* per definition, each discontinuity should be fixed with a join-ending, as defined by linejoin_type
*/
    Geom::PathBuilder pb;
    if (B.empty()) {
        return pb.peek().front();
    }

    pb.setStitching(true);

    Geom::Point start = B[0].at0();
    pb.moveTo(start);
    build_from_sbasis(pb, B[0], tol, false);
    unsigned prev_i = 0;
    for (unsigned i=1; i < B.size(); i++) {
        // if segment is degenerate, skip it
        // the degeneracy/constancy test had to be loosened (eps > 1e-5) 
        if (B[i].isConstant(1e-4)) {
            continue;
        }
        if (!are_near(B[prev_i].at1(), B[i].at0(), tol) )
        { // discontinuity found, so fix it :-)
            double width = y( B.cuts[i] );

            Geom::Point tang1 = -unitTangentAt(reverse(B[prev_i]),0.); // = unitTangentAt(B[prev_i],1);
            Geom::Point tang2 = unitTangentAt(B[i],0);
            Geom::Point discontinuity_vec = B[i].at0() - B[prev_i].at1();
            bool on_outside = ( dot(tang1, discontinuity_vec) >= 0. );

            if (on_outside) {
                // we are on the outside: add some type of join!
                switch (jointype) {
                case LINEJOIN_ROUND: {
                    /* for constant width paths, the rounding is a circular arc (rx == ry),
                       for non-constant width paths, the rounding can be done with an ellipse but is hard and ambiguous.
                       The elliptical arc should go through the discontinuity's start and end points (of course!)
                       and also should match the discontinuity tangents at those start and end points.
                       To resolve the ambiguity, the elliptical arc with minimal eccentricity should be chosen.
                       A 2Geom method was created to do exactly this :)
                       */

                    boost::optional<Geom::Point> O = intersection_point( B[prev_i].at1(), tang1,
                                                                              B[i].at0(), tang2 );
                    if (!O) {
                        // no center found, i.e. 180 degrees round
                       pb.lineTo(B[i].at0()); // default to bevel for too shallow cusp angles
                       break;
                    }

                    Geom::Ellipse ellipse;
                    try {
                        ellipse = find_ellipse(B[prev_i].at1(), B[i].at0(), *O);
                    }
                    catch (Geom::LogicalError &e) {
                        // 2geom did not find a fitting ellipse, this happens for weird thick paths :)
                        // do bevel, and break
                         pb.lineTo(B[i].at0());
                         break;
                    }

                    // check if ellipse.ray is within 'sane' range.
                    if ( ( fabs(ellipse.ray(Geom::X)) > 1e6 ) ||
                         ( fabs(ellipse.ray(Geom::Y)) > 1e6 ) )
                    {
                        // do bevel, and break
                         pb.lineTo(B[i].at0());
                         break;
                    }

                    pb.arcTo( ellipse.ray(Geom::X), ellipse.ray(Geom::Y), ellipse.rotationAngle(),
                              false, width < 0, B[i].at0() );

                    break;
                }
                case LINEJOIN_EXTRP_MITER: {
                    Geom::D2<Geom::SBasis> newcurve1 = B[prev_i] * Geom::reflection(rot90(tang1), B[prev_i].at1());
                    Geom::CubicBezier bzr1 = sbasis_to_cubicbezier( reverse(newcurve1) );

                    Geom::D2<Geom::SBasis> newcurve2 = B[i] * Geom::reflection(rot90(tang2), B[i].at0());
                    Geom::CubicBezier bzr2 = sbasis_to_cubicbezier( reverse(newcurve2) );

                    Geom::Crossings cross = crossings(bzr1, bzr2);
                    if (cross.empty()) {
                        // empty crossing: default to bevel
                        pb.lineTo(B[i].at0());
                    } else {
                        // check size of miter
                        Geom::Point point_on_path = B[prev_i].at1() - rot90(tang1) * width;
                        Geom::Coord len = distance(bzr1.pointAt(cross[0].ta), point_on_path);
                        if (len > fabs(width) * miter_limit) {
                            // miter too big: default to bevel
                            pb.lineTo(B[i].at0());
                        } else {
                            std::pair<Geom::CubicBezier, Geom::CubicBezier> sub1 = bzr1.subdivide(cross[0].ta);
                            std::pair<Geom::CubicBezier, Geom::CubicBezier> sub2 = bzr2.subdivide(cross[0].tb);
                            pb.curveTo(sub1.first[1], sub1.first[2], sub1.first[3]);
                            pb.curveTo(sub2.second[1], sub2.second[2], sub2.second[3]);
                        }
                    }
                    break;
                }
                case LINEJOIN_EXTRP_MITER_ARC: {
                    // Extrapolate using the curvature at the end of the path segments to join
                    Geom::Circle circle1 = Geom::touching_circle(reverse(B[prev_i]), 0.0);
                    Geom::Circle circle2 = Geom::touching_circle(B[i], 0.0);
                    std::vector<Geom::ShapeIntersection> solutions;
                    solutions = circle1.intersect(circle2);
                    if (solutions.size() == 2) {
                        Geom::Point sol(0.,0.);
                        if ( dot(tang2, solutions[0].point() - B[i].at0()) > 0 ) {
                            // points[0] is bad, choose points[1]
                            sol = solutions[1].point();
                        } else if ( dot(tang2, solutions[1].point() - B[i].at0()) > 0 ) { // points[0] could be good, now check points[1]
                            // points[1] is bad, choose points[0]
                            sol = solutions[0].point();
                        } else {
                            // both points are good, choose nearest
                            sol = ( distanceSq(B[i].at0(), solutions[0].point()) < distanceSq(B[i].at0(), solutions[1].point()) ) ?
                                    solutions[0].point() : solutions[1].point();
                        }

                        Geom::EllipticalArc *arc0 = circle1.arc(B[prev_i].at1(), 0.5*(B[prev_i].at1()+sol), sol);
                        Geom::EllipticalArc *arc1 = circle2.arc(sol, 0.5*(sol+B[i].at0()), B[i].at0());

                        if (arc0) {
                            build_from_sbasis(pb,arc0->toSBasis(), tol, false);
                            delete arc0;
                            arc0 = nullptr;
                        }
                        if (arc1) {
                            build_from_sbasis(pb,arc1->toSBasis(), tol, false);
                            delete arc1;
                            arc1 = nullptr;
                        }

                        break;
                    } else {
                        // fall back to miter
                        boost::optional<Geom::Point> p = intersection_point( B[prev_i].at1(), tang1,
                                                                             B[i].at0(), tang2 );
                        if (p) {
                            // check size of miter
                            Geom::Point point_on_path = B[prev_i].at1() - rot90(tang1) * width;
                            Geom::Coord len = distance(*p, point_on_path);
                            if (len <= fabs(width) * miter_limit) {
                                // miter OK
                                pb.lineTo(*p);
                            }
                        }
                        pb.lineTo(B[i].at0());
                    }
                    /*else if (solutions == 1) { // one circle is inside the other
                        // don't know what to do: default to bevel
                        pb.lineTo(B[i].at0());
                    } else { // no intersections
                        // don't know what to do: default to bevel
                        pb.lineTo(B[i].at0());
                    } */

                    break;
                }
                case LINEJOIN_MITER: {
                    boost::optional<Geom::Point> p = intersection_point( B[prev_i].at1(), tang1,
                                                                         B[i].at0(), tang2 );
                    if (p) {
                        // check size of miter
                        Geom::Point point_on_path = B[prev_i].at1() - rot90(tang1) * width;
                        Geom::Coord len = distance(*p, point_on_path);
                        if (len <= fabs(width) * miter_limit) {
                            // miter OK
                            pb.lineTo(*p);
                        }
                    }
                    pb.lineTo(B[i].at0());
                    break;
                }
                case LINEJOIN_SPIRO: {
                    Geom::Point direction = B[i].at0() - B[prev_i].at1();
                    double tang1_sign = dot(direction,tang1);
                    double tang2_sign = dot(direction,tang2);

                    Spiro::spiro_cp *controlpoints = g_new (Spiro::spiro_cp, 4);
                    controlpoints[0].x = (B[prev_i].at1() - tang1_sign*tang1)[Geom::X];
                    controlpoints[0].y = (B[prev_i].at1() - tang1_sign*tang1)[Geom::Y];
                    controlpoints[0].ty = '{';
                    controlpoints[1].x = B[prev_i].at1()[Geom::X];
                    controlpoints[1].y = B[prev_i].at1()[Geom::Y];
                    controlpoints[1].ty = ']';
                    controlpoints[2].x = B[i].at0()[Geom::X];
                    controlpoints[2].y = B[i].at0()[Geom::Y];
                    controlpoints[2].ty = '[';
                    controlpoints[3].x = (B[i].at0() + tang2_sign*tang2)[Geom::X];
                    controlpoints[3].y = (B[i].at0() + tang2_sign*tang2)[Geom::Y];
                    controlpoints[3].ty = '}';

                    Geom::Path spiro;
                    Spiro::spiro_run(controlpoints, 4, spiro);
                    pb.append(spiro.portion(1, spiro.size_open() - 1));
                    break;
                }
                case LINEJOIN_BEVEL:
                default:
                    pb.lineTo(B[i].at0());
                    break;
                }

                build_from_sbasis(pb, B[i], tol, false);

            } else {
                // we are on inside of corner!
                Geom::Path bzr1 = path_from_sbasis( B[prev_i], tol );
                Geom::Path bzr2 = path_from_sbasis( B[i], tol );
                Geom::Crossings cross = crossings(bzr1, bzr2);
                if (cross.size() != 1) {
                    // empty crossing or too many crossings: default to bevel
                    pb.lineTo(B[i].at0());
                    pb.append(bzr2);
                } else {
                    // :-) quick hack:
                    for (unsigned i=0; i < bzr1.size_open(); ++i) {
                        pb.backspace();
                    }

                    pb.append( bzr1.portion(0, cross[0].ta) );
                    pb.append( bzr2.portion(cross[0].tb, bzr2.size_open()) );
                }
            }
        } else {
            build_from_sbasis(pb, B[i], tol, false);
        }

        prev_i = i;
    }
    pb.flush();
    return pb.peek().front();
}

Geom::PathVector
LPEPowerStroke::doEffect_path (Geom::PathVector const & path_in)
{
    using namespace Geom;

    Geom::PathVector path_out;
    if (path_in.empty()) {
        return path_in;
    }
    Geom::PathVector pathv = pathv_to_linear_and_cubic_beziers(path_in);
    Geom::Piecewise<Geom::D2<Geom::SBasis> > pwd2_in = pathv[0].toPwSb();
    if (pwd2_in.empty()) {
        return path_in;
    }
    Piecewise<D2<SBasis> > der = derivative(pwd2_in);
    if (der.empty()) {
        return path_in;
    }
    Piecewise<D2<SBasis> > n = unitVector(der,0.00001);
    if (n.empty()) {
        return path_in;
    }

    n = rot90(n);
    offset_points.set_pwd2(pwd2_in, n);

    LineCapType end_linecap = static_cast<LineCapType>(end_linecap_type.get_value());
    LineCapType start_linecap = static_cast<LineCapType>(start_linecap_type.get_value());

    std::vector<Geom::Point> ts_no_scale = offset_points.data();
    if (ts_no_scale.empty()) {
        return path_in;
    }
    std::vector<Geom::Point> ts;
    for (auto & tsp : ts_no_scale) {
        Geom::Point p = Geom::Point(tsp[Geom::X], tsp[Geom::Y] * scale_width);
        ts.push_back(p);
    }
    if (sort_points) {
        sort(ts.begin(), ts.end(), compare_offsets);
    }
    // create stroke path where points (x,y) := (t, offset)
    Geom::Interpolate::Interpolator *interpolator = Geom::Interpolate::Interpolator::create(static_cast<Geom::Interpolate::InterpolatorType>(interpolator_type.get_value()));
    if (Geom::Interpolate::CubicBezierJohan *johan = dynamic_cast<Geom::Interpolate::CubicBezierJohan*>(interpolator)) {
        johan->setBeta(interpolator_beta);
    }
    if (Geom::Interpolate::CubicBezierSmooth *smooth = dynamic_cast<Geom::Interpolate::CubicBezierSmooth*>(interpolator)) {
        smooth->setBeta(interpolator_beta);
    }
    if (pathv[0].closed()) {
        std::vector<Geom::Point> ts_close;
        //we have only one knot or overwrite before
        Geom::Point start = Geom::Point( pwd2_in.domain().min(), ts.front()[Geom::Y]);
        Geom::Point end   = Geom::Point( pwd2_in.domain().max(), ts.front()[Geom::Y]); 
        if (ts.size() > 1) {
            ts_close.push_back(ts[ts.size()-2]);
            ts_close.push_back(ts.back());
            ts_close.push_back(ts.front());
            ts_close.push_back(ts[1]);
            Geom::Path closepath = interpolator->interpolateToPath(ts_close);
            start = closepath.pointAt(Geom::nearest_time(Geom::Point( pwd2_in.domain().min(),0), closepath));
            start[Geom::X] = pwd2_in.domain().min();
            end   = start;
            end[Geom::X] = pwd2_in.domain().max();
        }
        ts.insert(ts.begin(), start );
        ts.push_back( end );
        ts_close.clear();
    } else {
        // add width data for first and last point on the path
        // depending on cap type, these first and last points have width zero or take the width from the closest width point.
        ts.insert(ts.begin(), Point( pwd2_in.domain().min(),
                                    (start_linecap==LINECAP_ZERO_WIDTH) ? 0. : ts.front()[Geom::Y]) );
        ts.emplace_back( pwd2_in.domain().max(),
                             (end_linecap==LINECAP_ZERO_WIDTH) ? 0. : ts.back()[Geom::Y] );
    }

    // do the interpolation in a coordinate system that is more alike to the on-canvas knots,
    // instead of the heavily compressed coordinate system of (segment_no offset, Y) in which the knots are stored
    double pwd2_in_arclength = length(pwd2_in);
    double xcoord_scaling = pwd2_in_arclength / ts.back()[Geom::X];
    for (auto & t : ts) {
        t[Geom::X] *= xcoord_scaling;
    }
    
    Geom::Path strokepath = interpolator->interpolateToPath(ts);
    delete interpolator;

    // apply the inverse knot-xcoord scaling that was applied before the interpolation
    strokepath *= Scale(1/xcoord_scaling, 1);

    D2<Piecewise<SBasis> > patternd2 = make_cuts_independent(strokepath.toPwSb());
    Piecewise<SBasis> x = Piecewise<SBasis>(patternd2[0]);
    Piecewise<SBasis> y = Piecewise<SBasis>(patternd2[1]);
    // find time values for which x lies outside path domain
    // and only take portion of x and y that lies within those time values
    std::vector< double > rtsmin = roots (x - pwd2_in.domain().min());
    std::vector< double > rtsmax = roots (x + pwd2_in.domain().max());
    if ( !rtsmin.empty() && !rtsmax.empty() ) {
        x = portion(x, rtsmin.at(0), rtsmax.at(0));
        y = portion(y, rtsmin.at(0), rtsmax.at(0));
    }

    LineJoinType jointype = static_cast<LineJoinType>(linejoin_type.get_value());
    if (x.empty() || y.empty()) {
        return path_in;
    }
    Piecewise<D2<SBasis> > pwd2_out   = compose(pwd2_in,x) + y*compose(n,x);
    Piecewise<D2<SBasis> > mirrorpath = reverse( compose(pwd2_in,x) - y*compose(n,x));

    Geom::Path fixed_path       = path_from_piecewise_fix_cusps( pwd2_out,   y,          jointype, miter_limit, LPE_CONVERSION_TOLERANCE);
    Geom::Path fixed_mirrorpath = path_from_piecewise_fix_cusps( mirrorpath, reverse(y), jointype, miter_limit, LPE_CONVERSION_TOLERANCE);
    if (pathv[0].closed()) {
        fixed_path.close(true);
        path_out.push_back(fixed_path);
        fixed_mirrorpath.close(true);
        path_out.push_back(fixed_mirrorpath);
    } else {
        // add linecaps...
        switch (end_linecap) {
            case LINECAP_ZERO_WIDTH:
                // do nothing
                break;
            case LINECAP_PEAK:
            {
                Geom::Point end_deriv = -unitTangentAt( reverse(pwd2_in.segs.back()), 0.);
                double radius = 0.5 * distance(fixed_path.finalPoint(), fixed_mirrorpath.initialPoint());
                Geom::Point midpoint = 0.5*(fixed_path.finalPoint() + fixed_mirrorpath.initialPoint()) + radius*end_deriv;
                fixed_path.appendNew<LineSegment>(midpoint);
                fixed_path.appendNew<LineSegment>(fixed_mirrorpath.initialPoint());
                break;
            }
            case LINECAP_SQUARE:
            {
                Geom::Point end_deriv = -unitTangentAt( reverse(pwd2_in.segs.back()), 0.);
                double radius = 0.5 * distance(fixed_path.finalPoint(), fixed_mirrorpath.initialPoint());
                fixed_path.appendNew<LineSegment>( fixed_path.finalPoint() + radius*end_deriv );
                fixed_path.appendNew<LineSegment>( fixed_mirrorpath.initialPoint() + radius*end_deriv );
                fixed_path.appendNew<LineSegment>( fixed_mirrorpath.initialPoint() );
                break;
            }
            case LINECAP_BUTT:
            {
                fixed_path.appendNew<LineSegment>( fixed_mirrorpath.initialPoint() );
                break;
            }
            case LINECAP_ROUND:
            default:
            {
                double radius1 = 0.5 * distance(fixed_path.finalPoint(), fixed_mirrorpath.initialPoint());
                fixed_path.appendNew<EllipticalArc>( radius1, radius1, M_PI/2., false, y.lastValue() < 0, fixed_mirrorpath.initialPoint() );
                break;
            }
        }

        fixed_path.append(fixed_mirrorpath);
        switch (start_linecap) {
            case LINECAP_ZERO_WIDTH:
                // do nothing
                break;
            case LINECAP_PEAK:
            {
                Geom::Point start_deriv = unitTangentAt( pwd2_in.segs.front(), 0.);
                double radius = 0.5 * distance(fixed_path.initialPoint(), fixed_mirrorpath.finalPoint());
                Geom::Point midpoint = 0.5*(fixed_mirrorpath.finalPoint() + fixed_path.initialPoint()) - radius*start_deriv;
                fixed_path.appendNew<LineSegment>( midpoint );
                fixed_path.appendNew<LineSegment>( fixed_path.initialPoint() );
                break;
            }
            case LINECAP_SQUARE:
            {
                Geom::Point start_deriv = unitTangentAt( pwd2_in.segs.front(), 0.);
                double radius = 0.5 * distance(fixed_path.initialPoint(), fixed_mirrorpath.finalPoint());
                fixed_path.appendNew<LineSegment>( fixed_mirrorpath.finalPoint() - radius*start_deriv );
                fixed_path.appendNew<LineSegment>( fixed_path.initialPoint() - radius*start_deriv );
                fixed_path.appendNew<LineSegment>( fixed_path.initialPoint() );
                break;
            }
            case LINECAP_BUTT:
            {
                fixed_path.appendNew<LineSegment>( fixed_path.initialPoint() );
                break;
            }
            case LINECAP_ROUND:
            default:
            {
                double radius2 = 0.5 * distance(fixed_path.initialPoint(), fixed_mirrorpath.finalPoint());
                fixed_path.appendNew<EllipticalArc>( radius2, radius2, M_PI/2., false, y.firstValue() < 0, fixed_path.initialPoint() );
                break;
            }
        }
        fixed_path.close(true);
        path_out.push_back(fixed_path);
    }
    if (path_out.empty()) {
        return path_in;
        // doEffect_path (path_in);
    }
    return path_out;
}

void LPEPowerStroke::doAfterEffect(SPLPEItem const *lpeitem)
{
    is_load = false;
    if (pathvector_before_effect[0].size() == pathvector_after_effect[0].size()) {
        if (recusion_limit < 6) {
            Inkscape::LivePathEffect::Effect *effect =
                sp_lpe_item->getPathEffectOfType(Inkscape::LivePathEffect::SIMPLIFY);
            if (effect) {
                LivePathEffect::LPESimplify *simplify =
                    dynamic_cast<LivePathEffect::LPESimplify *>(effect->getLPEObj()->get_lpe());
                double threshold = simplify->threshold * 1.2;
                simplify->threshold.param_set_value(threshold);
                simplify->threshold.write_to_SVG();
                has_recursion = true;
            }
        }
        ++recusion_limit;
    } else {
        recusion_limit = 0;
    }
}

/* ######################## */

} //namespace LivePathEffect
} /* namespace Inkscape */

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
