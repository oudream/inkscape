// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SP_STYLE_H
#define SEEN_SP_STYLE_H

/** \file
 * SPStyle - a style object for SPItem objects
 */
/* Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Tavmjong Bah <tavmjong@free.fr>
 *
 * Copyright (C) 2014 Tavmjong Bah
 * Copyright (C) 2010 Jon A. Cruz
 * Copyright (C) 2001-2002 Lauris Kaplinski
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "style-enums.h"
#include "style-internal.h"

#include <sigc++/connection.h>
#include <iostream>
#include <vector>
#include "3rdparty/libcroco/cr-declaration.h"
#include "3rdparty/libcroco/cr-prop-list.h"

enum SPAttributeEnum : unsigned;

// Define SPIBasePtr, a Pointer to a data member of SPStyle of type SPIBase;
typedef SPIBase SPStyle::*SPIBasePtr;

namespace Inkscape {
namespace XML {
class Node;
}
}


/// An SVG style object.
class SPStyle {

public:
    
    SPStyle(SPDocument *document = nullptr, SPObject *object = nullptr);// document is ignored if valid object given
    ~SPStyle();
    const std::vector<SPIBase *> properties();
    void clear();
    void clear(SPAttributeEnum id);
    void read(SPObject *object, Inkscape::XML::Node *repr);
    void readFromObject(SPObject *object);
    void readFromPrefs(Glib::ustring const &path);
    bool isSet(SPAttributeEnum id);
    void readIfUnset(SPAttributeEnum id, char const *val, SPStyleSrc const &source = SP_STYLE_SRC_STYLE_PROP );
    Glib::ustring write( unsigned int const flags = SP_STYLE_FLAG_IFSET,
                         SPStyleSrc const &style_src_req = SP_STYLE_SRC_STYLE_PROP,
                         SPStyle const *const base = nullptr ) const;
    void cascade( SPStyle const *const parent );
    void merge(   SPStyle const *const parent );
    void mergeString( char const *const p );
    void mergeStatement( CRStatement *statement );
    bool operator==(const SPStyle& rhs);

    int style_ref()   { ++_refcount; return _refcount; }
    int style_unref() { --_refcount; return _refcount; }
    int refCount() { return _refcount; }

private:
    void _mergeString( char const *const p );
    void _mergeDeclList( CRDeclaration const *const decl_list, SPStyleSrc const &source );
    void _mergeDecl(     CRDeclaration const *const decl,      SPStyleSrc const &source );
    void _mergeProps( CRPropList *const props );
    void _mergeObjectStylesheet( SPObject const *const object );

private:
    int _refcount;
    static int _count; // Poor man's leak detector

// FIXME: Make private
public:
    /** Object we are attached to */
    SPObject *object;
    /** Document we are associated with */
    SPDocument *document;

private:
    /// Pointers to all the properties (for looping through them)
    std::vector<SPIBase *> _properties;

public:

    /* ----------------------- THE PROPERTIES ------------------------- */
    /*                    Match order in style.cpp.                     */

    /* SVG 2 attributes promoted to properties. */

    /** Path data */
    SPIString d;

    /* Font ---------------------------- */

    /** Font style */
    SPIEnum font_style;
    /** Which substyle of the font (CSS 2. CSS 3 redefines as shorthand) */
    SPIEnum font_variant;
    /** Weight of the font */
    SPIEnum font_weight;
    /** Stretch of the font */
    SPIEnum font_stretch;
    /** Size of the font */
    SPIFontSize font_size;
    /** Line height (css2 10.8.1) */
    SPILengthOrNormal line_height;
    /** Font family */
    SPIString font_family;
    /** Font shorthand */
    SPIFont font;
    /** Full font name, as font_factory::ConstructFontSpecification would give, for internal use. */
    SPIString font_specification;

    /* Font variants -------------------- */
    /** Font variant ligatures */
    SPILigatures font_variant_ligatures;
    /** Font variant position (subscript/superscript) */
    SPIEnum font_variant_position;
    /** Font variant caps (small caps) */
    SPIEnum font_variant_caps;
    /** Font variant numeric (numerical formatting) */
    SPINumeric font_variant_numeric;
    /** Font variant alternates (alternates/swatches) */
    SPIEnum font_variant_alternates;
    /** Font variant East Asian */
    SPIEastAsian font_variant_east_asian;
    /** Font feature settings (Low level access to TrueType tables) */
    SPIString font_feature_settings;

    /** Font variation settings (Low level access to OpenType variable font design-coordinate values) */
    SPIFontVariationSettings font_variation_settings;

    /* Text ----------------------------- */

    /** First line indent of paragraphs (css2 16.1) */
    SPILength text_indent;
    /** text alignment (css2 16.2) (not to be confused with text-anchor) */
    SPIEnum text_align;

    /** letter spacing (css2 16.4) */
    SPILengthOrNormal letter_spacing;
    /** word spacing (also css2 16.4) */
    SPILengthOrNormal word_spacing;
    /** capitalization (css2 16.5) */
    SPIEnum text_transform;

    /* CSS3 Text */
    /** text direction (svg1.1) */
    SPIEnum direction;
    /** Writing mode (svg1.1 10.7.2, CSS Writing Modes 3) */
    SPIEnum writing_mode;
    /** Text orientation (CSS Writing Modes 3) */
    SPIEnum text_orientation;
    /** Dominant baseline (svg1.1) */
    SPIEnum dominant_baseline;
    /** Baseline shift (svg1.1 10.9.2) */
    SPIBaselineShift baseline_shift;

    /* SVG */
    /** Anchor of the text (svg1.1 10.9.1) */
    SPIEnum text_anchor;

    /** white space (svg2) */
    SPIEnum white_space;

    /** SVG2 Text Wrapping */
    SPIShapes shape_inside;
    SPIShapes shape_subtract;
    SPILength shape_padding;
    SPILength shape_margin;
    SPILength inline_size;

    /* Text Decoration ----------------------- */

    /** text decoration (css2 16.3.1) */
    SPITextDecoration      text_decoration; 
    /** CSS 3 2.1, 2.2, 2.3 */
    /** Not done yet, test_decoration3        = css3 2.4*/
    SPITextDecorationLine  text_decoration_line;
    SPITextDecorationStyle text_decoration_style;  // SPIEnum? Only one can be set at time.
    SPIColor               text_decoration_color;
    SPIPaint               text_decoration_fill;
    SPIPaint               text_decoration_stroke;
    // used to implement text_decoration, not saved to or read from SVG file
    SPITextDecorationData  text_decoration_data;

    // 16.3.2 is text-shadow. That's complicated.

    /* General visual properties ------------- */

    /** clip-rule: 0 nonzero, 1 evenodd */
    SPIEnum clip_rule;

    /** display */
    SPIEnum display;

    /** overflow */
    SPIEnum overflow;

    /** visibility */
    SPIEnum visibility;

    /** opacity */
    SPIScale24 opacity;

    /** mix-blend-mode:  CSS Compositing and Blending Level 1 */
    SPIEnum isolation;
    SPIEnum mix_blend_mode;

    SPIPaintOrder paint_order;

    /** color */
    SPIColor color;
    /** color-interpolation */
    SPIEnum color_interpolation;
    /** color-interpolation-filters */
    SPIEnum color_interpolation_filters;

    /** solid-color */
    SPIColor solid_color;
    /** solid-opacity */
    SPIScale24 solid_opacity;

    /** vector effect */
    SPIVectorEffect vector_effect;

    /** fill */
    SPIPaint fill;
    /** fill-opacity */
    SPIScale24 fill_opacity;
    /** fill-rule: 0 nonzero, 1 evenodd */
    SPIEnum fill_rule;

    /** stroke */
    SPIPaint stroke;
    /** stroke-width */
    SPILength stroke_width;
    /** stroke-linecap */
    SPIEnum stroke_linecap;
    /** stroke-linejoin */
    SPIEnum stroke_linejoin;
    /** stroke-miterlimit */
    SPIFloat stroke_miterlimit;
    /** stroke-dasharray */
    SPIDashArray stroke_dasharray;
    /** stroke-dashoffset */
    SPILength stroke_dashoffset;
    /** stroke-opacity */
    SPIScale24 stroke_opacity;

    /** Marker list */
    SPIString marker;
    SPIString marker_start;
    SPIString marker_mid;
    SPIString marker_end;
    SPIString* marker_ptrs[SP_MARKER_LOC_QTY]; 

    /* Filter effects ------------------------ */

    /** Filter effect */
    SPIFilter filter;
    /** Filter blend mode */
    SPIEnum filter_blend_mode;
    /** normally not used, but duplicates the Gaussian blur deviation (if any) from the attached
        filter when the style is used for querying */
    SPILength filter_gaussianBlur_deviation;
    /** enable-background, used for defining where filter effects get their background image */
    SPIEnum enable_background;

    /** gradient-stop */
    SPIColor stop_color;
    SPIScale24 stop_opacity;

    /* Rendering hints ----------------------- */

    /** hints on how to render: e.g. speed vs. accuracy.
     * As of April, 2013, only image_rendering used. */
    SPIEnum color_rendering;
    SPIEnum image_rendering;
    SPIEnum shape_rendering;
    SPIEnum text_rendering;

    /* ----------------------- END PROPERTIES ------------------------- */

    /// style belongs to a cloned object
    bool cloned;

    sigc::connection release_connection;

    sigc::connection filter_modified_connection;
    sigc::connection fill_ps_modified_connection;
    sigc::connection stroke_ps_modified_connection;
    sigc::connection fill_ps_changed_connection;
    sigc::connection stroke_ps_changed_connection;

    /**
     * Emitted when paint server object, fill paint refers to, is changed. That is
     * when the reference starts pointing to a different address in memory.
     *
     * NB It is different from fill_ps_modified signal. When paint server is modified
     * it means some of it's attributes or children change.
     */
    sigc::signal<void, SPObject *, SPObject *> signal_fill_ps_changed;
    /**
     * Emitted when paint server object, fill paint refers to, is changed. That is
     * when the reference starts pointing to a different address in memory.
     */
    sigc::signal<void, SPObject *, SPObject *> signal_stroke_ps_changed;

    SPObject       *getFilter()          { return (filter.href) ? filter.href->getObject() : nullptr; }
    SPObject const *getFilter()    const { return (filter.href) ? filter.href->getObject() : nullptr; }
    Inkscape::URI const *getFilterURI() const { return (filter.href) ? filter.href->getURI() : nullptr; }

    SPPaintServer       *getFillPaintServer()         { return (fill.value.href) ? fill.value.href->getObject() : nullptr; }
    SPPaintServer const *getFillPaintServer()   const { return (fill.value.href) ? fill.value.href->getObject() : nullptr; }
    Inkscape::URI const *getFillURI()           const { return (fill.value.href) ? fill.value.href->getURI() : nullptr; }

    SPPaintServer       *getStrokePaintServer()       { return (stroke.value.href) ? stroke.value.href->getObject() : nullptr; }
    SPPaintServer const *getStrokePaintServer() const { return (stroke.value.href) ? stroke.value.href->getObject() : nullptr; }
    Inkscape::URI const *getStrokeURI()         const { return (stroke.value.href) ? stroke.value.href->getURI() : nullptr; }

    /**
     * Return a font feature string useful for Pango.
     */
    std::string getFontFeatureString();

};

SPStyle *sp_style_ref(SPStyle *style); // SPStyle::ref();

SPStyle *sp_style_unref(SPStyle *style); // SPStyle::unref();

void sp_style_set_to_uri(SPStyle *style, bool isfill, Inkscape::URI const *uri); // ?

char const *sp_style_get_css_unit_string(int unit);  // No change?

#define SP_CSS_FONT_SIZE_DEFAULT 12.0
double sp_style_css_size_px_to_units(double size, int unit, double font_size = SP_CSS_FONT_SIZE_DEFAULT); // No change?
double sp_style_css_size_units_to_px(double size, int unit, double font_size = SP_CSS_FONT_SIZE_DEFAULT); // No change?


SPCSSAttr *sp_css_attr_from_style (SPStyle const *const style, unsigned int flags);
SPCSSAttr *sp_css_attr_from_object(SPObject *object, unsigned int flags = SP_STYLE_FLAG_IFSET);
SPCSSAttr *sp_css_attr_unset_text(SPCSSAttr *css);
SPCSSAttr *sp_css_attr_unset_blacklist(SPCSSAttr *css);
SPCSSAttr *sp_css_attr_unset_uris(SPCSSAttr *css);
SPCSSAttr *sp_css_attr_scale(SPCSSAttr *css, double ex);

void sp_style_unset_property_attrs(SPObject *o);

void sp_style_set_property_url (SPObject *item, char const *property, SPObject *linked, bool recursive);

void css_quote( Glib::ustring &val );   // Add quotes around CSS values
void css_unquote( Glib::ustring &val ); // Remove quotes from CSS values (style-internal.cpp, xml/repr-css.cpp)
void css_font_family_quote( Glib::ustring &val ); // style-internal.cpp, text-toolbar.cpp
void css_font_family_unquote( Glib::ustring &val ); // style-internal.cpp, text-toolbar.cpp

Glib::ustring css2_escape_quote(char const *val);

#endif // SEEN_SP_STYLE_H


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
 
