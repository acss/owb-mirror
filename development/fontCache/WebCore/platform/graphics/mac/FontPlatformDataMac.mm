/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2006-7 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#import "config.h"
#import "FontPlatformData.h"

#import "WebCoreSystemInterface.h"

namespace WebCore {

FontPlatformData::FontPlatformData(NSFont *f, bool b , bool o)
: m_syntheticBold(b), m_syntheticOblique(o), m_font(f)
{
    if (f)
        CFRetain(f);
    m_size = f ? [f pointSize] : 0.0f;
#ifndef BUILDING_ON_TIGER
    m_cgFont = CTFontCopyGraphicsFont(toCTFontRef(f), 0);
    m_atsuFontID = CTFontGetPlatformFont(toCTFontRef(f), 0);
#else
    m_cgFont = wkGetCGFontFromNSFont(f);
    m_atsuFontID = wkGetNSFontATSUFontId(f);
#endif
}

FontPlatformData::FontPlatformData(const FontPlatformData& f)
{
    m_font = f.m_font && f.m_font != reinterpret_cast<NSFont *>(-1) ? static_cast<const NSFont *>(CFRetain(f.m_font)) : f.m_font;
    m_syntheticBold = f.m_syntheticBold;
    m_syntheticOblique = f.m_syntheticOblique;
    m_size = f.m_size;
    m_cgFont = f.m_cgFont;
    m_atsuFontID = f.m_atsuFontID;
}

FontPlatformData:: ~FontPlatformData()
{
    if (m_font && m_font != reinterpret_cast<NSFont *>(-1))
        CFRelease(m_font);
}

const FontPlatformData& FontPlatformData::operator=(const FontPlatformData& f)
{
    m_syntheticBold = f.m_syntheticBold;
    m_syntheticOblique = f.m_syntheticOblique;
    m_size = f.m_size;
    m_cgFont = f.m_cgFont;
    m_atsuFontID = f.m_atsuFontID;
    if (m_font == f.m_font)
        return *this;
    if (f.m_font && f.m_font != reinterpret_cast<NSFont *>(-1))
        CFRetain(f.m_font);
    if (m_font && m_font != reinterpret_cast<NSFont *>(-1))
        CFRelease(m_font);
    m_font = f.m_font;
    return *this;
}

void FontPlatformData::setFont(NSFont *font) {
    if (m_font == font)
        return;
    if (font)
        CFRetain(font);
    if (m_font && m_font != reinterpret_cast<NSFont *>(-1))
        CFRelease(m_font);
    m_font = font;
    m_size = font ? [font pointSize] : 0.0f;
#ifndef BUILDING_ON_TIGER
    m_cgFont = CTFontCopyGraphicsFont(toCTFontRef(font), 0);
    m_atsuFontID = CTFontGetPlatformFont(toCTFontRef(font), 0);
#else
    m_cgFont = wkGetCGFontFromNSFont(font);
    m_atsuFontID = wkGetNSFontATSUFontId(font);
#endif
}

}

