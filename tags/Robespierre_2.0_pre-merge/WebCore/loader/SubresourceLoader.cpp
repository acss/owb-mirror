/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SubresourceLoader.h"

#include "Document.h"
#include "Frame.h"
#include "Logging.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "SubresourceLoaderClient.h"
#include "SharedBuffer.h"
#include "FrameLoader.h"

namespace WebCore {

#ifndef NDEBUG
WTFLogChannel LogWebCoreSubresourceLoaderLeaks =  { 0x00000000, "", WTFLogChannelOn };

struct SubresourceLoaderCounter {
    static unsigned count; 

    ~SubresourceLoaderCounter() 
    { 
        if (count) 
            LOG(WebCoreSubresourceLoaderLeaks, "LEAK: %u SubresourceLoader\n", count); 
    }
};
unsigned SubresourceLoaderCounter::count = 0;
static SubresourceLoaderCounter subresourceLoaderCounter;
#endif

SubresourceLoader::SubresourceLoader(Frame* frame, SubresourceLoaderClient* client)
    : ResourceLoader(frame)
    , m_client(client)
    , m_loadingMultipartContent(false)
{
#ifndef NDEBUG
    ++SubresourceLoaderCounter::count;
#endif
    frameLoader()->addSubresourceLoader(this);
}

SubresourceLoader::~SubresourceLoader()
{
#ifndef NDEBUG
    --SubresourceLoaderCounter::count;
#endif
}

bool SubresourceLoader::load(const ResourceRequest& r)
{
    m_frame->loader()->didTellBridgeAboutLoad(r.url().url());
    
    return ResourceLoader::load(r);
}

PassRefPtr<SubresourceLoader> SubresourceLoader::create(Frame* frame, SubresourceLoaderClient* client, const ResourceRequest& request)
{
    if (!frame)
        return 0;

    FrameLoader* fl = frame->loader();
    if (fl->state() == FrameStateProvisional)
        return 0;

    ResourceRequest newRequest = request;
    
    // Since this is a subresource, we can load any URL (we ignore the return value).
    // But we still want to know whether we should hide the referrer or not, so we call the canLoadURL method.
    // FIXME: is that really the rule we want for subresources?
    bool hideReferrer;
    fl->canLoad(request.url(), fl->outgoingReferrer(), hideReferrer);
    if (hideReferrer)
        newRequest.clearHTTPReferrer();
    else if (!request.httpReferrer())
        newRequest.setHTTPReferrer(fl->outgoingReferrer());

    // Use the original request's cache policy for two reasons:
    // 1. For POST requests, we mutate the cache policy for the main resource,
    //    but we do not want this to apply to subresources
    // 2. Delegates that modify the cache policy using willSendRequest: should
    //    not affect any other resources. Such changes need to be done
    //    per request.
    if (newRequest.isConditional())
        newRequest.setCachePolicy(ReloadIgnoringCacheData);
    else
        newRequest.setCachePolicy(fl->originalRequest().cachePolicy());

    fl->addExtraFieldsToRequest(newRequest, false, false);

    RefPtr<SubresourceLoader> subloader(new SubresourceLoader(frame, client));
    if (!subloader->load(newRequest))
        return 0;

    return subloader.release();
}

void SubresourceLoader::willSendRequest(ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    ResourceLoader::willSendRequest(newRequest, redirectResponse);
    if (!newRequest.isNull() && m_originalURL != newRequest.url() && m_client)
        m_client->willSendRequest(this, newRequest, redirectResponse);
}

void SubresourceLoader::didReceiveResponse(const ResourceResponse& r)
{
    ASSERT(!r.isNull());

    if (r.isMultipart())
        m_loadingMultipartContent = true;

    // Reference the object in this method since the additional processing can do
    // anything including removing the last reference to this object; one example of this is 3266216.
    RefPtr<SubresourceLoader> protect(this);

    if (m_client)
        m_client->didReceiveResponse(this, r);
    
    // The loader can cancel a load if it receives a multipart response for a non-image
    if (reachedTerminalState())
        return;
    ResourceLoader::didReceiveResponse(r);
    
    RefPtr<SharedBuffer> buffer = resourceData();
    if (m_loadingMultipartContent && buffer && buffer->size()) {
        // Since a subresource loader does not load multipart sections progressively,
        // deliver the previously received data to the loader all at once now.
        // Then clear the data to make way for the next multipart section.
        if (m_client)
            m_client->didReceiveData(this, buffer->data(), buffer->size());
        clearResourceData();
        
        // After the first multipart section is complete, signal to delegates that this load is "finished" 
        didFinishLoadingOnePart();
    }
}

void SubresourceLoader::didReceiveData(const char* data, int length, long long lengthReceived, bool allAtOnce)
{
    // Reference the object in this method since the additional processing can do
    // anything including removing the last reference to this object; one example of this is 3266216.
    RefPtr<SubresourceLoader> protect(this);

    // A subresource loader does not load multipart sections progressively.
    // So don't deliver any data to the loader yet.
    if (!m_loadingMultipartContent && m_client)
        m_client->didReceiveData(this, data, length);

    ResourceLoader::didReceiveData(data, length, lengthReceived, allAtOnce);
}

void SubresourceLoader::didFinishLoading()
{
    if (cancelled())
        return;
    ASSERT(!reachedTerminalState());

    // Calling removeSubresourceLoader will likely result in a call to deref, so we must protect ourselves.
    RefPtr<SubresourceLoader> protect(this);

    if (m_client)
        m_client->didFinishLoading(this);
    
    m_handle = 0;

    if (cancelled())
        return;
    frameLoader()->removeSubresourceLoader(this);
    ResourceLoader::didFinishLoading();
}

void SubresourceLoader::didFail(const ResourceError& error)
{
    if (cancelled())
        return;
    ASSERT(!reachedTerminalState());

    // Calling removeSubresourceLoader will likely result in a call to deref, so we must protect ourselves.
    RefPtr<SubresourceLoader> protect(this);

    if (m_client)
        m_client->didFail(this, error);
    
    m_handle = 0;
    
    if (cancelled())
        return;
    frameLoader()->removeSubresourceLoader(this);
    ResourceLoader::didFail(error);
}

void SubresourceLoader::didCancel(const ResourceError& error)
{
    ASSERT(!reachedTerminalState());

    // Calling removeSubresourceLoader will likely result in a call to deref, so we must protect ourselves.
    RefPtr<SubresourceLoader> protect(this);

    if (m_client)
        m_client->didFail(this, error);
    
    if (cancelled())
        return;
    frameLoader()->removeSubresourceLoader(this);
    ResourceLoader::didCancel(error);
}

void SubresourceLoader::stopLoading()
{
    // FIXME: This should stop loading for real and not just clear the client.
    m_client = 0;
}
    
}
