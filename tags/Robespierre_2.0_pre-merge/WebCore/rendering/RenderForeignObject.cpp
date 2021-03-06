/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
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
 *
 */

#include "config.h"

#ifdef SVG_SUPPORT

#include "RenderForeignObject.h"

#include "GraphicsContext.h"
#include "SVGForeignObjectElement.h"
#include "SVGLength.h"

namespace WebCore {

RenderForeignObject::RenderForeignObject(SVGForeignObjectElement* node) 
    : RenderSVGBlock(node)
{
}

#ifdef __OWB__
BAL::BTAffineTransform RenderForeignObject::translationForAttributes()
#else
AffineTransform RenderForeignObject::translationForAttributes()
#endif
{
    SVGForeignObjectElement* foreign = static_cast<SVGForeignObjectElement*>(element());
#ifdef __OWB__
    return BAL::BTAffineTransform().translate(foreign->x().value(), foreign->y().value());
#else
    return AffineTransform().translate(foreign->x().value(), foreign->y().value());
#endif
}

void RenderForeignObject::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.context->paintingDisabled())
        return;

    paintInfo.context->save();
    paintInfo.context->concatCTM(AffineTransform().translate(parentX, parentY));
    paintInfo.context->concatCTM(localTransform());
    paintInfo.context->concatCTM(translationForAttributes());
    paintInfo.context->clip(getClipRect(parentX, parentY));

    float opacity = style()->opacity();
    if (opacity < 1.0f)
        // FIXME: Possible optimization by clipping to bbox here, once relativeBBox is implemented & clip, mask and filter support added.
        paintInfo.context->beginTransparencyLayer(opacity);

    PaintInfo pi(paintInfo);
    pi.rect = absoluteTransform().inverse().mapRect(paintInfo.rect);
    RenderBlock::paint(pi, 0, 0);

    if (opacity < 1.0f)
        paintInfo.context->endTransparencyLayer();

    paintInfo.context->restore();
}

void RenderForeignObject::computeAbsoluteRepaintRect(IntRect& r, bool f)
{
#ifdef __OWB__
    BAL::BTAffineTransform transform = translationForAttributes() * localTransform();
#else
    AffineTransform transform = translationForAttributes() * localTransform();
#endif
    r = transform.mapRect(r);

    RenderBlock::computeAbsoluteRepaintRect(r, f);
}

bool RenderForeignObject::requiresLayer()
{
    return false;
}

void RenderForeignObject::layout()
{
    ASSERT(needsLayout());
    ASSERT(minMaxKnown());

    IntRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        oldBounds = m_absoluteBounds;

    RenderBlock::layout();

    m_absoluteBounds = getAbsoluteRepaintRect();

    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds);

    setNeedsLayout(false);
}

bool RenderForeignObject::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty, HitTestAction hitTestAction)
{
#ifdef __OWB__
    BAL::BTAffineTransform totalTransform = absoluteTransform();
#else
    AffineTransform totalTransform = absoluteTransform();
#endif
    totalTransform *= translationForAttributes();
    double localX, localY;
    totalTransform.inverse().map(x, y, &localX, &localY);
    return RenderBlock::nodeAtPoint(request, result, static_cast<int>(localX), static_cast<int>(localY), tx, ty, hitTestAction);
}

} // namespace WebCore

#endif // SVG_SUPPORT
