/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLCanvasElement.h"
#include "CanvasRenderingContext.h"

#include "CanvasRenderingContext.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8Node.h"
#include "V8Proxy.h"

namespace WebCore {

CALLBACK_FUNC_DECL(HTMLCanvasElementGetContext)
{
    INC_STATS("DOM.HTMLCanvasElement.context");
    v8::Handle<v8::Object> holder = args.Holder();
    HTMLCanvasElement* imp = V8DOMWrapper::convertDOMWrapperToNode<HTMLCanvasElement>(holder);
    String contextId = toWebCoreString(args[0]);
    CanvasRenderingContext* result = imp->getContext(contextId);
    if (!result)
        return v8::Undefined();
    if (result->is2d())
        return V8DOMWrapper::convertToV8Object(V8ClassIndex::CANVASRENDERINGCONTEXT2D, result);
#if ENABLE(3D_CANVAS)
    else if (result->is3d())
        return V8DOMWrapper::convertToV8Object(V8ClassIndex::CANVASRENDERINGCONTEXT3D, result);
#endif
    ASSERT_NOT_REACHED();
    return v8::Undefined();
}

} // namespace WebCore

