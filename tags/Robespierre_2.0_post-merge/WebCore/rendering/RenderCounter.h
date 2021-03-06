/*
 * Copyright (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef RenderCounter_h
#define RenderCounter_h

#include "RenderText.h"

namespace WebCore {

class CounterNode;

class RenderCounter : public RenderText {
public:
    RenderCounter(Document*, const CounterContent&);

    virtual const char* renderName() const;
    virtual bool isRenderCounter() const;
    virtual PassRefPtr<StringImpl> originalText() const;
    
    virtual void dirtyLineBoxes(bool, bool);
    virtual void calcPrefWidths(int leadWidth);

    static void destroyCounterNodes(RenderObject*);

private:
    CounterContent m_counter;
    mutable CounterNode* m_counterNode;
};

} // namespace WebCore

#endif // RenderCounter_h
