/*
 Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 
 This file is part of the WebKit project
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 Boston, MA 02111-1307, USA.
 */

#ifndef SVGMPathElement_h
#define SVGMPathElement_h
#if ENABLE(SVG)

#include "SVGURIReference.h"
#include "SVGExternalResourcesRequired.h"

namespace WebCore {
    
    class SVGPathElement;
    
    class SVGMPathElement : public SVGElement,
                                   SVGURIReference,
                                   SVGExternalResourcesRequired
    {
    public:
        SVGMPathElement(const QualifiedName&, Document*);
        virtual ~SVGMPathElement();
        
        virtual void parseMappedAttribute(MappedAttribute*);
        
        SVGPathElement* pathElement();
        
    protected:
        virtual const SVGElement* contextElement() const { return this; }
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGMPathElement_h

// vim:ts=4:noet
