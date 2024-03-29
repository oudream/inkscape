// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * SPKnot implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Abhishek Sharma
 *
 * Copyright (C) 1999-2005 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifdef HAVE_CONFIG_H
#endif
#include <gdk/gdkkeysyms.h>
#include <glibmm/i18n.h>
#include "display/sodipodi-ctrl.h"
#include "desktop.h"

#include "knot.h"
#include "knot-ptr.h"
#include "document.h"
#include "document-undo.h"
#include "message-stack.h"
#include "message-context.h"
#include "ui/tools/node-tool.h"
#include <gtk/gtk.h>

using Inkscape::DocumentUndo;

#define KNOT_EVENT_MASK (GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | \
             GDK_POINTER_MOTION_MASK | \
             GDK_POINTER_MOTION_HINT_MASK | \
             GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK)

const gchar *nograbenv = getenv("INKSCAPE_NO_GRAB");
static bool nograb = (nograbenv && *nograbenv && (*nograbenv != '0'));

static bool grabbed = false;
static bool moved = false;

static gint xp = 0, yp = 0; // where drag started
static gint tolerance = 0;
static bool within_tolerance = false;

static bool transform_escaped = false; // true iff resize or rotate was cancelled by esc.

void knot_ref(SPKnot* knot) {
    knot->ref_count++;
}

void knot_unref(SPKnot* knot) {
    if (--knot->ref_count < 1) {
        delete knot;
    }
}


static int sp_knot_handler(SPCanvasItem *item, GdkEvent *event, SPKnot *knot);

SPKnot::SPKnot(SPDesktop *desktop, gchar const *tip)
    : ref_count(1)
{
    this->desktop = nullptr;
    this->item = nullptr;
    this->owner = nullptr;
    this->flags = 0;

    this->size = 8;
    this->angle = 0;
    this->pos = Geom::Point(0, 0);
    this->grabbed_rel_pos = Geom::Point(0, 0);
    this->anchor = SP_ANCHOR_CENTER;
    this->shape = SP_KNOT_SHAPE_SQUARE;
    this->mode = SP_KNOT_MODE_XOR;
    this->tip = nullptr;
    this->_event_handler_id = 0;
    this->pressure = 0;

    this->fill[SP_KNOT_STATE_NORMAL] = 0xffffff00;
    this->fill[SP_KNOT_STATE_MOUSEOVER] = 0xff0000ff;
    this->fill[SP_KNOT_STATE_DRAGGING] = 0xff0000ff;
    this->fill[SP_KNOT_STATE_SELECTED] = 0x0000ffff;

    this->stroke[SP_KNOT_STATE_NORMAL] = 0x01000000;
    this->stroke[SP_KNOT_STATE_MOUSEOVER] = 0x01000000;
    this->stroke[SP_KNOT_STATE_DRAGGING] = 0x01000000;
    this->stroke[SP_KNOT_STATE_SELECTED] = 0x01000000;

    this->image[SP_KNOT_STATE_NORMAL] = nullptr;
    this->image[SP_KNOT_STATE_MOUSEOVER] = nullptr;
    this->image[SP_KNOT_STATE_DRAGGING] = nullptr;
    this->image[SP_KNOT_STATE_SELECTED] = nullptr;
    
    this->cursor[SP_KNOT_STATE_NORMAL] = nullptr;
    this->cursor[SP_KNOT_STATE_MOUSEOVER] = nullptr;
    this->cursor[SP_KNOT_STATE_DRAGGING] = nullptr;
    this->cursor[SP_KNOT_STATE_SELECTED] = nullptr;

    this->saved_cursor = nullptr;
    this->pixbuf = nullptr;


    this->desktop = desktop;
    this->flags = SP_KNOT_VISIBLE;

    if (tip) {
        this->tip = g_strdup (tip);
    }

    this->item = sp_canvas_item_new(desktop->getControls(),
                                    SP_TYPE_CTRL,
                                    "anchor", SP_ANCHOR_CENTER,
                                    "size", 9,
                                    "angle", 0.0,
                                    "filled", TRUE,
                                    "fill_color", 0xffffff00,
                                    "stroked", TRUE,
                                    "stroke_color", 0x01000000,
                                    "mode", SP_KNOT_MODE_XOR,
                                    NULL);

    this->_event_handler_id = g_signal_connect(G_OBJECT(this->item), "event",
                                                 G_CALLBACK(sp_knot_handler), this);
    knot_created_callback(this);
}

SPKnot::~SPKnot() {
    auto display = gdk_display_get_default();
    auto seat    = gdk_display_get_default_seat(display);
    auto device  = gdk_seat_get_pointer(seat);

    if ((this->flags & SP_KNOT_GRABBED) && gdk_display_device_is_grabbed(display, device)) {
        // This happens e.g. when deleting a node in node tool while dragging it
        gdk_seat_ungrab(seat);
    }

    if (this->_event_handler_id > 0) {
        g_signal_handler_disconnect(G_OBJECT (this->item), this->_event_handler_id);
        this->_event_handler_id = 0;
    }

    if (this->item) {
        sp_canvas_item_destroy(this->item);
        this->item = nullptr;
    }

    for (auto & i : this->cursor) {
        if (i) {
            g_object_unref(i);
            i = nullptr;
        }
    }

    if (this->tip) {
        g_free(this->tip);
        this->tip = nullptr;
    }

    // FIXME: cannot snap to destroyed knot (lp:1309050)
    //sp_event_context_discard_delayed_snap_event(this->desktop->event_context);
    knot_deleted_callback(this);
}

void SPKnot::startDragging(Geom::Point const &p, gint x, gint y, guint32 etime) {
    // save drag origin
    xp = x;
    yp = y;
    within_tolerance = true;

    this->grabbed_rel_pos = p - this->pos;
    this->drag_origin = this->pos;

    if (!nograb) {
        sp_canvas_item_grab(this->item, KNOT_EVENT_MASK, this->cursor[SP_KNOT_STATE_DRAGGING], etime);
    }
    this->setFlag(SP_KNOT_GRABBED, TRUE);

    grabbed = TRUE;
}

void SPKnot::selectKnot(bool select){
    setFlag(SP_KNOT_SELECTED, select);
}

/**
 * Called to handle events on knots.
 */
static int sp_knot_handler(SPCanvasItem */*item*/, GdkEvent *event, SPKnot *knot)
{
    g_assert(knot != nullptr);
    g_assert(SP_IS_KNOT(knot));

    /* Run client universal event handler, if present */
    bool consumed = knot->event_signal.emit(knot, event);

    if (consumed) {
        return true;
    }

    bool key_press_event_unconsumed = FALSE;

    knot_ref(knot);

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    tolerance = prefs->getIntLimited("/options/dragtolerance/value", 0, 0, 100);

    switch (event->type) {
    case GDK_2BUTTON_PRESS:
        if (event->button.button == 1) {
            knot->doubleclicked_signal.emit(knot, event->button.state);

            grabbed = FALSE;
            moved = FALSE;
            consumed = TRUE;
        }
        break;
    case GDK_BUTTON_PRESS:
        if ((event->button.button == 1) && knot->desktop && knot->desktop->event_context && !knot->desktop->event_context->space_panning) {
            Geom::Point const p = knot->desktop->w2d(Geom::Point(event->button.x, event->button.y));
            knot->startDragging(p, (gint) event->button.x, (gint) event->button.y, event->button.time);
            knot->mousedown_signal.emit(knot, event->button.state);
            consumed = TRUE;
        }
        break;
    case GDK_BUTTON_RELEASE:
        if (event->button.button == 1 && knot->desktop && knot->desktop->event_context && !knot->desktop->event_context->space_panning) {
            // If we have any pending snap event, then invoke it now
            if (knot->desktop->event_context->_delayed_snap_event) {
                sp_event_context_snap_watchdog_callback(knot->desktop->event_context->_delayed_snap_event);
            }
            sp_event_context_discard_delayed_snap_event(knot->desktop->event_context);
            knot->pressure = 0;

            if (transform_escaped) {
                transform_escaped = false;
                consumed = TRUE;
            } else {
                knot->setFlag(SP_KNOT_GRABBED, FALSE);

                if (!nograb) {
                    sp_canvas_item_ungrab(knot->item);
                }

                if (moved) {
                    knot->setFlag(SP_KNOT_DRAGGING, FALSE);
                    knot->ungrabbed_signal.emit(knot, event->button.state);
                } else {
                    knot->click_signal.emit(knot, event->button.state);
                }

                grabbed = FALSE;
                moved = FALSE;
                consumed = TRUE;
            }
        }
        Inkscape::UI::Tools::sp_update_helperpath();
        break;
    case GDK_MOTION_NOTIFY:
        if (grabbed && knot->desktop && knot->desktop->event_context && !knot->desktop->event_context->space_panning) {
            consumed = TRUE;

            if ( within_tolerance
                 && ( abs( (gint) event->motion.x - xp ) < tolerance )
                 && ( abs( (gint) event->motion.y - yp ) < tolerance ) ) {
                break; // do not drag if we're within tolerance from origin
            }

            // Once the user has moved farther than tolerance from the original location
            // (indicating they intend to move the object, not click), then always process the
            // motion notify coordinates as given (no snapping back to origin)
            within_tolerance = false;

            if (gdk_event_get_axis (event, GDK_AXIS_PRESSURE, &knot->pressure)) {
                knot->pressure = CLAMP (knot->pressure, 0, 1);
            } else {
                knot->pressure = 0.5;
            }

            if (!moved) {
                knot->setFlag(SP_KNOT_DRAGGING, TRUE);
                knot->grabbed_signal.emit(knot, event->button.state);
            }

            sp_event_context_snap_delay_handler(knot->desktop->event_context, nullptr, knot, (GdkEventMotion *)event, Inkscape::UI::Tools::DelayedSnapEvent::KNOT_HANDLER);
            sp_knot_handler_request_position(event, knot);
            moved = TRUE;
        }
        Inkscape::UI::Tools::sp_update_helperpath();
        break;
    case GDK_ENTER_NOTIFY:
        knot->setFlag(SP_KNOT_MOUSEOVER, TRUE);
        knot->setFlag(SP_KNOT_GRABBED, FALSE);

        if (knot->tip && knot->desktop && knot->desktop->event_context) {
            knot->desktop->event_context->defaultMessageContext()->set(Inkscape::NORMAL_MESSAGE, knot->tip);
        }

        grabbed = FALSE;
        moved = FALSE;
        consumed = TRUE;
        break;
    case GDK_LEAVE_NOTIFY:
        knot->setFlag(SP_KNOT_MOUSEOVER, FALSE);
        knot->setFlag(SP_KNOT_GRABBED, FALSE);

        if (knot->tip && knot->desktop && knot->desktop->event_context) {
            knot->desktop->event_context->defaultMessageContext()->clear();
        }

        grabbed = FALSE;
        moved = FALSE;
        consumed = TRUE;
        break;
    case GDK_KEY_PRESS: // keybindings for knot
        switch (Inkscape::UI::Tools::get_latin_keyval(&event->key)) {
            case GDK_KEY_Escape:
                knot->setFlag(SP_KNOT_GRABBED, FALSE);

                if (!nograb) {
                    sp_canvas_item_ungrab(knot->item);
                }

                if (moved) {
                    knot->setFlag(SP_KNOT_DRAGGING, FALSE);

                    knot->ungrabbed_signal.emit(knot, event->button.state);

                    DocumentUndo::undo(knot->desktop->getDocument());
                    knot->desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Node or handle drag canceled."));
                    transform_escaped = true;
                    consumed = TRUE;
                }

                grabbed = FALSE;
                moved = FALSE;

                sp_event_context_discard_delayed_snap_event(knot->desktop->event_context);
                break;
            default:
                consumed = FALSE;
                key_press_event_unconsumed = TRUE;
                break;
        }
        break;
    default:
        break;
    }

    knot_unref(knot);

    if (key_press_event_unconsumed) {
        return false; // e.g. in case "%" was pressed to toggle snapping, or Q for quick zoom (while dragging a handle)
    } else {
        return  consumed || grabbed;
    }
}

void sp_knot_handler_request_position(GdkEvent *event, SPKnot *knot) {
    Geom::Point const motion_w(event->motion.x, event->motion.y);
    Geom::Point const motion_dt = knot->desktop->w2d(motion_w);
    Geom::Point p = motion_dt - knot->grabbed_rel_pos;

    knot->requestPosition(p, event->motion.state);
    knot->desktop->scroll_to_point (motion_dt);
    knot->desktop->set_coordinate_status(knot->pos); // display the coordinate of knot, not cursor - they may be different!

    if (event->motion.state & GDK_BUTTON1_MASK) {
        Inkscape::UI::Tools::gobble_motion_events(GDK_BUTTON1_MASK);
    }
}

void SPKnot::show() {
    this->setFlag(SP_KNOT_VISIBLE, TRUE);
}

void SPKnot::hide() {
    this->setFlag(SP_KNOT_VISIBLE, FALSE);
}

void SPKnot::requestPosition(Geom::Point const &p, guint state) {
    bool done = this->request_signal.emit(this, &const_cast<Geom::Point&>(p), state);

    /* If user did not complete, we simply move knot to new position */
    if (!done) {
        this->setPosition(p, state);
    }
}

void SPKnot::setPosition(Geom::Point const &p, guint state) {
    this->pos = p;

    if (this->item) {
        SP_CTRL(this->item)->moveto(p);
    }

    this->moved_signal.emit(this, p, state);
}

void SPKnot::moveto(Geom::Point const &p) {
    this->pos = p;

    if (this->item) {
        SP_CTRL(this->item)->moveto(p);
    }
}

Geom::Point SPKnot::position() const {
    return this->pos;
}

void SPKnot::setFlag(guint flag, bool set) {
    if (set) {
        this->flags |= flag;
    } else {
        this->flags &= ~flag;
    }

    switch (flag) {
    case SP_KNOT_VISIBLE:
            if (set) {
                sp_canvas_item_show(this->item);
            } else {
                sp_canvas_item_hide(this->item);
            }
            break;
    case SP_KNOT_MOUSEOVER:
    case SP_KNOT_DRAGGING:
    case SP_KNOT_SELECTED:
            this->_setCtrlState();
            break;
    case SP_KNOT_GRABBED:
            break;
    default:
            g_assert_not_reached();
            break;
    }
}

void SPKnot::updateCtrl() {
    if (!this->item) {
        return;
    }

    g_object_set(this->item, "shape", this->shape, NULL);
    g_object_set(this->item, "mode", this->mode, NULL);
    g_object_set(this->item, "size", this->size, NULL);
    g_object_set(this->item, "angle", this->angle, NULL);
    g_object_set(this->item, "anchor", this->anchor, NULL);

    if (this->pixbuf) {
        g_object_set(this->item, "pixbuf", this->pixbuf, NULL);
    }

    this->_setCtrlState();
}

void SPKnot::_setCtrlState() {
    int state = SP_KNOT_STATE_NORMAL;

    if (this->flags & SP_KNOT_DRAGGING) {
        state = SP_KNOT_STATE_DRAGGING;
    } else if (this->flags & SP_KNOT_MOUSEOVER) {
        state = SP_KNOT_STATE_MOUSEOVER;
    } else if (this->flags & SP_KNOT_SELECTED) {
        state = SP_KNOT_STATE_SELECTED;
    }
    g_object_set(this->item, "fill_color",   this->fill[state],   NULL);
    g_object_set(this->item, "stroke_color", this->stroke[state], NULL);
}


void SPKnot::setSize(guint i) {
    size = i;
}

void SPKnot::setShape(guint i) {
    shape = (SPKnotShapeType) i;
}

void SPKnot::setAnchor(guint i) {
    anchor = (SPAnchorType) i;
}

void SPKnot::setMode(guint i) {
    mode = (SPKnotModeType) i;
}

void SPKnot::setPixbuf(gpointer p) {
    pixbuf = p;
}

void SPKnot::setAngle(double i) {
    angle = i;
}

void SPKnot::setFill(guint32 normal, guint32 mouseover, guint32 dragging, guint32 selected) {
    fill[SP_KNOT_STATE_NORMAL] = normal;
    fill[SP_KNOT_STATE_MOUSEOVER] = mouseover;
    fill[SP_KNOT_STATE_DRAGGING] = dragging;
    fill[SP_KNOT_STATE_SELECTED] = selected;
}

void SPKnot::setStroke(guint32 normal, guint32 mouseover, guint32 dragging, guint32 selected) {
    stroke[SP_KNOT_STATE_NORMAL] = normal;
    stroke[SP_KNOT_STATE_MOUSEOVER] = mouseover;
    stroke[SP_KNOT_STATE_DRAGGING] = dragging;
    stroke[SP_KNOT_STATE_SELECTED] = selected;
}

void SPKnot::setImage(guchar* normal, guchar* mouseover, guchar* dragging, guchar* selected) {
    image[SP_KNOT_STATE_NORMAL] = normal;
    image[SP_KNOT_STATE_MOUSEOVER] = mouseover;
    image[SP_KNOT_STATE_DRAGGING] = dragging;
    image[SP_KNOT_STATE_SELECTED] = selected;
}

void SPKnot::setCursor(GdkCursor* normal, GdkCursor* mouseover, GdkCursor* dragging, GdkCursor* selected) {
    if (cursor[SP_KNOT_STATE_NORMAL]) {
        g_object_unref(cursor[SP_KNOT_STATE_NORMAL]);
    }

    cursor[SP_KNOT_STATE_NORMAL] = normal;

    if (normal) {
        g_object_ref(normal);
    }

    if (cursor[SP_KNOT_STATE_MOUSEOVER]) {
        g_object_unref(cursor[SP_KNOT_STATE_MOUSEOVER]);
    }

    cursor[SP_KNOT_STATE_MOUSEOVER] = mouseover;

    if (mouseover) {
        g_object_ref(mouseover);
    }

    if (cursor[SP_KNOT_STATE_DRAGGING]) {
        g_object_unref(cursor[SP_KNOT_STATE_DRAGGING]);
    }

    cursor[SP_KNOT_STATE_DRAGGING] = dragging;

    if (dragging) {
        g_object_ref(dragging);
    }
    
    if (cursor[SP_KNOT_STATE_SELECTED]) {
        g_object_unref(cursor[SP_KNOT_STATE_SELECTED]);
    }

    cursor[SP_KNOT_STATE_SELECTED] = selected;

    if (selected) {
        g_object_ref(selected);
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
