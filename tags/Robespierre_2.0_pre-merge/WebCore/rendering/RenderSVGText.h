/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef RenderSVGText_h
#define RenderSVGText_h

#ifdef SVG_SUPPORT

#include "AffineTransform.h"
#include "RenderSVGBlock.h"

namespace WebCore {

class SVGTextElement;

class RenderSVGText : public RenderSVGBlock {
public:
    RenderSVGText(SVGTextElement* node);

    virtual const char* renderName() const { return "RenderSVGText"; }
    
#ifdef __OWB__
    virtual BAL::BTAffineTransform localTransform() const { return m_transform; }
    virtual void setLocalTransform(const BAL::BTAffineTransform& transform) { m_transform = transform; }
#else
    virtual AffineTransform localTransform() const { return m_transform; }
    virtual void setLocalTransform(const AffineTransform& transform) { m_transform = transform; }
#endif
    virtual void paint(PaintInfo&, int tx, int ty);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);
    virtual void absoluteRects(Vector<IntRect>&, int tx, int ty);
    virtual IntRect getAbsoluteRepaintRect();
    virtual bool requiresLayer();
    virtual void layout();
    virtual FloatRect relativeBBox(bool includeStroke = true) const;
    virtual InlineBox* createInlineBox(bool makePlaceHolderBox, bool isRootLineBox, bool isOnlyRun = false);
    virtual bool isSVGText() const { return true; }

private:
#ifdef __OWB__
    BAL::BTAffineTransform m_transform;
#else
    AffineTransform m_transform;
#endif
    IntRect m_absoluteBounds;
};

}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
