/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Holger Hans Peter Freyther <zecke@selfish.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageBuffer.h"

#include "BitmapImage.h"
#include "GraphicsContext.h"
#include "ImageData.h"
#include "NotImplemented.h"
#include "Pattern.h"

#include <cairo.h>

using namespace std;

namespace WKAL {

auto_ptr<ImageBuffer> ImageBuffer::create(const IntSize& size, bool)
{
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                          size.width(), size.height());
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
        return auto_ptr<ImageBuffer>();

    return auto_ptr<ImageBuffer>(new ImageBuffer(surface));
}

ImageBuffer::ImageBuffer(_cairo_surface* surface)
    : m_surface(surface)
{
    cairo_t* cr = cairo_create(m_surface);
    m_context.set(new GraphicsContext(cr));

    /*
     * The context is now owned by the GraphicsContext
     */
    cairo_destroy(cr);
}

ImageBuffer::~ImageBuffer()
{
    cairo_surface_destroy(m_surface);
}

GraphicsContext* ImageBuffer::context() const
{
    return m_context.get();
}

Image* ImageBuffer::image() const
{
    if (!m_image) {
        // It's assumed that if image() is called, the actual rendering to the
        // GraphicsContext must be done.
        ASSERT(context());
        // BitmapImage will release the passed in surface on destruction
        m_image = BitmapImage::create(cairo_surface_reference(m_surface));
    }
    return m_image.get();
}

PassRefPtr<ImageData> ImageBuffer::getImageData(const IntRect&) const
{
    notImplemented();
    return 0;
}

void ImageBuffer::putImageData(ImageData*, const IntRect&, const IntPoint&)
{
    notImplemented();
}

String ImageBuffer::toDataURL(const String&) const
{
    notImplemented();
    return String();
}

} // namespace WebCore
