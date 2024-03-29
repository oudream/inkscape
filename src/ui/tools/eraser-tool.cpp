// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Eraser drawing mode
 *
 * Authors:
 *   Mitsuru Oka <oka326@parkcity.ne.jp>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   MenTaLguY <mental@rydia.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * The original dynadraw code:
 *   Paul Haeberli <paul@sgi.com>
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 1999-2005 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 * Copyright (C) 2005-2007 bulia byak
 * Copyright (C) 2006 MenTaLguY
 * Copyright (C) 2008 Jon A. Cruz
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#define noERASER_VERBOSE

#include <string>
#include <cstring>
#include <numeric>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glibmm/i18n.h>

#include <2geom/bezier-utils.h>
#include <2geom/pathvector.h>

#include "context-fns.h"
#include "desktop-events.h"
#include "desktop-style.h"
#include "desktop.h"
#include "document-undo.h"
#include "document.h"
#include "layer-manager.h"
#include "layer-model.h"
#include "message-context.h"
#include "path-chemistry.h"
#include "rubberband.h"
#include "selection-chemistry.h"
#include "selection.h"
#include "splivarot.h"
#include "verbs.h"

#include "display/sp-canvas.h"
#include "display/canvas-arena.h"
#include "display/canvas-bpath.h"
#include "display/curve.h"

#include "include/macros.h"

#include "object/sp-clippath.h"
#include "object/sp-item-group.h"
#include "object/sp-path.h"
#include "object/sp-rect.h"
#include "object/sp-root.h"
#include "object/sp-shape.h"
#include "object/sp-text.h"
#include "object/sp-use.h"
#include "style.h"

#include "ui/pixmaps/cursor-eraser.xpm"

#include "svg/svg.h"

#include "ui/tools/eraser-tool.h"

using Inkscape::DocumentUndo;

#define ERC_RED_RGBA 0xff0000ff

#define TOLERANCE_ERASER 0.1

#define ERASER_EPSILON 0.5e-6
#define ERASER_EPSILON_START 0.5e-2
#define ERASER_VEL_START 1e-5

#define DRAG_MIN 0.0
#define DRAG_DEFAULT 1.0
#define DRAG_MAX 1.0

namespace Inkscape {
namespace UI {
namespace Tools {

const std::string& EraserTool::getPrefsPath() {
	return EraserTool::prefsPath;
}

const std::string EraserTool::prefsPath = "/tools/eraser";

EraserTool::EraserTool()
    : DynamicBase(cursor_eraser_xpm)
    , nowidth(false)
{
}

EraserTool::~EraserTool() = default;

void EraserTool::setup() {
    DynamicBase::setup();

    this->accumulated = new SPCurve();
    this->currentcurve = new SPCurve();

    this->cal1 = new SPCurve();
    this->cal2 = new SPCurve();

    this->currentshape = sp_canvas_item_new(desktop->getSketch(), SP_TYPE_CANVAS_BPATH, nullptr);

    sp_canvas_bpath_set_fill(SP_CANVAS_BPATH(this->currentshape), ERC_RED_RGBA, SP_WIND_RULE_EVENODD);
    sp_canvas_bpath_set_stroke(SP_CANVAS_BPATH(this->currentshape), 0x00000000, 1.0, SP_STROKE_LINEJOIN_MITER, SP_STROKE_LINECAP_BUTT);

    /* fixme: Cannot we cascade it to root more clearly? */
    g_signal_connect(G_OBJECT(this->currentshape), "event", G_CALLBACK(sp_desktop_root_handler), desktop);

/*
static ProfileFloatElement f_profile[PROFILE_FLOAT_SIZE] = {
    {"mass",0.02, 0.0, 1.0},
    {"wiggle",0.0, 0.0, 1.0},
    {"angle",30.0, -90.0, 90.0},
    {"thinning",0.1, -1.0, 1.0},
    {"tremor",0.0, 0.0, 1.0},
    {"flatness",0.9, 0.0, 1.0},
    {"cap_rounding",0.0, 0.0, 5.0}
};
*/

    sp_event_context_read(this, "mass");
    sp_event_context_read(this, "wiggle");
    sp_event_context_read(this, "angle");
    sp_event_context_read(this, "width");
    sp_event_context_read(this, "thinning");
    sp_event_context_read(this, "tremor");
    sp_event_context_read(this, "flatness");
    sp_event_context_read(this, "tracebackground");
    sp_event_context_read(this, "usepressure");
    sp_event_context_read(this, "usetilt");
    sp_event_context_read(this, "abs_width");
    sp_event_context_read(this, "cap_rounding");

    this->is_drawing = false;
    //TODO not sure why get 0.01 if slider width == 0, maybe a double/int problem

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    if (prefs->getBool("/tools/eraser/selcue", false) != 0) {
    	this->enableSelectionCue();
    }

    // TODO temp force:
    this->enableSelectionCue();
}

static double
flerp(double f0, double f1, double p)
{
    return f0 + ( f1 - f0 ) * p;
}

void EraserTool::reset(Geom::Point p) {
    this->last = this->cur = getNormalizedPoint(p);
    this->vel = Geom::Point(0,0);
    this->vel_max = 0;
    this->acc = Geom::Point(0,0);
    this->ang = Geom::Point(0,0);
    this->del = Geom::Point(0,0);
}

void EraserTool::extinput(GdkEvent *event) {
    if (gdk_event_get_axis (event, GDK_AXIS_PRESSURE, &this->pressure))
        this->pressure = CLAMP (this->pressure, ERC_MIN_PRESSURE, ERC_MAX_PRESSURE);
    else
        this->pressure = ERC_DEFAULT_PRESSURE;

    if (gdk_event_get_axis (event, GDK_AXIS_XTILT, &this->xtilt))
        this->xtilt = CLAMP (this->xtilt, ERC_MIN_TILT, ERC_MAX_TILT);
    else
        this->xtilt = ERC_DEFAULT_TILT;

    if (gdk_event_get_axis (event, GDK_AXIS_YTILT, &this->ytilt))
        this->ytilt = CLAMP (this->ytilt, ERC_MIN_TILT, ERC_MAX_TILT);
    else
        this->ytilt = ERC_DEFAULT_TILT;
}


bool EraserTool::apply(Geom::Point p) {
    Geom::Point n = getNormalizedPoint(p);

    /* Calculate mass and drag */
    double const mass = flerp(1.0, 160.0, this->mass);
    double const drag = flerp(0.0, 0.5, this->drag * this->drag);

    /* Calculate force and acceleration */
    Geom::Point force = n - this->cur;

    // If force is below the absolute threshold ERASER_EPSILON,
    // or we haven't yet reached ERASER_VEL_START (i.e. at the beginning of stroke)
    // _and_ the force is below the (higher) ERASER_EPSILON_START threshold,
    // discard this move. 
    // This prevents flips, blobs, and jerks caused by microscopic tremor of the tablet pen,
    // especially bothersome at the start of the stroke where we don't yet have the inertia to
    // smooth them out.
    if ( Geom::L2(force) < ERASER_EPSILON || (this->vel_max < ERASER_VEL_START && Geom::L2(force) < ERASER_EPSILON_START)) {
        return FALSE;
    }

    this->acc = force / mass;

    /* Calculate new velocity */
    this->vel += this->acc;

    if (Geom::L2(this->vel) > this->vel_max)
        this->vel_max = Geom::L2(this->vel);

    /* Calculate angle of drawing tool */

    double a1;
    if (this->usetilt) {
        // 1a. calculate nib angle from input device tilt:
        gdouble length = std::sqrt(this->xtilt*this->xtilt + this->ytilt*this->ytilt);;

        if (length > 0) {
            Geom::Point ang1 = Geom::Point(this->ytilt/length, this->xtilt/length);
            a1 = atan2(ang1);
        }
        else
            a1 = 0.0;
    }
    else {
        // 1b. fixed dc->angle (absolutely flat nib):
        double const radians = ( (this->angle - 90) / 180.0 ) * M_PI;
        Geom::Point ang1 = Geom::Point(-sin(radians),  cos(radians));
        a1 = atan2(ang1);
    }

    // 2. perpendicular to dc->vel (absolutely non-flat nib):
    gdouble const mag_vel = Geom::L2(this->vel);
    if ( mag_vel < ERASER_EPSILON ) {
        return FALSE;
    }
    Geom::Point ang2 = Geom::rot90(this->vel) / mag_vel;

    // 3. Average them using flatness parameter:
    // calculate angles
    double a2 = atan2(ang2);
    // flip a2 to force it to be in the same half-circle as a1
    bool flipped = false;
    if (fabs (a2-a1) > 0.5*M_PI) {
        a2 += M_PI;
        flipped = true;
    }
    // normalize a2
    if (a2 > M_PI)
        a2 -= 2*M_PI;
    if (a2 < -M_PI)
        a2 += 2*M_PI;
    // find the flatness-weighted bisector angle, unflip if a2 was flipped
    // FIXME: when dc->vel is oscillating around the fixed angle, the new_ang flips back and forth. How to avoid this?
    double new_ang = a1 + (1 - this->flatness) * (a2 - a1) - (flipped? M_PI : 0);

    // Try to detect a sudden flip when the new angle differs too much from the previous for the
    // current velocity; in that case discard this move
    double angle_delta = Geom::L2(Geom::Point (cos (new_ang), sin (new_ang)) - this->ang);
    if ( angle_delta / Geom::L2(this->vel) > 4000 ) {
        return FALSE;
    }

    // convert to point
    this->ang = Geom::Point (cos (new_ang), sin (new_ang));

//    g_print ("force %g  acc %g  vel_max %g  vel %g  a1 %g  a2 %g  new_ang %g\n", Geom::L2(force), Geom::L2(dc->acc), dc->vel_max, Geom::L2(dc->vel), a1, a2, new_ang);

    /* Apply drag */
    this->vel *= 1.0 - drag;

    /* Update position */
    this->last = this->cur;
    this->cur += this->vel;

    return TRUE;
}

void EraserTool::brush() {
    g_assert( this->npoints >= 0 && this->npoints < SAMPLING_SIZE );

    // How much velocity thins strokestyle
    double vel_thin = flerp (0, 160, this->vel_thin);

    // Influence of pressure on thickness
    double pressure_thick = (this->usepressure ? this->pressure : 1.0);

    // get the real brush point, not the same as pointer (affected by hatch tracking and/or mass
    // drag)
    Geom::Point brush = getViewPoint(this->cur);
    //Geom::Point brush_w = SP_EVENT_CONTEXT(dc)->desktop->d2w(brush); 

    double trace_thick = 1;

    double width = (pressure_thick * trace_thick - vel_thin * Geom::L2(this->vel)) * this->width;

    double tremble_left = 0, tremble_right = 0;
    if (this->tremor > 0) {
        // obtain two normally distributed random variables, using polar Box-Muller transform
        double x1, x2, w, y1, y2;
        do {
            x1 = 2.0 * g_random_double_range(0,1) - 1.0;
            x2 = 2.0 * g_random_double_range(0,1) - 1.0;
            w = x1 * x1 + x2 * x2;
        } while ( w >= 1.0 );
        w = sqrt( (-2.0 * log( w ) ) / w );
        y1 = x1 * w;
        y2 = x2 * w;

        // deflect both left and right edges randomly and independently, so that:
        // (1) dc->tremor=1 corresponds to sigma=1, decreasing dc->tremor narrows the bell curve;
        // (2) deflection depends on width, but is upped for small widths for better visual uniformity across widths;
        // (3) deflection somewhat depends on speed, to prevent fast strokes looking
        // comparatively smooth and slow ones excessively jittery
        tremble_left  = (y1)*this->tremor * (0.15 + 0.8*width) * (0.35 + 14*Geom::L2(this->vel));
        tremble_right = (y2)*this->tremor * (0.15 + 0.8*width) * (0.35 + 14*Geom::L2(this->vel));
    }

    if ( width < 0.02 * this->width ) {
        width = 0.02 * this->width;
    }

    double dezoomify_factor = 0.05 * 1000;
    if (!this->abs_width) {
        dezoomify_factor /= SP_EVENT_CONTEXT(this)->desktop->current_zoom();
    }

    Geom::Point del_left = dezoomify_factor * (width + tremble_left) * this->ang;
    Geom::Point del_right = dezoomify_factor * (width + tremble_right) * this->ang;

    this->point1[this->npoints] = brush + del_left;
    this->point2[this->npoints] = brush - del_right;
    
    if (this->nowidth) {
        this->point1[this->npoints] = Geom::middle_point(this->point1[this->npoints],this->point2[this->npoints]);
    }
    this->del = 0.5*(del_left + del_right);

    this->npoints++;
}

static void
sp_erc_update_toolbox (SPDesktop *desktop, const gchar *id, double value)
{
    desktop->setToolboxAdjustmentValue (id, value);
}

void EraserTool::cancel() {
    SPDesktop *desktop = SP_EVENT_CONTEXT(this)->desktop;
    this->dragging = FALSE;
    this->is_drawing = false;
    sp_canvas_item_ungrab(SP_CANVAS_ITEM(desktop->acetate));
            /* Remove all temporary line segments */
    for (auto i : this->segments)
        sp_canvas_item_destroy(SP_CANVAS_ITEM(i));
    this->segments.clear();
            /* reset accumulated curve */
            this->accumulated->reset();
            this->clear_current();
            if (this->repr) {
                this->repr = nullptr;
            }
}

bool EraserTool::root_handler(GdkEvent* event) {
    gint ret = FALSE;
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    gint eraser_mode = prefs->getInt("/tools/eraser/mode", 2);
    switch (event->type) {
        case GDK_BUTTON_PRESS:
            if (event->button.button == 1 && !this->space_panning) {
                if (Inkscape::have_viable_layer(desktop, defaultMessageContext()) == false) {
                    return TRUE;
                }

                Geom::Point const button_w(event->button.x, event->button.y);
                Geom::Point const button_dt(desktop->w2d(button_w));

                this->reset(button_dt);
                this->extinput(event);
                this->apply(button_dt);

                this->accumulated->reset();

                if (this->repr) {
                    this->repr = nullptr;
                }
                if ( eraser_mode == ERASER_MODE_DELETE ) {
                    Inkscape::Rubberband::get(desktop)->start(desktop, button_dt);
                    Inkscape::Rubberband::get(desktop)->setMode(RUBBERBAND_MODE_TOUCHPATH);
                }
                /* initialize first point */
                this->npoints = 0;

                sp_canvas_item_grab(SP_CANVAS_ITEM(desktop->acetate),
                                    ( GDK_KEY_PRESS_MASK |
                                      GDK_BUTTON_RELEASE_MASK |
                                      GDK_POINTER_MOTION_MASK |
                                      GDK_BUTTON_PRESS_MASK ),
                                    nullptr,
                                    event->button.time);

                ret = TRUE;

                desktop->canvas->forceFullRedrawAfterInterruptions(3);
                this->is_drawing = true;
            }
            break;

        case GDK_MOTION_NOTIFY: {
            Geom::Point const motion_w(event->motion.x, event->motion.y);
            Geom::Point motion_dt(desktop->w2d(motion_w)
            		);
            this->extinput(event);

            this->message_context->clear();

            if ( this->is_drawing && (event->motion.state & GDK_BUTTON1_MASK) && !this->space_panning) {
                this->dragging = TRUE;

                this->message_context->set(Inkscape::NORMAL_MESSAGE, _("<b>Drawing</b> an eraser stroke"));

                if (!this->apply(motion_dt)) {
                    ret = TRUE;
                    break;
                }

                if ( this->cur != this->last ) {
                    this->brush();
                    g_assert( this->npoints > 0 );
                    this->fit_and_split(false);
                }

                ret = TRUE;
            }
            if ( eraser_mode == ERASER_MODE_DELETE ) {
                this->accumulated->reset();
                Inkscape::Rubberband::get(desktop)->move(motion_dt);
            }
        }
        break;

    case GDK_BUTTON_RELEASE: {
        Geom::Point const motion_w(event->button.x, event->button.y);
        Geom::Point const motion_dt(desktop->w2d(motion_w));

        sp_canvas_item_ungrab(SP_CANVAS_ITEM(desktop->acetate));
        desktop->canvas->endForcedFullRedraws();
        this->is_drawing = false;

        if (this->dragging && event->button.button == 1 && !this->space_panning) {
            this->dragging = FALSE;

            this->apply(motion_dt);

            /* Remove all temporary line segments */
            for (auto i : this->segments)
                sp_canvas_item_destroy(SP_CANVAS_ITEM(i));
            this->segments.clear();

            /* Create object */
            this->fit_and_split(true);
            this->accumulate();
            this->set_to_accumulated(); // performs document_done

            /* reset accumulated curve */
            this->accumulated->reset();

            this->clear_current();
            if (this->repr) {
                this->repr = nullptr;
            }

            this->message_context->clear();
            ret = TRUE;
        }

        if (eraser_mode == ERASER_MODE_DELETE && Inkscape::Rubberband::get(desktop)->is_started()) {
            Inkscape::Rubberband::get(desktop)->stop();
        }
            
        break;
    }

    case GDK_KEY_PRESS:
        switch (get_latin_keyval (&event->key)) {
//        case GDK_KEY_Up:
//        case GDK_KEY_KP_Up:
//            if (!MOD__CTRL_ONLY(event)) {
//                this->angle += 5.0;

//                if (this->angle > 90.0) {
//                    this->angle = 90.0;
//                }
//                sp_erc_update_toolbox (desktop, "eraser-angle", this->angle);
//                ret = TRUE;
//            }
//            break;

//        case GDK_KEY_Down:
//        case GDK_KEY_KP_Down:
//            if (!MOD__CTRL_ONLY(event)) {
//                this->angle -= 5.0;

//                if (this->angle < -90.0) {
//                    this->angle = -90.0;
//                }

//                sp_erc_update_toolbox (desktop, "eraser-angle", this->angle);
//                ret = TRUE;
//            }
//            break;

        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
            if (!MOD__CTRL_ONLY(event)) {
                this->width += 0.01;

                if (this->width > 1.0) {
                    this->width = 1.0;
                }

                sp_erc_update_toolbox (desktop, "eraser-width", this->width * 100); // the same spinbutton is for alt+x
                ret = TRUE;
            }
            break;

        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
            if (!MOD__CTRL_ONLY(event)) {
                this->width -= 0.01;

                if (this->width < 0.01) {
                    this->width = 0.01;
                }

                sp_erc_update_toolbox (desktop, "eraser-width", this->width * 100);
                ret = TRUE;
            }
            break;

        case GDK_KEY_Home:
        case GDK_KEY_KP_Home:
            this->width = 0.01;
            sp_erc_update_toolbox (desktop, "eraser-width", this->width * 100);
            ret = TRUE;
            break;

        case GDK_KEY_End:
        case GDK_KEY_KP_End:
            this->width = 1.0;
            sp_erc_update_toolbox (desktop, "eraser-width", this->width * 100);
            ret = TRUE;
            break;

        case GDK_KEY_x:
        case GDK_KEY_X:
            if (MOD__ALT_ONLY(event)) {
                desktop->setToolboxFocusTo ("eraser-width");
                ret = TRUE;
            }
            break;

        case GDK_KEY_Escape:
            if ( eraser_mode == ERASER_MODE_DELETE ) {
                Inkscape::Rubberband::get(desktop)->stop();
            }
            if (this->is_drawing) {
                // if drawing, cancel, otherwise pass it up for deselecting
                this->cancel();
                ret = TRUE;
            }
            break;

        case GDK_KEY_z:
        case GDK_KEY_Z:
            if (MOD__CTRL_ONLY(event) && this->is_drawing) {
                // if drawing, cancel, otherwise pass it up for undo
                this->cancel();
                ret = TRUE;
            }
            break;

        default:
            break;
        }
        break;

    case GDK_KEY_RELEASE:
        switch (get_latin_keyval(&event->key)) {
            case GDK_KEY_Control_L:
            case GDK_KEY_Control_R:
                this->message_context->clear();
                break;

            default:
                break;
        }
        break;

    default:
        break;
    }

    if (!ret) {
    	ret = DynamicBase::root_handler(event);
    }

    return ret;
}

void EraserTool::clear_current() {
    // reset bpath
    sp_canvas_bpath_set_bpath(SP_CANVAS_BPATH(this->currentshape), nullptr);

    // reset curve
    this->currentcurve->reset();
    this->cal1->reset();
    this->cal2->reset();

    // reset points
    this->npoints = 0;
}

void EraserTool::set_to_accumulated() {
    bool workDone = false;
    SPDocument *document = this->desktop->doc();
    if (!this->accumulated->is_empty()) {
        if (!this->repr) {
            /* Create object */
            Inkscape::XML::Document *xml_doc = this->desktop->doc()->getReprDoc();
            Inkscape::XML::Node *repr = xml_doc->createElement("svg:path");

            /* Set style */
            sp_desktop_apply_style_tool (this->desktop, repr, "/tools/eraser", false);

            this->repr = repr;
        }
        SPObject * top_layer = desktop->currentRoot();
        SPItem *item_repr = SP_ITEM(top_layer->appendChildRepr(this->repr));
        Inkscape::GC::release(this->repr);
        item_repr->updateRepr();
        Geom::PathVector pathv = this->accumulated->get_pathvector() * this->desktop->dt2doc();
        pathv *= item_repr->i2doc_affine().inverse();
        gchar *str = sp_svg_write_path(pathv);
        g_assert( str != nullptr );
        this->repr->setAttribute("d", str);
        g_free(str);
        Geom::OptRect eraserBbox;
        if ( this->repr ) {
            bool wasSelection = false;
            Inkscape::Selection *selection = this->desktop->getSelection();
            Inkscape::Preferences *prefs = Inkscape::Preferences::get();
            gint eraser_mode = prefs->getInt("/tools/eraser/mode", ERASER_MODE_CLIP);
            Inkscape::XML::Document *xml_doc = this->desktop->doc()->getReprDoc();

            SPItem* acid = SP_ITEM(this->desktop->doc()->getObjectByRepr(this->repr));
            eraserBbox = acid->documentVisualBounds();
            std::vector<SPItem*> remainingItems;
            std::vector<SPItem*> toWorkOn;
            if (selection->isEmpty()) {
                if (eraser_mode  == ERASER_MODE_CUT || eraser_mode  == ERASER_MODE_CLIP) {
                    toWorkOn = document->getItemsPartiallyInBox(this->desktop->dkey, *eraserBbox, false, false, false, true);
                } else {
                    Inkscape::Rubberband *r = Inkscape::Rubberband::get(this->desktop);
                    toWorkOn = document->getItemsAtPoints(this->desktop->dkey, r->getPoints());
                }
                toWorkOn.erase(std::remove(toWorkOn.begin(), toWorkOn.end(), acid), toWorkOn.end());
            } else {
                if (eraser_mode == ERASER_MODE_DELETE) {
                    Inkscape::Rubberband *r = Inkscape::Rubberband::get(this->desktop);
                    std::vector<SPItem*> touched;
                    touched = document->getItemsAtPoints(this->desktop->dkey, r->getPoints());
                    for (std::vector<SPItem*>::const_iterator i = touched.begin();i!=touched.end();++i) {
                        if(selection->includes(*i)){
                            toWorkOn.push_back((*i));
                        }
                    }
                } else {
                    toWorkOn.insert(toWorkOn.end(), selection->items().begin(), selection->items().end());
                }
                wasSelection = true;
            }

            if ( !toWorkOn.empty() ) {
                if (eraser_mode  == ERASER_MODE_CUT) {
                    for (std::vector<SPItem*>::const_iterator i = toWorkOn.begin(); i != toWorkOn.end(); ++i){
                        SPItem *item = *i;
                        SPUse *use = dynamic_cast<SPUse *>(item);
                        if (SP_IS_PATH(item) && SP_PATH(item)->nodesInPath () == 2){
                            SPItem *item = *i;
                            item->deleteObject(true);
                            workDone = true;
                        } else if (SP_IS_GROUP(item) || use ) {
                            /*Do nothing*/
                        } else {
                            Geom::OptRect bbox = item->documentVisualBounds();
                            if (bbox && bbox->intersects(*eraserBbox)) {
                                Inkscape::XML::Node* dup = this->repr->duplicate(xml_doc);
                                this->repr->parent()->appendChild(dup);
                                Inkscape::GC::release(dup); // parent takes over
                                selection->set(dup);
                                if (!this->nowidth) {
                                    selection->pathUnion(true);
                                }
                                selection->add(item);
                                if(item->style->fill_rule.value == SP_WIND_RULE_EVENODD){
                                    SPCSSAttr *css = sp_repr_css_attr_new();
                                    sp_repr_css_set_property(css, "fill-rule", "evenodd");
                                    sp_desktop_set_style(this->desktop, css);
                                    sp_repr_css_attr_unref(css);
                                    css = nullptr;
                                }
                                if (this->nowidth) {
                                    selection->pathCut(true);
                                } else {
                                    selection->pathDiff(true);
                                }
                                workDone = true; // TODO set this only if something was cut.
                                bool break_apart = prefs->getBool("/tools/eraser/break_apart", false);
                                if(!break_apart){
                                    selection->combine(true);
                                } else {
                                    if(!this->nowidth){
                                        selection->breakApart(true);
                                    }
                                }
                                if ( !selection->isEmpty() ) {
                                    // If the item was not completely erased, track the new remainder.
                                    std::vector<SPItem*> nowSel(selection->items().begin(), selection->items().end());
                                    for (std::vector<SPItem*>::const_iterator i2 = nowSel.begin();i2!=nowSel.end();++i2) {
                                        remainingItems.push_back(*i2);
                                    }
                                }
                            } else {
                                remainingItems.push_back(item);
                            }
                        }
                    }
                } else if (eraser_mode == ERASER_MODE_CLIP) {
                    if (!this->nowidth) {
                        remainingItems.clear();
                        for (std::vector<SPItem*>::const_iterator i = toWorkOn.begin(); i != toWorkOn.end(); ++i){
                            selection->clear();
                            SPItem *item = *i;
                            Geom::OptRect bbox = item->documentVisualBounds();
                            Inkscape::XML::Document *xml_doc = this->desktop->doc()->getReprDoc();
                            Inkscape::XML::Node* dup = this->repr->duplicate(xml_doc);
                            this->repr->parent()->appendChild(dup);
                            Inkscape::GC::release(dup); // parent takes over
                            selection->set(dup);
                            selection->pathUnion(true);
                            if (bbox && bbox->intersects(*eraserBbox)) {
                                SPClipPath *clip_path = item->clip_ref->getObject();
                                if (clip_path) {
                                    std::vector<SPItem*> selected;
                                    selected.push_back(SP_ITEM(clip_path->firstChild()));
                                    std::vector<Inkscape::XML::Node*> to_select;
                                    std::vector<SPItem*> items(selected);
                                    sp_item_list_to_curves(items, selected, to_select);
                                    Inkscape::XML::Node * clip_data = SP_ITEM(clip_path->firstChild())->getRepr();
                                    if (!clip_data && !to_select.empty()) {
                                        clip_data = *(to_select.begin());
                                    }
                                    if (clip_data) {
                                        Inkscape::XML::Node *dup_clip = clip_data->duplicate(xml_doc);
                                        if (dup_clip) {
                                            SPItem * dup_clip_obj = SP_ITEM(item_repr->parent->appendChildRepr(dup_clip));
                                            Inkscape::GC::release(dup_clip);
                                            if (dup_clip_obj) {
                                                dup_clip_obj->transform *=
                                                    item->getRelativeTransform(SP_ITEM(item_repr->parent));
                                                dup_clip_obj->updateRepr();
                                                clip_path->deleteObject(true);
                                                selection->raiseToTop(true);
                                                selection->add(dup_clip);
                                                selection->pathDiff(true);
                                                //SPItem * clip = SP_ITEM(*(selection->items().begin()));
                                            }
                                        }
                                    }
                                } else {
                                    Inkscape::XML::Node *rect_repr = xml_doc->createElement("svg:rect");
                                    sp_desktop_apply_style_tool (this->desktop, rect_repr, "/tools/eraser", false);
                                    SPRect * rect = SP_RECT(item_repr->parent->appendChildRepr(rect_repr));
                                    Inkscape::GC::release(rect_repr);
                                    rect->setPosition (bbox->left(), bbox->top(), bbox->width(), bbox->height());
                                    rect->transform = SP_ITEM(rect->parent)->i2doc_affine().inverse();

                                    rect->updateRepr();
                                    rect->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
                                    selection->raiseToTop(true);
                                    selection->add(rect);
                                    selection->pathDiff(true);
                                }
                                selection->raiseToTop(true);
                                selection->add(item);
                                selection->setMask(true, false, true);
                            } else {
                                SPItem *erase_clip = selection->singleItem();
                                if (erase_clip) {
                                    erase_clip->deleteObject(true);
                                }
                            }
                            workDone = true;
                            selection->clear();
                            if (wasSelection) {
                                remainingItems.push_back(item);
                            }
                        }
                    }
                } else {
                    for (std::vector<SPItem*>::const_iterator i = toWorkOn.begin();i!=toWorkOn.end();++i) {
                        SPItem *item = *i;
                        item->deleteObject(true);
                        workDone = true;
                    }
                }

                if (eraser_mode == ERASER_MODE_DELETE) {
                    selection->deleteItems();
                    remainingItems.clear();
                }

                selection->clear();

                if ( wasSelection ) {
                    if ( !remainingItems.empty() ) {
                        selection->add(remainingItems.begin(), remainingItems.end());
                    }
                }
            }
            // Remove the eraser stroke itself:
            sp_repr_unparent( this->repr );
            this->repr = nullptr;
        }
    } else {
        if (this->repr) {
            sp_repr_unparent(this->repr);
            this->repr = nullptr;
        }
    }
    if ( workDone ) {
        DocumentUndo::done(document, SP_VERB_CONTEXT_ERASER, _("Draw eraser stroke"));
    } else {
        DocumentUndo::cancel(document);
    }
}

static void
add_cap(SPCurve *curve,
        Geom::Point const &pre, Geom::Point const &from,
        Geom::Point const &to, Geom::Point const &post,
        double rounding)
{
    Geom::Point vel = rounding * Geom::rot90( to - from ) / sqrt(2.0);
    double mag = Geom::L2(vel);

    Geom::Point v_in = from - pre;
    double mag_in = Geom::L2(v_in);

    if ( mag_in > ERASER_EPSILON ) {
        v_in = mag * v_in / mag_in;
    } else {
        v_in = Geom::Point(0, 0);
    }

    Geom::Point v_out = to - post;
    double mag_out = Geom::L2(v_out);

    if ( mag_out > ERASER_EPSILON ) {
        v_out = mag * v_out / mag_out;
    } else {
        v_out = Geom::Point(0, 0);
    }

    if ( Geom::L2(v_in) > ERASER_EPSILON || Geom::L2(v_out) > ERASER_EPSILON ) {
        curve->curveto(from + v_in, to + v_out, to);
    }
}

void EraserTool::accumulate() {
    // construct a crude outline of the eraser's path.
    // this desperately needs to be rewritten to use the path outliner...
    if ( !this->cal1->is_empty() && !this->cal2->is_empty() ) {
        this->accumulated->reset(); /*  Is this required ?? */
        SPCurve *rev_cal2 = this->cal2->create_reverse();

        g_assert(this->cal1->get_segment_count() > 0);
        g_assert(rev_cal2->get_segment_count() > 0);
        g_assert( ! this->cal1->first_path()->closed() );
        g_assert( ! rev_cal2->first_path()->closed() );

        Geom::BezierCurve const * dc_cal1_firstseg  = dynamic_cast<Geom::BezierCurve const *>( this->cal1->first_segment() );
        Geom::BezierCurve const * rev_cal2_firstseg = dynamic_cast<Geom::BezierCurve const *>( rev_cal2->first_segment() );
        Geom::BezierCurve const * dc_cal1_lastseg   = dynamic_cast<Geom::BezierCurve const *>( this->cal1->last_segment() );
        Geom::BezierCurve const * rev_cal2_lastseg  = dynamic_cast<Geom::BezierCurve const *>( rev_cal2->last_segment() );

        g_assert( dc_cal1_firstseg );
        g_assert( rev_cal2_firstseg );
        g_assert( dc_cal1_lastseg );
        g_assert( rev_cal2_lastseg );

        this->accumulated->append(this->cal1, FALSE);
        if(!this->nowidth) {
            add_cap(this->accumulated,
                    dc_cal1_lastseg->finalPoint() - dc_cal1_lastseg->unitTangentAt(1),
                    dc_cal1_lastseg->finalPoint(),
                    rev_cal2_firstseg->initialPoint(),
                    rev_cal2_firstseg->initialPoint() + rev_cal2_firstseg->unitTangentAt(0),
                    this->cap_rounding);

            this->accumulated->append(rev_cal2, TRUE);

            add_cap(this->accumulated,
                    rev_cal2_lastseg->finalPoint() - rev_cal2_lastseg->unitTangentAt(1),
                    rev_cal2_lastseg->finalPoint(),
                    dc_cal1_firstseg->initialPoint(),
                    dc_cal1_firstseg->initialPoint() + dc_cal1_firstseg->unitTangentAt(0),
                    this->cap_rounding);

            this->accumulated->closepath();
        }

        rev_cal2->unref();

        this->cal1->reset();
        this->cal2->reset();
    }
}

static double square(double const x)
{
    return x * x;
}

void EraserTool::fit_and_split(bool release) {
    double const tolerance_sq = square( desktop->w2d().descrim() * TOLERANCE_ERASER );
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    this->nowidth = prefs->getDouble( "/tools/eraser/width", 1) == 0;

#ifdef ERASER_VERBOSE
    g_print("[F&S:R=%c]", release?'T':'F');
#endif

    if (!( this->npoints > 0 && this->npoints < SAMPLING_SIZE ))
        return; // just clicked

    if ( this->npoints == SAMPLING_SIZE - 1 || release ) {
#define BEZIER_SIZE       4
#define BEZIER_MAX_BEZIERS  8
#define BEZIER_MAX_LENGTH ( BEZIER_SIZE * BEZIER_MAX_BEZIERS )

#ifdef ERASER_VERBOSE
        g_print("[F&S:#] this->npoints:%d, release:%s\n",
                dc->npoints, release ? "TRUE" : "FALSE");
#endif

        /* Current eraser */
        if ( this->cal1->is_empty() || this->cal2->is_empty() ) {
            /* dc->npoints > 0 */
            /* g_print("erasers(1|2) reset\n"); */
            this->cal1->reset();
            this->cal2->reset();

            this->cal1->moveto(this->point1[0]);
            this->cal2->moveto(this->point2[0]);
        }

        Geom::Point b1[BEZIER_MAX_LENGTH];
        gint const nb1 = Geom::bezier_fit_cubic_r(b1, this->point1, this->npoints, tolerance_sq, BEZIER_MAX_BEZIERS);
        g_assert( nb1 * BEZIER_SIZE <= gint(G_N_ELEMENTS(b1)) );

        Geom::Point b2[BEZIER_MAX_LENGTH];
        gint const nb2 = Geom::bezier_fit_cubic_r(b2, this->point2, this->npoints, tolerance_sq, BEZIER_MAX_BEZIERS);
        g_assert( nb2 * BEZIER_SIZE <= gint(G_N_ELEMENTS(b2)) );

        if ( nb1 != -1 && nb2 != -1 ) {
            /* Fit and draw and reset state */
#ifdef ERASER_VERBOSE
            g_print("nb1:%d nb2:%d\n", nb1, nb2);
#endif

            /* CanvasShape */
            if (! release) {
                this->currentcurve->reset();
                this->currentcurve->moveto(b1[0]);

                for (Geom::Point *bp1 = b1; bp1 < b1 + BEZIER_SIZE * nb1; bp1 += BEZIER_SIZE) {
                    this->currentcurve->curveto(bp1[1], bp1[2], bp1[3]);
                }

                this->currentcurve->lineto(b2[BEZIER_SIZE*(nb2-1) + 3]);

                for (Geom::Point *bp2 = b2 + BEZIER_SIZE * ( nb2 - 1 ); bp2 >= b2; bp2 -= BEZIER_SIZE) {
                    this->currentcurve->curveto(bp2[2], bp2[1], bp2[0]);
                }

                // FIXME: this->segments is always NULL at this point??
                if (this->segments.empty()) { // first segment
                    add_cap(this->currentcurve, b2[1], b2[0], b1[0], b1[1], this->cap_rounding);
                }

                this->currentcurve->closepath();
                sp_canvas_bpath_set_bpath(SP_CANVAS_BPATH(this->currentshape), this->currentcurve, true);
            }

            /* Current eraser */
            for (Geom::Point *bp1 = b1; bp1 < b1 + BEZIER_SIZE * nb1; bp1 += BEZIER_SIZE) {
                this->cal1->curveto(bp1[1], bp1[2], bp1[3]);
            }

            for (Geom::Point *bp2 = b2; bp2 < b2 + BEZIER_SIZE * nb2; bp2 += BEZIER_SIZE) {
                this->cal2->curveto(bp2[1], bp2[2], bp2[3]);
            }
        } else {
            /* fixme: ??? */
#ifdef ERASER_VERBOSE
            g_print("[fit_and_split] failed to fit-cubic.\n");
#endif
            this->draw_temporary_box();

            for (gint i = 1; i < this->npoints; i++) {
                this->cal1->lineto(this->point1[i]);
            }

            for (gint i = 1; i < this->npoints; i++) {
                this->cal2->lineto(this->point2[i]);
            }
        }

        /* Fit and draw and copy last point */
#ifdef ERASER_VERBOSE
        g_print("[%d]Yup\n", this->npoints);
#endif
        if (!release) {
            gint eraser_mode = prefs->getInt("/tools/eraser/mode",2);
            g_assert(!this->currentcurve->is_empty());

            SPCanvasItem *cbp = sp_canvas_item_new(desktop->getSketch(), SP_TYPE_CANVAS_BPATH, nullptr);
            SPCurve *curve = this->currentcurve->copy();
            sp_canvas_bpath_set_bpath(SP_CANVAS_BPATH (cbp), curve, true);
            curve->unref();

            guint32 fillColor = sp_desktop_get_color_tool (desktop, "/tools/eraser", true);
            //guint32 strokeColor = sp_desktop_get_color_tool (desktop, "/tools/eraser", false);
            double opacity = sp_desktop_get_master_opacity_tool (desktop, "/tools/eraser");
            double fillOpacity = sp_desktop_get_opacity_tool (desktop, "/tools/eraser", true);
            //double strokeOpacity = sp_desktop_get_opacity_tool (desktop, "/tools/eraser", false);
            sp_canvas_bpath_set_fill(SP_CANVAS_BPATH(cbp), ((fillColor & 0xffffff00) | SP_COLOR_F_TO_U(opacity*fillOpacity)), SP_WIND_RULE_EVENODD);
            //on second thougtht don't do stroke yet because we don't have stoke-width yet and because stoke appears between segments while drawing
            //sp_canvas_bpath_set_stroke(SP_CANVAS_BPATH(cbp), ((strokeColor & 0xffffff00) | SP_COLOR_F_TO_U(opacity*strokeOpacity)), 1.0, SP_STROKE_LINEJOIN_MITER, SP_STROKE_LINECAP_BUTT);
            sp_canvas_bpath_set_stroke(SP_CANVAS_BPATH(cbp), 0x00000000, 1.0, SP_STROKE_LINEJOIN_MITER, SP_STROKE_LINECAP_BUTT);
            /* fixme: Cannot we cascade it to root more clearly? */
            g_signal_connect(G_OBJECT(cbp), "event", G_CALLBACK(sp_desktop_root_handler), desktop);

            this->segments.push_back(cbp);

            if (eraser_mode == ERASER_MODE_DELETE) {
                sp_canvas_item_hide(cbp);
                sp_canvas_item_hide(this->currentshape);
            }
        }

        this->point1[0] = this->point1[this->npoints - 1];
        this->point2[0] = this->point2[this->npoints - 1];
        this->npoints = 1;
    } else {
        this->draw_temporary_box();
    }
}

void EraserTool::draw_temporary_box() {
    this->currentcurve->reset();

    this->currentcurve->moveto(this->point1[this->npoints-1]);

    for (gint i = this->npoints-2; i >= 0; i--) {
        this->currentcurve->lineto(this->point1[i]);
    }

    for (gint i = 0; i < this->npoints; i++) {
        this->currentcurve->lineto(this->point2[i]);
    }

    if (this->npoints >= 2) {
        add_cap(this->currentcurve, this->point2[this->npoints-2], this->point2[this->npoints-1], this->point1[this->npoints-1], this->point1[this->npoints-2], this->cap_rounding);
    }

    this->currentcurve->closepath();
    sp_canvas_bpath_set_bpath(SP_CANVAS_BPATH(this->currentshape), this->currentcurve, true);
}

}
}
}

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
