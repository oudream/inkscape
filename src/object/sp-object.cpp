// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * SPObject implementation.
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Stephen Silver <sasilver@users.sourceforge.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *   Adrian Boguszewski
 *
 * Copyright (C) 1999-2016 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cstring>
#include <string>
#include <vector>

#include <boost/range/adaptor/transformed.hpp>

#include "helper/sp-marshal.h"
#include "xml/node-event-vector.h"
#include "attributes.h"
#include "attribute-rel-util.h"
#include "color-profile.h"
#include "document.h"
#include "preferences.h"
#include "style.h"
#include "live_effects/lpeobject.h"
#include "sp-factory.h"
#include "sp-paint-server.h"
#include "sp-root.h"
#include "sp-style-elem.h"
#include "sp-script.h"
#include "streq.h"
#include "strneq.h"
#include "xml/node-fns.h"
#include "debug/event-tracker.h"
#include "debug/simple-event.h"
#include "debug/demangle.h"
#include "util/format.h"
#include "util/longest-common-suffix.h"

#define noSP_OBJECT_DEBUG_CASCADE

#define noSP_OBJECT_DEBUG

#ifdef SP_OBJECT_DEBUG
# define debug(f, a...) { g_print("%s(%d) %s:", \
                                  __FILE__,__LINE__,__FUNCTION__); \
                          g_print(f, ## a); \
                          g_print("\n"); \
                        }
#else
# define debug(f, a...) /* */
#endif

// Define to enable indented tracing of SPObject.
//#define OBJECT_TRACE
unsigned SPObject::indent_level = 0;

guint update_in_progress = 0; // guard against update-during-update

Inkscape::XML::NodeEventVector object_event_vector = {
    SPObject::repr_child_added,
    SPObject::repr_child_removed,
    SPObject::repr_attr_changed,
    SPObject::repr_content_changed,
    SPObject::repr_order_changed
};

/**
 * A friend class used to set internal members on SPObject so as to not expose settors in SPObject's public API
 */
class SPObjectImpl
{
public:

/**
 * Null's the id member of an SPObject without attempting to free prior contents.
 *
 * @param[inout] obj Pointer to the object which's id shall be nulled.
 */
    static void setIdNull( SPObject* obj ) {
        if (obj) {
            obj->id = nullptr;
        }
    }

/**
 * Sets the id member of an object, freeing any prior content.
 *
 * @param[inout] obj Pointer to the object which's id shall be set.
 * @param[in] id New id
 */
    static void setId( SPObject* obj, gchar const* id ) {
        if (obj && (id != obj->id) ) {
            if (obj->id) {
                g_free(obj->id);
                obj->id = nullptr;
            }
            if (id) {
                obj->id = g_strdup(id);
            }
        }
    }
};

/**
 * Constructor, sets all attributes to default values.
 */
SPObject::SPObject()
    : cloned(0), clone_original(nullptr), uflags(0), mflags(0), hrefcount(0), _total_hrefcount(0),
      document(nullptr), parent(nullptr), id(nullptr), repr(nullptr), refCount(1), hrefList(std::list<SPObject*>()),
      _successor(nullptr), _collection_policy(SPObject::COLLECT_WITH_PARENT),
      _label(nullptr), _default_label(nullptr)
{
    debug("id=%p, typename=%s",this, g_type_name_from_instance((GTypeInstance*)this));

    //used XML Tree here.
    this->getRepr(); // TODO check why this call is made

    SPObjectImpl::setIdNull(this);

    // FIXME: now we create style for all objects, but per SVG, only the following can have style attribute:
    // vg, g, defs, desc, title, symbol, use, image, switch, path, rect, circle, ellipse, line, polyline,
    // polygon, text, tspan, tref, textPath, altGlyph, glyphRef, marker, linearGradient, radialGradient,
    // stop, pattern, clipPath, mask, filter, feImage, a, font, glyph, missing-glyph, foreignObject
    this->style = new SPStyle( nullptr, this ); // Is it necessary to call with "this"?
    this->context_style = nullptr;
}

/**
 * Destructor, frees the used memory and unreferences a potential successor of the object.
 */
SPObject::~SPObject() {
    g_free(this->_label);
    g_free(this->_default_label);

    this->_label = nullptr;
    this->_default_label = nullptr;

    if (this->_successor) {
        sp_object_unref(this->_successor, nullptr);
        this->_successor = nullptr;
    }
    if (parent) {
        parent->children.erase(parent->children.iterator_to(*this));
    }

    if( style == nullptr ) {
        // style pointer could be NULL if unreffed too many times.
        // Conjecture: style pointer is never NULL.
        std::cerr << "SPObject::~SPObject(): style pointer is NULL" << std::endl;
    } else if( style->refCount() > 1 ) {
        // Conjecture: style pointer should be unreffed by other classes before reaching here.
        // Conjecture is false for SPTSpan where ref is held by InputStreamTextSource.
        // As an additional note:
        //   The outer tspan of a nested tspan will result in a ref count of five: one for the
        //   TSpan itself, one for the InputStreamTextSource instance before the inner tspan and
        //   one for the one after, along with one for each corresponding DrawingText instance.
        // std::cerr << "SPObject::~SPObject(): someone else still holding ref to style" << std::endl;
        //
        sp_style_unref( this->style );
    } else {
        delete this->style;
    }
}

// CPPIFY: make pure virtual
void SPObject::read_content() {
    //throw;
}

void SPObject::update(SPCtx* /*ctx*/, unsigned int /*flags*/) {
    //throw;
}

void SPObject::modified(unsigned int /*flags*/) {
#ifdef OBJECT_TRACE
    objectTrace( "SPObject::modified  (default) (empty function)" );
    objectTrace( "SPObject::modified  (default)", false );
#endif
    //throw;
}

namespace {

namespace Debug = Inkscape::Debug;
namespace Util = Inkscape::Util;

typedef Debug::SimpleEvent<Debug::Event::REFCOUNT> BaseRefCountEvent;

class RefCountEvent : public BaseRefCountEvent {
public:
    RefCountEvent(SPObject *object, int bias, char const *name)
    : BaseRefCountEvent(name)
    {
        _addProperty("object", Util::format("%p", object).pointer());
        _addProperty("class", Debug::demangle(g_type_name(G_TYPE_FROM_INSTANCE(object))));
        _addProperty("new-refcount", Util::format("%d", G_OBJECT(object)->ref_count + bias).pointer());
    }
};

class RefEvent : public RefCountEvent {
public:
    RefEvent(SPObject *object)
    : RefCountEvent(object, 1, "sp-object-ref")
    {}
};

class UnrefEvent : public RefCountEvent {
public:
    UnrefEvent(SPObject *object)
    : RefCountEvent(object, -1, "sp-object-unref")
    {}
};

}

gchar const* SPObject::getId() const {
    return id;
}

Inkscape::XML::Node * SPObject::getRepr() {
    return repr;
}

Inkscape::XML::Node const* SPObject::getRepr() const{
    return repr;
}


SPObject *sp_object_ref(SPObject *object, SPObject *owner)
{
    g_return_val_if_fail(object != nullptr, NULL);
    g_return_val_if_fail(SP_IS_OBJECT(object), NULL);
    g_return_val_if_fail(!owner || SP_IS_OBJECT(owner), NULL);

    Inkscape::Debug::EventTracker<RefEvent> tracker(object);
    //g_object_ref(G_OBJECT(object));
    object->refCount++;
    return object;
}

SPObject *sp_object_unref(SPObject *object, SPObject *owner)
{
    g_return_val_if_fail(object != nullptr, NULL);
    g_return_val_if_fail(SP_IS_OBJECT(object), NULL);
    g_return_val_if_fail(!owner || SP_IS_OBJECT(owner), NULL);

    Inkscape::Debug::EventTracker<UnrefEvent> tracker(object);
    //g_object_unref(G_OBJECT(object));
    object->refCount--;

    if (object->refCount <= 0) {
        delete object;
    }

    return nullptr;
}

SPObject *sp_object_href(SPObject *object, SPObject* owner)
{
    g_return_val_if_fail(object != nullptr, NULL);
    g_return_val_if_fail(SP_IS_OBJECT(object), NULL);

    object->hrefcount++;
    object->_updateTotalHRefCount(1);

    if(owner)
        object->hrefList.push_front(owner);

    return object;
}

SPObject *sp_object_hunref(SPObject *object, SPObject* owner)
{
    g_return_val_if_fail(object != nullptr, NULL);
    g_return_val_if_fail(SP_IS_OBJECT(object), NULL);
    g_return_val_if_fail(object->hrefcount > 0, NULL);

    object->hrefcount--;
    object->_updateTotalHRefCount(-1);

    if(owner)
        object->hrefList.remove(owner);

    return nullptr;
}

void SPObject::_updateTotalHRefCount(int increment) {
    SPObject *topmost_collectable = nullptr;
    for ( SPObject *iter = this ; iter ; iter = iter->parent ) {
        iter->_total_hrefcount += increment;
        if ( iter->_total_hrefcount < iter->hrefcount ) {
            g_critical("HRefs overcounted");
        }
        if ( iter->_total_hrefcount == 0 &&
             iter->_collection_policy != COLLECT_WITH_PARENT )
        {
            topmost_collectable = iter;
        }
    }
    if (topmost_collectable) {
        topmost_collectable->requestOrphanCollection();
    }
}

bool SPObject::isAncestorOf(SPObject const *object) const {
    g_return_val_if_fail(object != nullptr, false);
    object = object->parent;
    while (object) {
        if ( object == this ) {
            return true;
        }
        object = object->parent;
    }
    return false;
}

namespace {

bool same_objects(SPObject const &a, SPObject const &b) {
    return &a == &b;
}

}

SPObject const *SPObject::nearestCommonAncestor(SPObject const *object) const {
    g_return_val_if_fail(object != nullptr, NULL);

    using Inkscape::Algorithms::longest_common_suffix;
    return longest_common_suffix<SPObject::ConstParentIterator>(this, object, nullptr, &same_objects);
}

static SPObject const *AncestorSon(SPObject const *obj, SPObject const *ancestor) {
    SPObject const *result = nullptr;
    if ( obj && ancestor ) {
        if (obj->parent == ancestor) {
            result = obj;
        } else {
            result = AncestorSon(obj->parent, ancestor);
        }
    }
    return result;
}

int sp_object_compare_position(SPObject const *first, SPObject const *second)
{
    int result = 0;
    if (first != second) {
        SPObject const *ancestor = first->nearestCommonAncestor(second);
        // Need a common ancestor to be able to compare
        if ( ancestor ) {
            // we have an object and its ancestor (should not happen when sorting selection)
            if (ancestor == first) {
                result = 1;
            } else if (ancestor == second) {
                result = -1;
            } else {
                SPObject const *to_first = AncestorSon(first, ancestor);
                SPObject const *to_second = AncestorSon(second, ancestor);

                g_assert(to_second->parent == to_first->parent);

                result = sp_repr_compare_position(to_first->getRepr(), to_second->getRepr());
            }
        }
    }
    return result;
}

bool sp_object_compare_position_bool(SPObject const *first, SPObject const *second){
    return sp_object_compare_position(first,second)<0;
}


SPObject *SPObject::appendChildRepr(Inkscape::XML::Node *repr) {
    if ( !cloned ) {
        getRepr()->appendChild(repr);
        return document->getObjectByRepr(repr);
    } else {
        g_critical("Attempt to append repr as child of cloned object");
        return nullptr;
    }
}

void SPObject::setCSS(SPCSSAttr *css, gchar const *attr)
{
    g_assert(this->getRepr() != nullptr);
    sp_repr_css_set(this->getRepr(), css, attr);
}

void SPObject::changeCSS(SPCSSAttr *css, gchar const *attr)
{
    g_assert(this->getRepr() != nullptr);
    sp_repr_css_change(this->getRepr(), css, attr);
}

std::vector<SPObject*> SPObject::childList(bool add_ref, Action) {
    std::vector<SPObject*> l;
    for (auto& child: children) {
        if (add_ref) {
            sp_object_ref(&child);
        }
        l.push_back(&child);
    }
    return l;
}

gchar const *SPObject::label() const {
    return _label;
}

gchar const *SPObject::defaultLabel() const {
    if (_label) {
        return _label;
    } else {
        if (!_default_label) {
            if (getId()) {
                _default_label = g_strdup_printf("#%s", getId());
            } else {
                _default_label = g_strdup_printf("<%s>", getRepr()->name());
            }
        }
        return _default_label;
    }
}

void SPObject::setLabel(gchar const *label)
{
    getRepr()->setAttribute("inkscape:label", label, false);
}


void SPObject::requestOrphanCollection() {
    g_return_if_fail(document != nullptr);
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    // do not remove style or script elements (Bug #276244)
    if (dynamic_cast<SPStyleElem *>(this)) {
        // leave it
    } else if (dynamic_cast<SPScript *>(this)) {
        // leave it
  
    } else if ((! prefs->getBool("/options/cleanupswatches/value", false)) && SP_IS_PAINT_SERVER(this) && static_cast<SPPaintServer*>(this)->isSwatch() ) {
        // leave it
    } else if (IS_COLORPROFILE(this)) {
        // leave it
    } else if (IS_LIVEPATHEFFECT(this)) {
        document->queueForOrphanCollection(this);
    } else {
        document->queueForOrphanCollection(this);

        /** \todo
         * This is a temporary hack added to make fill&stroke rebuild its
         * gradient list when the defs are vacuumed.  gradient-vector.cpp
         * listens to the modified signal on defs, and now we give it that
         * signal.  Mental says that this should be made automatic by
         * merging SPObjectGroup with SPObject; SPObjectGroup would issue
         * this signal automatically. Or maybe just derive SPDefs from
         * SPObjectGroup?
         */

        this->requestModified(SP_OBJECT_CHILD_MODIFIED_FLAG);
    }
}

void SPObject::_sendDeleteSignalRecursive() {
    for (auto& child: children) {
        child._delete_signal.emit(&child);
        child._sendDeleteSignalRecursive();
    }
}

void SPObject::deleteObject(bool propagate, bool propagate_descendants)
{
    sp_object_ref(this, nullptr);
    if ( SP_IS_LPE_ITEM(this) && SP_LPE_ITEM(this)->hasPathEffect()) {
        SP_LPE_ITEM(this)->removeAllPathEffects(false);
    }
    if (propagate) {
        _delete_signal.emit(this);
    }
    if (propagate_descendants) {
        this->_sendDeleteSignalRecursive();
    }
    
    Inkscape::XML::Node *repr = getRepr();
    if (repr && repr->parent()) {
        sp_repr_unparent(repr);
    }

    if (_successor) {
        _successor->deleteObject(propagate, propagate_descendants);
    }
    sp_object_unref(this, nullptr);
}

void SPObject::cropToObject(SPObject *except)
{
    std::vector<SPObject*> toDelete;
    for (auto& child: children) {
        if (SP_IS_ITEM(&child)) {
            if (child.isAncestorOf(except)) {
                child.cropToObject(except);
            } else if(&child != except) {
                toDelete.push_back(&child);
            }
        }
    }
    for (auto & i : toDelete) {
        i->deleteObject(true, true);
    }
}

void SPObject::attach(SPObject *object, SPObject *prev)
{
    //g_return_if_fail(parent != NULL);
    //g_return_if_fail(SP_IS_OBJECT(parent));
    g_return_if_fail(object != nullptr);
    g_return_if_fail(SP_IS_OBJECT(object));
    g_return_if_fail(!prev || SP_IS_OBJECT(prev));
    g_return_if_fail(!prev || prev->parent == this);
    g_return_if_fail(!object->parent);

    sp_object_ref(object, this);
    object->parent = this;
    this->_updateTotalHRefCount(object->_total_hrefcount);

    auto it = children.begin();
    if (prev != nullptr) {
        it = ++children.iterator_to(*prev);
    }
    children.insert(it, *object);

    if (!object->xml_space.set)
        object->xml_space.value = this->xml_space.value;
}

void SPObject::reorder(SPObject* obj, SPObject* prev) {
    g_return_if_fail(obj != nullptr);
    g_return_if_fail(obj->parent);
    g_return_if_fail(obj->parent == this);
    g_return_if_fail(obj != prev);
    g_return_if_fail(!prev || prev->parent == obj->parent);

    auto it = children.begin();
    if (prev != nullptr) {
        it = ++children.iterator_to(*prev);
    }

    children.splice(it, children, children.iterator_to(*obj));
}

void SPObject::detach(SPObject *object)
{
    //g_return_if_fail(parent != NULL);
    //g_return_if_fail(SP_IS_OBJECT(parent));
    g_return_if_fail(object != nullptr);
    g_return_if_fail(SP_IS_OBJECT(object));
    g_return_if_fail(object->parent == this);

    children.erase(children.iterator_to(*object));
    object->releaseReferences();

    object->parent = nullptr;

    this->_updateTotalHRefCount(-object->_total_hrefcount);
    sp_object_unref(object, this);
}

SPObject *SPObject::get_child_by_repr(Inkscape::XML::Node *repr)
{
    g_return_val_if_fail(repr != nullptr, NULL);
    SPObject *result = nullptr;

    if (children.size() > 0 && children.back().getRepr() == repr) {
        result = &children.back();   // optimization for common scenario
    } else {
        for (auto& child: children) {
            if (child.getRepr() == repr) {
                result = &child;
                break;
            }
        }
    }
    return result;
}

void SPObject::child_added(Inkscape::XML::Node *child, Inkscape::XML::Node *ref) {
    SPObject* object = this;

    const std::string type_string = NodeTraits::get_type_string(*child);

    SPObject* ochild = SPFactory::createObject(type_string);
    if (ochild == nullptr) {
        // Currently, there are many node types that do not have
        // corresponding classes in the SPObject tree.
        // (rdf:RDF, inkscape:clipboard, ...)
        // Thus, simply ignore this case for now.
        return;
    }

    SPObject *prev = ref ? object->get_child_by_repr(ref) : nullptr;
    object->attach(ochild, prev);
    sp_object_unref(ochild, nullptr);

    ochild->invoke_build(object->document, child, object->cloned);
}

void SPObject::release() {
    SPObject* object = this;
    debug("id=%p, typename=%s", object, g_type_name_from_instance((GTypeInstance*)object));
    auto tmp = children | boost::adaptors::transformed([](SPObject& obj){return &obj;});
    std::vector<SPObject *> toRelease(tmp.begin(), tmp.end());

    for (auto& p: toRelease) {
        object->detach(p);
    }
}

void SPObject::remove_child(Inkscape::XML::Node* child) {
    debug("id=%p, typename=%s", this, g_type_name_from_instance((GTypeInstance*)this));

    SPObject *ochild = this->get_child_by_repr(child);

    // If the xml node has got a corresponding child in the object tree
    if (ochild) {
        this->detach(ochild);
    }
}

void SPObject::order_changed(Inkscape::XML::Node *child, Inkscape::XML::Node * /*old_ref*/, Inkscape::XML::Node *new_ref) {
    SPObject* object = this;

    SPObject *ochild = object->get_child_by_repr(child);
    g_return_if_fail(ochild != nullptr);
    SPObject *prev = new_ref ? object->get_child_by_repr(new_ref) : nullptr;
    object->reorder(ochild, prev);
    ochild->_position_changed_signal.emit(ochild);
}

void SPObject::build(SPDocument *document, Inkscape::XML::Node *repr) {

#ifdef OBJECT_TRACE
    objectTrace( "SPObject::build" );
#endif
    SPObject* object = this;

    /* Nothing specific here */
    debug("id=%p, typename=%s", object, g_type_name_from_instance((GTypeInstance*)object));

    object->readAttr("xml:space");
    object->readAttr("inkscape:label");
    object->readAttr("inkscape:collect");
    if(object->cloned && (repr->attribute("id")) ) // The cases where this happens are when the "original" has no id. This happens
                                                   // if it is a SPString (a TextNode, e.g. in a <title>), or when importing
                                                   // stuff externally modified to have no id. 
        object->clone_original = document->getObjectById(repr->attribute("id"));

    for (Inkscape::XML::Node *rchild = repr->firstChild() ; rchild != nullptr; rchild = rchild->next()) {
        const std::string typeString = NodeTraits::get_type_string(*rchild);

        SPObject* child = SPFactory::createObject(typeString);
        if (child == nullptr) {
            // Currently, there are many node types that do not have
            // corresponding classes in the SPObject tree.
            // (rdf:RDF, inkscape:clipboard, ...)
            // Thus, simply ignore this case for now.
            continue;
        }

        object->attach(child, object->lastChild());
        sp_object_unref(child, nullptr);
        child->invoke_build(document, rchild, object->cloned);
    }

#ifdef OBJECT_TRACE
    objectTrace( "SPObject::build", false );
#endif
}

void SPObject::invoke_build(SPDocument *document, Inkscape::XML::Node *repr, unsigned int cloned)
{
#ifdef OBJECT_TRACE
    objectTrace( "SPObject::invoke_build" );
#endif
    debug("id=%p, typename=%s", this, g_type_name_from_instance((GTypeInstance*)this));

    //g_assert(object != NULL);
    //g_assert(SP_IS_OBJECT(object));
    g_assert(document != nullptr);
    g_assert(repr != nullptr);

    g_assert(this->document == nullptr);
    g_assert(this->repr == nullptr);
    g_assert(this->getId() == nullptr);

    /* Bookkeeping */

    this->document = document;
    this->repr = repr;
    if (!cloned) {
        Inkscape::GC::anchor(repr);
    }
    this->cloned = cloned;

    /* Invoke derived methods, if any */
    this->build(document, repr);

    if ( !cloned ) {
        this->document->bindObjectToRepr(this->repr, this);

        if (Inkscape::XML::id_permitted(this->repr)) {
            /* If we are not cloned, and not seeking, force unique id */
            gchar const *id = this->repr->attribute("id");
            if (!document->isSeeking()) {
                {
                    gchar *realid = sp_object_get_unique_id(this, id);
                    g_assert(realid != nullptr);

                    this->document->bindObjectToId(realid, this);
                    SPObjectImpl::setId(this, realid);
                    g_free(realid);
                }

                /* Redefine ID, if required */
                if ((id == nullptr) || (std::strcmp(id, this->getId()) != 0)) {
                    this->repr->setAttribute("id", this->getId());
                }
            } else if (id) {
                // bind if id, but no conflict -- otherwise, we can expect
                // a subsequent setting of the id attribute
                if (!this->document->getObjectById(id)) {
                    this->document->bindObjectToId(id, this);
                    SPObjectImpl::setId(this, id);
                }
            }
        }
    } else {
        g_assert(this->getId() == nullptr);
    }


    /* Signalling (should be connected AFTER processing derived methods */
    sp_repr_add_listener(repr, &object_event_vector, this);

#ifdef OBJECT_TRACE
    objectTrace( "SPObject::invoke_build", false );
#endif
}

int SPObject::getIntAttribute(char const *key, int def)
{
    sp_repr_get_int(getRepr(),key,&def);
    return def;
}

unsigned SPObject::getPosition(){
    g_assert(this->repr);

    return repr->position();
}

void SPObject::appendChild(Inkscape::XML::Node *child) {
    g_assert(this->repr);

    repr->appendChild(child);
}

SPObject* SPObject::nthChild(unsigned index) {
    g_assert(this->repr);
    if (hasChildren()) {
        std::vector<SPObject*> l;
        unsigned counter = 0;
        for (auto& child: children) {
            if (counter == index) {
                return &child;
            }
            counter++;
        }
    }
    return nullptr;
}

void SPObject::addChild(Inkscape::XML::Node *child, Inkscape::XML::Node * prev)
{
    g_assert(this->repr);

    repr->addChild(child,prev);
}

void SPObject::releaseReferences() {
    g_assert(this->document);
    g_assert(this->repr);

    sp_repr_remove_listener_by_data(this->repr, this);

    this->_release_signal.emit(this);

    this->release();

    /* all hrefs should be released by the "release" handlers */
    g_assert(this->hrefcount == 0);

    if (!cloned) {
        if (this->id) {
            this->document->bindObjectToId(this->id, nullptr);
        }
        g_free(this->id);
        this->id = nullptr;

        g_free(this->_default_label);
        this->_default_label = nullptr;

        this->document->bindObjectToRepr(this->repr, nullptr);

        Inkscape::GC::release(this->repr);
    } else {
        g_assert(!this->id);
    }

    // style belongs to SPObject, we should not need to unref here.
    // if (this->style) {
    //     this->style = sp_style_unref(this->style);
    // }

    this->document = nullptr;
    this->repr = nullptr;
}


SPObject *SPObject::getPrev()
{
    SPObject *prev = nullptr;
    if (parent && !parent->children.empty() && &parent->children.front() != this) {
        prev = &*(--parent->children.iterator_to(*this));
    }
    return prev;
}

SPObject* SPObject::getNext()
{
    SPObject *next = nullptr;
    if (parent && !parent->children.empty() && &parent->children.back() != this) {
        next = &*(++parent->children.iterator_to(*this));
    }
    return next;
}

void SPObject::repr_child_added(Inkscape::XML::Node * /*repr*/, Inkscape::XML::Node *child, Inkscape::XML::Node *ref, gpointer data)
{
    SPObject *object = SP_OBJECT(data);

    object->child_added(child, ref);
}

void SPObject::repr_child_removed(Inkscape::XML::Node * /*repr*/, Inkscape::XML::Node *child, Inkscape::XML::Node * /*ref*/, gpointer data)
{
    SPObject *object = SP_OBJECT(data);

    object->remove_child(child);
}

void SPObject::repr_order_changed(Inkscape::XML::Node * /*repr*/, Inkscape::XML::Node *child, Inkscape::XML::Node *old, Inkscape::XML::Node *newer, gpointer data)
{
    SPObject *object = SP_OBJECT(data);

    object->order_changed(child, old, newer);
}

void SPObject::set(SPAttributeEnum key, gchar const* value) {

#ifdef OBJECT_TRACE
    std::stringstream temp;
    temp << "SPObject::set: " << key  << " " << (value?value:"null");
    objectTrace( temp.str() );
#endif

    g_assert(key != SP_ATTR_INVALID);

    SPObject* object = this;

    switch (key) {
        case SP_ATTR_ID:

            //XML Tree being used here.
            if ( !object->cloned && object->getRepr()->type() == Inkscape::XML::ELEMENT_NODE ) {
                SPDocument *document=object->document;
                SPObject *conflict=nullptr;

                gchar const *new_id = value;

                if (new_id) {
                    conflict = document->getObjectById((char const *)new_id);
                }

                if ( conflict && conflict != object ) {
                    if (!document->isSeeking()) {
                        sp_object_ref(conflict, nullptr);
                        // give the conflicting object a new ID
                        gchar *new_conflict_id = sp_object_get_unique_id(conflict, nullptr);
                        conflict->setAttribute("id", new_conflict_id);
                        g_free(new_conflict_id);
                        sp_object_unref(conflict, nullptr);
                    } else {
                        new_id = nullptr;
                    }
                }

                if (object->getId()) {
                    document->bindObjectToId(object->getId(), nullptr);
                    SPObjectImpl::setId(object, nullptr);
                }

                if (new_id) {
                    SPObjectImpl::setId(object, new_id);
                    document->bindObjectToId(object->getId(), object);
                }

                g_free(object->_default_label);
                object->_default_label = nullptr;
            }
            break;
        case SP_ATTR_INKSCAPE_LABEL:
            g_free(object->_label);
            if (value) {
                object->_label = g_strdup(value);
            } else {
                object->_label = nullptr;
            }
            g_free(object->_default_label);
            object->_default_label = nullptr;
            break;
        case SP_ATTR_INKSCAPE_COLLECT:
            if ( value && !std::strcmp(value, "always") ) {
                object->setCollectionPolicy(SPObject::ALWAYS_COLLECT);
            } else {
                object->setCollectionPolicy(SPObject::COLLECT_WITH_PARENT);
            }
            break;
        case SP_ATTR_XML_SPACE:
            if (value && !std::strcmp(value, "preserve")) {
                object->xml_space.value = SP_XML_SPACE_PRESERVE;
                object->xml_space.set = TRUE;
            } else if (value && !std::strcmp(value, "default")) {
                object->xml_space.value = SP_XML_SPACE_DEFAULT;
                object->xml_space.set = TRUE;
            } else if (object->parent) {
                SPObject *parent;
                parent = object->parent;
                object->xml_space.value = parent->xml_space.value;
            }
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
            break;
        case SP_ATTR_STYLE:
            object->style->readFromObject( object );
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
            break;
        default:
            break;
    }
#ifdef OBJECT_TRACE
    objectTrace( "SPObject::set", false );
#endif
}

void SPObject::setKeyValue(SPAttributeEnum key, gchar const *value)
{
    //g_assert(object != NULL);
    //g_assert(SP_IS_OBJECT(object));

    this->set(key, value);
}

void SPObject::readAttr(gchar const *key)
{
    //g_assert(object != NULL);
    //g_assert(SP_IS_OBJECT(object));
    g_assert(key != nullptr);

    //XML Tree being used here.
    g_assert(this->getRepr() != nullptr);

    auto keyid = sp_attribute_lookup(key);
    if (keyid != SP_ATTR_INVALID) {
        /* Retrieve the 'key' attribute from the object's XML representation */
        gchar const *value = getRepr()->attribute(key);

        setKeyValue(keyid, value);
    }
}

void SPObject::repr_attr_changed(Inkscape::XML::Node * /*repr*/, gchar const *key, gchar const * /*oldval*/, gchar const * /*newval*/, bool is_interactive, gpointer data)
{
    SPObject *object = SP_OBJECT(data);

    object->readAttr(key);

    // manual changes to extension attributes require the normal
    // attributes, which depend on them, to be updated immediately
    if (is_interactive) {
        object->updateRepr(0);
    }
}

void SPObject::repr_content_changed(Inkscape::XML::Node * /*repr*/, gchar const * /*oldcontent*/, gchar const * /*newcontent*/, gpointer data)
{
    SPObject *object = SP_OBJECT(data);

    object->read_content();
}

/**
 * Return string representation of space value.
 */
static gchar const *sp_xml_get_space_string(unsigned int space)
{
    switch (space) {
        case SP_XML_SPACE_DEFAULT:
            return "default";
        case SP_XML_SPACE_PRESERVE:
            return "preserve";
        default:
            return nullptr;
    }
}

Inkscape::XML::Node* SPObject::write(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, guint flags) {
#ifdef OBJECT_TRACE
    objectTrace( "SPObject::write" );
#endif

    if (!repr && (flags & SP_OBJECT_WRITE_BUILD)) {
        repr = this->getRepr()->duplicate(doc);
        if (!( flags & SP_OBJECT_WRITE_EXT )) {
            repr->setAttribute("inkscape:collect", nullptr);
        }
    } else if (repr) {
        repr->setAttribute("id", this->getId());

        if (this->xml_space.set) {
            char const *xml_space;
            xml_space = sp_xml_get_space_string(this->xml_space.value);
            repr->setAttribute("xml:space", xml_space);
        }

        if ( flags & SP_OBJECT_WRITE_EXT &&
             this->collectionPolicy() == SPObject::ALWAYS_COLLECT )
        {
            repr->setAttribute("inkscape:collect", "always");
        } else {
            repr->setAttribute("inkscape:collect", nullptr);
        }

        if (style) {
            // Write if property set by style attribute in this object
            Glib::ustring s =
                style->write(SP_STYLE_FLAG_IFSET | SP_STYLE_FLAG_IFSRC, SP_STYLE_SRC_STYLE_PROP);

            // Check for valid attributes. This may be time consuming.
            // It is useful, though, for debugging Inkscape code.
            Inkscape::Preferences *prefs = Inkscape::Preferences::get();
            if( prefs->getBool("/options/svgoutput/check_on_editing") ) {

                unsigned int flags = sp_attribute_clean_get_prefs();
                Glib::ustring s_cleaned = sp_attribute_clean_style( repr, s.c_str(), flags ); 
            }

            if( s.empty() ) {
                repr->setAttribute("style", nullptr);
            } else {
                repr->setAttribute("style", s.c_str());
            }

        } else {
            /** \todo I'm not sure what to do in this case.  Bug #1165868
             * suggests that it can arise, but the submitter doesn't know
             * how to do so reliably.  The main two options are either
             * leave repr's style attribute unchanged, or explicitly clear it.
             * Must also consider what to do with property attributes for
             * the element; see below.
             */
            char const *style_str = repr->attribute("style");
            if (!style_str) {
                style_str = "NULL";
            }
            g_warning("Item's style is NULL; repr style attribute is %s", style_str);
        }
    }

#ifdef OBJECT_TRACE
    objectTrace( "SPObject::write", false );
#endif
    return repr;
}

Inkscape::XML::Node * SPObject::updateRepr(unsigned int flags)
{
#ifdef OBJECT_TRACE
    objectTrace( "SPObject::updateRepr 1" );
#endif

    if ( !cloned ) {
        Inkscape::XML::Node *repr = getRepr();
        if (repr) {
#ifdef OBJECT_TRACE
            objectTrace( "SPObject::updateRepr 1", false );
#endif
            return updateRepr(repr->document(), repr, flags);
        } else {
            g_critical("Attempt to update non-existent repr");
#ifdef OBJECT_TRACE
            objectTrace( "SPObject::updateRepr 1", false );
#endif
            return nullptr;
        }
    } else {
        /* cloned objects have no repr */
#ifdef OBJECT_TRACE
        objectTrace( "SPObject::updateRepr 1", false );
#endif
        return nullptr;
    }
}

Inkscape::XML::Node * SPObject::updateRepr(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, unsigned int flags)
{
#ifdef OBJECT_TRACE
    objectTrace( "SPObject::updateRepr 2" );
#endif

    g_assert(doc != nullptr);

    if (cloned) {
        /* cloned objects have no repr */
#ifdef OBJECT_TRACE
        objectTrace( "SPObject::updateRepr 2", false );
#endif
        return nullptr;
    }

    if (!(flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = getRepr();
    }

#ifdef OBJECT_TRACE
    Inkscape::XML::Node *node = write(doc, repr, flags);
    objectTrace( "SPObject::updateRepr 2", false );
    return node;
#else
    return this->write(doc, repr, flags);
#endif

}

/* Modification */

void SPObject::requestDisplayUpdate(unsigned int flags)
{
    g_return_if_fail( this->document != nullptr );

    // update_in_progress is a global variable. It can be come greater than one when reading in a second
    // document (as in creating the broken image bitmap). It is still an important warning so we don't
    // remove it entirely. We probably shouldn't be calling requestDisplayUpdate in the set() methods.
    if (update_in_progress > 2) {
        g_print("WARNING: Requested update while update in progress, counter = %d\n", update_in_progress);
    }

    /* requestModified must be used only to set one of SP_OBJECT_MODIFIED_FLAG or
     * SP_OBJECT_CHILD_MODIFIED_FLAG */
    g_return_if_fail(!(flags & SP_OBJECT_PARENT_MODIFIED_FLAG));
    g_return_if_fail((flags & SP_OBJECT_MODIFIED_FLAG) || (flags & SP_OBJECT_CHILD_MODIFIED_FLAG));
    g_return_if_fail(!((flags & SP_OBJECT_MODIFIED_FLAG) && (flags & SP_OBJECT_CHILD_MODIFIED_FLAG)));

#ifdef OBJECT_TRACE
    objectTrace( "SPObject::requestDisplayUpdate" );
#endif

    bool already_propagated = (!(this->uflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG)));
    //https://stackoverflow.com/a/7841333
    if ((this->uflags & flags) !=  flags ) {
        this->uflags |= flags;
    }
    /* If requestModified has already been called on this object or one of its children, then we
     * don't need to set CHILD_MODIFIED on our ancestors because it's already been done.
     */
    if (already_propagated) {
        if(this->document) {
            if (parent) {
                parent->requestDisplayUpdate(SP_OBJECT_CHILD_MODIFIED_FLAG);
            } else {
                this->document->requestModified();
            }
        }
    }

#ifdef OBJECT_TRACE
    objectTrace( "SPObject::requestDisplayUpdate", false );
#endif

}

void SPObject::updateDisplay(SPCtx *ctx, unsigned int flags)
{
    g_return_if_fail(!(flags & ~SP_OBJECT_MODIFIED_CASCADE));

#ifdef OBJECT_TRACE
    objectTrace( "SPObject::updateDisplay" );
#endif

    update_in_progress ++;

#ifdef SP_OBJECT_DEBUG_CASCADE
    g_print("Update %s:%s %x %x %x\n", g_type_name_from_instance((GTypeInstance *) this), getId(), flags, this->uflags, this->mflags);
#endif

    /* Get this flags */
    flags |= this->uflags;
    /* Copy flags to modified cascade for later processing */
    this->mflags |= this->uflags;
    /* We have to clear flags here to allow rescheduling update */
    this->uflags = 0;

    // Merge style if we have good reasons to think that parent style is changed */
    /** \todo
     * I am not sure whether we should check only propagated
     * flag. We are currently assuming that style parsing is
     * done immediately. I think this is correct (Lauris).
     */
    if ((flags & SP_OBJECT_STYLE_MODIFIED_FLAG) && (flags & SP_OBJECT_PARENT_MODIFIED_FLAG)) {
        if (this->style && this->parent) {
            style->cascade( this->parent->style );
        }
    }

    try
    {
        this->update(ctx, flags);
    }
    catch(...)
    {
        /** \todo
        * in case of catching an exception we need to inform the user somehow that the document is corrupted
        * maybe by implementing an document flag documentOk
        * or by a modal error dialog
        */
        g_warning("SPObject::updateDisplay(SPCtx *ctx, unsigned int flags) : throw in ((SPObjectClass *) G_OBJECT_GET_CLASS(this))->update(this, ctx, flags);");
    }

    update_in_progress --;

#ifdef OBJECT_TRACE
    objectTrace( "SPObject::updateDisplay", false );
#endif
}

void SPObject::requestModified(unsigned int flags)
{
    g_return_if_fail( this->document != nullptr );

    /* requestModified must be used only to set one of SP_OBJECT_MODIFIED_FLAG or
     * SP_OBJECT_CHILD_MODIFIED_FLAG */
    g_return_if_fail(!(flags & SP_OBJECT_PARENT_MODIFIED_FLAG));
    g_return_if_fail((flags & SP_OBJECT_MODIFIED_FLAG) || (flags & SP_OBJECT_CHILD_MODIFIED_FLAG));
    g_return_if_fail(!((flags & SP_OBJECT_MODIFIED_FLAG) && (flags & SP_OBJECT_CHILD_MODIFIED_FLAG)));

#ifdef OBJECT_TRACE
    objectTrace( "SPObject::requestModified" );
#endif

    bool already_propagated = (!(this->mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG)));

    this->mflags |= flags;

    /* If requestModified has already been called on this object or one of its children, then we
     * don't need to set CHILD_MODIFIED on our ancestors because it's already been done.
     */
    if (already_propagated) {
        if (parent) {
            parent->requestModified(SP_OBJECT_CHILD_MODIFIED_FLAG);
        } else {
            document->requestModified();
        }
    }
#ifdef OBJECT_TRACE
    objectTrace( "SPObject::requestModified", false );
#endif
}

void SPObject::emitModified(unsigned int flags)
{
    /* only the MODIFIED_CASCADE flag is legal here */
    g_return_if_fail(!(flags & ~SP_OBJECT_MODIFIED_CASCADE));

#ifdef OBJECT_TRACE
    objectTrace( "SPObject::emitModified", true, flags );
#endif

#ifdef SP_OBJECT_DEBUG_CASCADE
    g_print("Modified %s:%s %x %x %x\n", g_type_name_from_instance((GTypeInstance *) this), getId(), flags, this->uflags, this->mflags);
#endif

    flags |= this->mflags;
    /* We have to clear mflags beforehand, as signal handlers may
     * make changes and therefore queue new modification notifications
     * themselves. */
    this->mflags = 0;

    sp_object_ref(this);

    this->modified(flags);

    _modified_signal.emit(this, flags);
    sp_object_unref(this);

#ifdef OBJECT_TRACE
    objectTrace( "SPObject::emitModified", false );
#endif
}

gchar const *SPObject::getTagName(SPException *ex) const
{
    g_assert(repr != nullptr);
    /* If exception is not clear, return */
    if (!SP_EXCEPTION_IS_OK(ex)) {
        return nullptr;
    }

    /// \todo fixme: Exception if object is NULL? */
    //XML Tree being used here.
    return getRepr()->name();
}

gchar const *SPObject::getAttribute(gchar const *key, SPException *ex) const
{
    g_assert(this->repr != nullptr);
    /* If exception is not clear, return */
    if (!SP_EXCEPTION_IS_OK(ex)) {
        return nullptr;
    }

    /// \todo fixme: Exception if object is NULL? */
    //XML Tree being used here.
    return (gchar const *) getRepr()->attribute(key);
}

void SPObject::setAttribute(gchar const *key, gchar const *value, SPException *ex)
{
    g_assert(this->repr != nullptr);
    /* If exception is not clear, return */
    g_return_if_fail(SP_EXCEPTION_IS_OK(ex));

    /// \todo fixme: Exception if object is NULL? */
    //XML Tree being used here.
    getRepr()->setAttribute(key, value, false);
}

void SPObject::setAttribute(char const *key, Glib::ustring const &value, SPException *ex)
{
    setAttribute(key, value.empty() ? nullptr : value.c_str(), ex);
}

void SPObject::setAttribute(Glib::ustring const &key, Glib::ustring const &value, SPException *ex)
{
    setAttribute( key.empty()   ? nullptr : key.c_str(),
                  value.empty() ? nullptr : value.c_str(), ex);
}


void SPObject::removeAttribute(gchar const *key, SPException *ex)
{
    /* If exception is not clear, return */
    g_return_if_fail(SP_EXCEPTION_IS_OK(ex));

    /// \todo fixme: Exception if object is NULL? */
    //XML Tree being used here.
    getRepr()->setAttribute(key, nullptr, false);
}

bool SPObject::storeAsDouble( gchar const *key, double *val ) const
{
    g_assert(this->getRepr()!= nullptr);
    return sp_repr_get_double(((Inkscape::XML::Node *)(this->getRepr())),key,val);
}

/** Helper */
gchar *
sp_object_get_unique_id(SPObject    *object,
                        gchar const *id)
{
    static unsigned long count = 0;

    g_assert(SP_IS_OBJECT(object));

    count++;

    //XML Tree being used here.
    gchar const *name = object->getRepr()->name();
    g_assert(name != nullptr);

    gchar const *local = std::strchr(name, ':');
    if (local) {
        name = local + 1;
    }

    if (id != nullptr) {
        if (object->document->getObjectById(id) == nullptr) {
            return g_strdup(id);
        }
    }

    size_t const name_len = std::strlen(name);
    size_t const buflen = name_len + (sizeof(count) * 10 / 4) + 1;
    gchar *const buf = (gchar *) g_malloc(buflen);
    std::memcpy(buf, name, name_len);
    gchar *const count_buf = buf + name_len;
    size_t const count_buflen = buflen - name_len;
    do {
        ++count;
        g_snprintf(count_buf, count_buflen, "%lu", count);
    } while ( object->document->getObjectById(buf) != nullptr );
    return buf;
}

void SPObject::_requireSVGVersion(Inkscape::Version version) {
    for ( SPObject::ParentIterator iter=this ; iter ; ++iter ) {
        SPObject *object = iter;
        if (SP_IS_ROOT(object)) {
            SPRoot *root = SP_ROOT(object);
            if ( root->version.svg < version ) {
                root->version.svg = version;
            }
        }
    }
}

// Titles and descriptions

/* Note:
   Titles and descriptions are stored in 'title' and 'desc' child elements
   (see section 5.4 of the SVG 1.0 and 1.1 specifications).  The spec allows
   an element to have more than one 'title' child element, but strongly
   recommends against this and requires using the first one if a choice must
   be made.  The same applies to 'desc' elements.  Therefore, these functions
   ignore all but the first 'title' child element and first 'desc' child
   element, except when deleting a title or description.

   This will change in SVG 2, where multiple 'title' and 'desc' elements will
   be allowed with different localized strings.
*/

gchar * SPObject::title() const
{
    return getTitleOrDesc("svg:title");
}

bool SPObject::setTitle(gchar const *title, bool verbatim)
{
    return setTitleOrDesc(title, "svg:title", verbatim);
}

gchar * SPObject::desc() const
{
    return getTitleOrDesc("svg:desc");
}

bool SPObject::setDesc(gchar const *desc, bool verbatim)
{
    return setTitleOrDesc(desc, "svg:desc", verbatim);
}

char * SPObject::getTitleOrDesc(gchar const *svg_tagname) const
{
    char *result = nullptr;
    SPObject *elem = findFirstChild(svg_tagname);
    if ( elem ) {
        //This string copy could be avoided by changing 
        //the return type of SPObject::getTitleOrDesc 
        //to std::unique_ptr<Glib::ustring>
        result = g_strdup(elem->textualContent().c_str());
    }
    return result;
}

bool SPObject::setTitleOrDesc(gchar const *value, gchar const *svg_tagname, bool verbatim)
{
    if (!verbatim) {
        // If the new title/description is just whitespace,
        // treat it as though it were NULL.
        if (value) {
            bool just_whitespace = true;
            for (const gchar *cp = value; *cp; ++cp) {
                if (!std::strchr("\r\n \t", *cp)) {
                    just_whitespace = false;
                    break;
                }
            }
            if (just_whitespace) {
                value = nullptr;
            }
        }
        // Don't stomp on mark-up if there is no real change.
        if (value) {
            gchar *current_value = getTitleOrDesc(svg_tagname);
            if (current_value) {
                bool different = std::strcmp(current_value, value);
                g_free(current_value);
                if (!different) {
                    return false;
                }
            }
        }
    }

    SPObject *elem = findFirstChild(svg_tagname);

    if (value == nullptr) {
        if (elem == nullptr) {
            return false;
        }
        // delete the title/description(s)
        while (elem) {
            elem->deleteObject();
            elem = findFirstChild(svg_tagname);
        }
        return true;
    }

    Inkscape::XML::Document *xml_doc = document->getReprDoc();

    if (elem == nullptr) {
        // create a new 'title' or 'desc' element, putting it at the
        // beginning (in accordance with the spec's recommendations)
        Inkscape::XML::Node *xml_elem = xml_doc->createElement(svg_tagname);
        repr->addChild(xml_elem, nullptr);
        elem = document->getObjectByRepr(xml_elem);
        Inkscape::GC::release(xml_elem);
    }
    else {
        // remove the current content of the 'text' or 'desc' element
        auto tmp = elem->children | boost::adaptors::transformed([](SPObject& obj) { return &obj; });
        std::vector<SPObject*> vec(tmp.begin(), tmp.end());
        for (auto &child: vec) {
            child->deleteObject();
        }
    }

    // add the new content
    elem->appendChildRepr(xml_doc->createTextNode(value));
    return true;
}

SPObject* SPObject::findFirstChild(gchar const *tagname) const
{
    for (auto& child: const_cast<SPObject*>(this)->children)
    {
        if (child.repr->type() == Inkscape::XML::ELEMENT_NODE &&
            !std::strcmp(child.repr->name(), tagname)) {
            return &child;
        }
    }
    return nullptr;
}

Glib::ustring SPObject::textualContent() const
{
    Glib::ustring text;

    for (auto& child: children)
    {
        Inkscape::XML::NodeType child_type = child.repr->type();

        if (child_type == Inkscape::XML::ELEMENT_NODE) {
            text += child.textualContent();
        }
        else if (child_type == Inkscape::XML::TEXT_NODE) {
            text += child.repr->content();
        }
    }
    return text;
}

// For debugging: Print SP tree structure.
void SPObject::recursivePrintTree( unsigned level )
{
    if (level == 0) {
        std::cout << "SP Object Tree" << std::endl;
    }
    std::cout << "SP: ";
    for (unsigned i = 0; i < level; ++i) {
        std::cout << "  ";
    }
    std::cout << (getId()?getId():"No object id") << std::endl;
    for (auto& child: children) {
        child.recursivePrintTree(level + 1);
    }
}

// Function to allow tracing of program flow through SPObject and derived classes.
// To trace function, add at entrance ('in' = true) and exit of function ('in' = false).
void SPObject::objectTrace( std::string text, bool in, unsigned flags ) {
    if( in ) {
        for (unsigned i = 0; i < indent_level; ++i) {
            std::cout << "  ";
        }
        std::cout << text << ":"
                  << " entrance: "
                  << (id?id:"null")
                  << " uflags: " << uflags
                  << " mflags: " << mflags
                  << " flags: " << flags << std::endl;
        ++indent_level;
    } else {
        --indent_level;
        for (unsigned i = 0; i < indent_level; ++i) {
            std::cout << "  ";
        }
        std::cout << text << ":"
                  << " exit: "
                  << (id?id:"null")
                  << " uflags: " << uflags
                  << " mflags: " << mflags
                  << " flags: " << flags << std::endl;
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
