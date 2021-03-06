/*
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
#ifndef IntSizeHash_h
#define IntSizeHash_h

#include "IntSize.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

using WebCore::IntSize;

namespace WTF {

    template<> struct IntHash<IntSize> {
        static unsigned hash(const IntSize& key) { return intHash((static_cast<uint64_t>(key.width()) << 32 | key.height())); }
        static bool equal(const IntSize& a, const IntSize& b) { return a == b; }
    };
    template<> struct DefaultHash<IntSize> { typedef IntHash<IntSize> Hash; };
    
    template<> struct HashTraits<IntSize> : GenericHashTraits<IntSize> {
        static const bool emptyValueIsZero = true;
        static const bool needsDestruction = false;
        static const bool needsRef = false;
        static IntSize deletedValue() { return IntSize(-1, -1); }
    };
} // namespace WTF

#endif
