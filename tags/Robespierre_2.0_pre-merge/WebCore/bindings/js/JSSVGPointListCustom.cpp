/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#ifdef SVG_SUPPORT
#include "JSSVGPointList.h"

#include "Document.h"
#include "Frame.h"
#include "JSSVGPoint.h"
#include "SVGDocumentExtensions.h"
#include "SVGPointList.h"
#include "SVGStyledElement.h"

#include <wtf/Assertions.h>

using namespace KJS;

namespace WebCore {

JSValue* JSSVGPointList::clear(ExecState* exec, const List&)
{
    ExceptionCode ec = 0;

    SVGPointList* imp = static_cast<SVGPointList*>(impl());
    imp->clear(ec);
    setDOMException(exec, ec);

    ASSERT(imp->context());
    imp->context()->notifyAttributeChange();

    return jsUndefined();
}

JSValue* JSSVGPointList::initialize(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    FloatPoint newItem = toSVGPoint(args[0]);

    SVGPointList* imp = static_cast<SVGPointList*>(impl());
    SVGList<RefPtr<SVGPODListItem<FloatPoint> > >* listImp = imp;

    SVGPODListItem<FloatPoint>* listItem = listImp->initialize(new SVGPODListItem<FloatPoint>(newItem), ec).get(); 
    JSSVGPODTypeWrapperCreatorForList<FloatPoint, SVGPointList>* obj = new JSSVGPODTypeWrapperCreatorForList<FloatPoint, SVGPointList>(listItem, imp);

    KJS::JSValue* result = toJS(exec, obj);
    setDOMException(exec, ec);

    ASSERT(imp->context());
    imp->context()->notifyAttributeChange();

    return result;
}

JSValue* JSSVGPointList::getItem(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;

    bool indexOk;
    unsigned index = args[0]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPointList* imp = static_cast<SVGPointList*>(impl());
    SVGList<RefPtr<SVGPODListItem<FloatPoint> > >* listImp = imp;

    SVGPODListItem<FloatPoint>* listItem = listImp->getItem(index, ec).get();
    JSSVGPODTypeWrapperCreatorForList<FloatPoint, SVGPointList>* obj = new JSSVGPODTypeWrapperCreatorForList<FloatPoint, SVGPointList>(listItem, imp);

    KJS::JSValue* result = toJS(exec, obj);
    setDOMException(exec, ec);
    return result;
}

JSValue* JSSVGPointList::insertItemBefore(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    FloatPoint newItem = toSVGPoint(args[0]);

    bool indexOk;
    unsigned index = args[1]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPointList* imp = static_cast<SVGPointList*>(impl());
    SVGList<RefPtr<SVGPODListItem<FloatPoint> > >* listImp = imp;

    SVGPODListItem<FloatPoint>* listItem = listImp->insertItemBefore(new SVGPODListItem<FloatPoint>(newItem), index, ec).get();
    JSSVGPODTypeWrapperCreatorForList<FloatPoint, SVGPointList>* obj = new JSSVGPODTypeWrapperCreatorForList<FloatPoint, SVGPointList>(listItem, imp);

    KJS::JSValue* result = toJS(exec, obj);
    setDOMException(exec, ec);

    ASSERT(imp->context());
    imp->context()->notifyAttributeChange();

    return result;
}

JSValue* JSSVGPointList::replaceItem(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    FloatPoint newItem = toSVGPoint(args[0]);

    bool indexOk;
    unsigned index = args[1]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPointList* imp = static_cast<SVGPointList*>(impl());
    SVGList<RefPtr<SVGPODListItem<FloatPoint> > >* listImp = imp;

    SVGPODListItem<FloatPoint>* listItem = listImp->replaceItem(new SVGPODListItem<FloatPoint>(newItem), index, ec).get(); 
    JSSVGPODTypeWrapperCreatorForList<FloatPoint, SVGPointList>* obj = new JSSVGPODTypeWrapperCreatorForList<FloatPoint, SVGPointList>(listItem, imp);

    KJS::JSValue* result = toJS(exec, obj);
    setDOMException(exec, ec);

    ASSERT(imp->context());
    imp->context()->notifyAttributeChange();

    return result;
}

JSValue* JSSVGPointList::removeItem(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    
    bool indexOk;
    unsigned index = args[0]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPointList* imp = static_cast<SVGPointList*>(impl());
    SVGList<RefPtr<SVGPODListItem<FloatPoint> > >* listImp = imp;

    RefPtr<SVGPODListItem<FloatPoint> > listItem(listImp->removeItem(index, ec));
    JSSVGPODTypeWrapper<FloatPoint>* obj = new JSSVGPODTypeWrapper<FloatPoint>((FloatPoint&) *listItem.get());

    KJS::JSValue* result = toJS(exec, obj);
    setDOMException(exec, ec);

    ASSERT(imp->context());
    imp->context()->notifyAttributeChange();

    return result;
}

JSValue* JSSVGPointList::appendItem(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    FloatPoint newItem = toSVGPoint(args[0]);

    SVGPointList* imp = static_cast<SVGPointList*>(impl());
    SVGList<RefPtr<SVGPODListItem<FloatPoint> > >* listImp = imp;

    SVGPODListItem<FloatPoint>* listItem = listImp->appendItem(new SVGPODListItem<FloatPoint>(newItem), ec).get(); 
    JSSVGPODTypeWrapperCreatorForList<FloatPoint, SVGPointList>* obj = new JSSVGPODTypeWrapperCreatorForList<FloatPoint, SVGPointList>(listItem, imp);

    KJS::JSValue* result = toJS(exec, obj);
    setDOMException(exec, ec);

    ASSERT(imp->context());
    imp->context()->notifyAttributeChange();

    return result;
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
