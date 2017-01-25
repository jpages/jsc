/*
 * Copyright (C) 2012-2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NetworkResourceLoader.h"

#include "DataReference.h"
#include "Logging.h"
#include "NetworkBlobRegistry.h"
#include "NetworkCache.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkLoad.h"
#include "NetworkProcessConnectionMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebResourceLoaderMessages.h"
#include <WebCore/BlobDataFileReference.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SynchronousLoaderClient.h>
#include <wtf/CurrentTime.h>
#include <wtf/RunLoop.h>

using namespace WebCore;

namespace WebKit {

struct NetworkResourceLoader::SynchronousLoadData {
    SynchronousLoadData(RefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply>&& reply)
        : delayedReply(WTF::move(reply))
    {
        ASSERT(delayedReply);
    }
    ResourceRequest currentRequest;
    RefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply> delayedReply;
    ResourceResponse response;
    ResourceError error;
};

static void sendReplyToSynchronousRequest(NetworkResourceLoader::SynchronousLoadData& data, const SharedBuffer* buffer)
{
    ASSERT(data.delayedReply);
    ASSERT(!data.response.isNull() || !data.error.isNull());

    Vector<char> responseBuffer;
    if (buffer && buffer->size())
        responseBuffer.append(buffer->data(), buffer->size());

    data.delayedReply->send(data.error, data.response, responseBuffer);
    data.delayedReply = nullptr;
}

NetworkResourceLoader::NetworkResourceLoader(const NetworkResourceLoadParameters& parameters, NetworkConnectionToWebProcess& connection, RefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply>&& synchronousReply)
    : m_parameters(parameters)
    , m_connection(connection)
    , m_defersLoading(parameters.defersLoading)
    , m_bufferingTimer(*this, &NetworkResourceLoader::bufferingTimerFired)
{
    ASSERT(RunLoop::isMain());
    // FIXME: This is necessary because of the existence of EmptyFrameLoaderClient in WebCore.
    //        Once bug 116233 is resolved, this ASSERT can just be "m_webPageID && m_webFrameID"
    ASSERT((m_parameters.webPageID && m_parameters.webFrameID) || m_parameters.clientCredentialPolicy == DoNotAskClientForAnyCredentials);

    if (originalRequest().httpBody()) {
        for (const auto& element : originalRequest().httpBody()->elements()) {
            if (element.m_type == FormDataElement::Type::EncodedBlob)
                m_fileReferences.appendVector(NetworkBlobRegistry::singleton().filesInBlob(connection, element.m_url));
        }
    }

    if (originalRequest().url().protocolIs("blob")) {
        ASSERT(!m_parameters.resourceSandboxExtension);
        m_fileReferences.appendVector(NetworkBlobRegistry::singleton().filesInBlob(connection, originalRequest().url()));
    }

    if (synchronousReply)
        m_synchronousLoadData = std::make_unique<SynchronousLoadData>(WTF::move(synchronousReply));
}

NetworkResourceLoader::~NetworkResourceLoader()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_networkLoad);
    ASSERT(!isSynchronous() || !m_synchronousLoadData->delayedReply);
}

bool NetworkResourceLoader::isSynchronous() const
{
    return !!m_synchronousLoadData;
}

void NetworkResourceLoader::start()
{
    ASSERT(RunLoop::isMain());

    if (m_defersLoading)
        return;

#if ENABLE(NETWORK_CACHE)
    if (!NetworkCache::singleton().isEnabled() || sessionID().isEphemeral() || !originalRequest().url().protocolIsInHTTPFamily()) {
        startNetworkLoad();
        return;
    }

    RefPtr<NetworkResourceLoader> loader(this);
    NetworkCache::singleton().retrieve(originalRequest(), { m_parameters.webPageID, m_parameters.webFrameID }, [loader](std::unique_ptr<NetworkCache::Entry> entry) {
        if (loader->hasOneRef()) {
            // The loader has been aborted and is only held alive by this lambda.
            return;
        }
        if (!entry) {
            loader->startNetworkLoad();
            return;
        }
        if (loader->m_parameters.needsCertificateInfo && !entry->response().containsCertificateInfo()) {
            loader->startNetworkLoad();
            return;
        }
        if (entry->needsValidation()) {
            loader->validateCacheEntry(WTF::move(entry));
            return;
        }
        loader->didRetrieveCacheEntry(WTF::move(entry));
    });
#else
    startNetworkLoad();
#endif
}

void NetworkResourceLoader::startNetworkLoad(const Optional<ResourceRequest>& updatedRequest)
{
    consumeSandboxExtensions();

    if (isSynchronous() || m_parameters.maximumBufferingTime > 0_ms)
        m_bufferedData = SharedBuffer::create();

#if ENABLE(NETWORK_CACHE)
    if (NetworkCache::singleton().isEnabled())
        m_bufferedDataForCache = SharedBuffer::create();
#endif

    NetworkLoadParameters parameters = m_parameters;
    parameters.defersLoading = m_defersLoading;
    if (updatedRequest)
        parameters.request = updatedRequest.value();
    m_networkLoad = std::make_unique<NetworkLoad>(*this, parameters);
}

void NetworkResourceLoader::setDefersLoading(bool defers)
{
    if (m_defersLoading == defers)
        return;
    m_defersLoading = defers;

    if (m_networkLoad) {
        m_networkLoad->setDefersLoading(defers);
        return;
    }

    if (!m_defersLoading)
        start();
}

void NetworkResourceLoader::cleanup()
{
    ASSERT(RunLoop::isMain());

    m_bufferingTimer.stop();

    invalidateSandboxExtensions();

    m_networkLoad = nullptr;

    // This will cause NetworkResourceLoader to be destroyed and therefore we do it last.
    m_connection->didCleanupResourceLoader(*this);
}

#if !USE(NETWORK_SESSION)
void NetworkResourceLoader::didConvertHandleToDownload()
{
    ASSERT(m_networkLoad);
    m_didConvertHandleToDownload = true;
}
#endif

void NetworkResourceLoader::abort()
{
    ASSERT(RunLoop::isMain());

    if (m_networkLoad
#if !USE(NETWORK_SESSION)
        && !m_didConvertHandleToDownload
#endif
    ) {
        m_networkLoad->cancel();

#if ENABLE(NETWORK_CACHE)
        if (NetworkCache::singleton().isEnabled()) {
            // We might already have used data from this incomplete load. Ensure older versions don't remain in the cache after cancel.
            if (!m_response.isNull())
                NetworkCache::singleton().remove(originalRequest());
        }
#endif
    }

    cleanup();
}

auto NetworkResourceLoader::didReceiveResponse(const ResourceResponse& receivedResponse) -> ShouldContinueDidReceiveResponse
{
    m_response = receivedResponse;

    // For multipart/x-mixed-replace didReceiveResponseAsync gets called multiple times and buffering would require special handling.
    if (!isSynchronous() && m_response.isMultipart())
        m_bufferedData = nullptr;

    bool shouldSendDidReceiveResponse = true;
#if ENABLE(NETWORK_CACHE)
    if (m_response.isMultipart())
        m_bufferedDataForCache = nullptr;

    if (m_cacheEntryForValidation) {
        bool validationSucceeded = m_response.httpStatusCode() == 304; // 304 Not Modified
        if (validationSucceeded) {
            NetworkCache::singleton().update(originalRequest(), { m_parameters.webPageID, m_parameters.webFrameID }, *m_cacheEntryForValidation, m_response);
            // If the request was conditional then this revalidation was not triggered by the network cache and we pass the
            // 304 response to WebCore.
            if (originalRequest().isConditional())
                m_cacheEntryForValidation = nullptr;
        } else
            m_cacheEntryForValidation = nullptr;
    }
    shouldSendDidReceiveResponse = !m_cacheEntryForValidation;
#endif

    bool shouldWaitContinueDidReceiveResponse = originalRequest().requester() == ResourceRequest::Requester::Main;
    if (shouldSendDidReceiveResponse) {
        if (isSynchronous())
            m_synchronousLoadData->response = m_response;
        else {
            if (!sendAbortingOnFailure(Messages::WebResourceLoader::DidReceiveResponse(m_response, shouldWaitContinueDidReceiveResponse)))
                return ShouldContinueDidReceiveResponse::No;
        }
    }

    // For main resources, the web process is responsible for sending back a NetworkResourceLoader::ContinueDidReceiveResponse message.
    bool shouldContinueDidReceiveResponse = !shouldWaitContinueDidReceiveResponse;
#if ENABLE(NETWORK_CACHE)
    shouldContinueDidReceiveResponse = shouldContinueDidReceiveResponse || m_cacheEntryForValidation;
#endif

    return shouldContinueDidReceiveResponse ? ShouldContinueDidReceiveResponse::Yes : ShouldContinueDidReceiveResponse::No;
}

void NetworkResourceLoader::didReceiveBuffer(RefPtr<SharedBuffer>&& buffer, int reportedEncodedDataLength)
{
#if ENABLE(NETWORK_CACHE)
    ASSERT(!m_cacheEntryForValidation);

    if (m_bufferedDataForCache) {
        // Prevent memory growth in case of streaming data.
        const size_t maximumCacheBufferSize = 10 * 1024 * 1024;
        if (m_bufferedDataForCache->size() + buffer->size() <= maximumCacheBufferSize)
            m_bufferedDataForCache->append(buffer.get());
        else
            m_bufferedDataForCache = nullptr;
    }
#endif
    // FIXME: At least on OS X Yosemite we always get -1 from the resource handle.
    unsigned encodedDataLength = reportedEncodedDataLength >= 0 ? reportedEncodedDataLength : buffer->size();

    m_bytesReceived += buffer->size();
    if (m_bufferedData) {
        m_bufferedData->append(buffer.get());
        m_bufferedDataEncodedDataLength += encodedDataLength;
        startBufferingTimerIfNeeded();
        return;
    }
    sendBufferMaybeAborting(*buffer, encodedDataLength);
}

void NetworkResourceLoader::didFinishLoading(double finishTime)
{
#if ENABLE(NETWORK_CACHE)
    if (NetworkCache::singleton().isEnabled()) {
        if (m_cacheEntryForValidation) {
            // 304 Not Modified
            ASSERT(m_response.httpStatusCode() == 304);
            LOG(NetworkCache, "(NetworkProcess) revalidated");
            didRetrieveCacheEntry(WTF::move(m_cacheEntryForValidation));
            return;
        }
        bool allowStale = originalRequest().cachePolicy() >= ReturnCacheDataElseLoad;
        bool hasCacheableRedirect = m_response.isHTTP() && redirectChainAllowsReuse(m_redirectChainCacheStatus, allowStale ? ReuseExpiredRedirection : DoNotReuseExpiredRedirection);
        if (hasCacheableRedirect && m_redirectChainCacheStatus.status == RedirectChainCacheStatus::CachedRedirection) {
            // Maybe we should cache the actual redirects instead of the end result?
            auto now = std::chrono::system_clock::now();
            auto responseEndOfValidity = now + computeFreshnessLifetimeForHTTPFamily(m_response, now) - computeCurrentAge(m_response, now);
            hasCacheableRedirect = responseEndOfValidity <= m_redirectChainCacheStatus.endOfValidity;
        }

        bool isPrivateSession = sessionID().isEphemeral();
        if (m_bufferedDataForCache && hasCacheableRedirect && !isPrivateSession) {
            // Keep the connection alive.
            RefPtr<NetworkConnectionToWebProcess> connection(&connectionToWebProcess());
            RefPtr<NetworkResourceLoader> loader(this);
            NetworkCache::singleton().store(originalRequest(), m_response, WTF::move(m_bufferedDataForCache), [loader, connection](NetworkCache::MappedBody& mappedBody) {
#if ENABLE(SHAREABLE_RESOURCE)
                if (mappedBody.shareableResourceHandle.isNull())
                    return;
                LOG(NetworkCache, "(NetworkProcess) sending DidCacheResource");
                loader->send(Messages::NetworkProcessConnection::DidCacheResource(loader->originalRequest(), mappedBody.shareableResourceHandle, loader->sessionID()));
#endif
            });
        } else if (!hasCacheableRedirect) {
            // Make sure we don't keep a stale entry in the cache.
            NetworkCache::singleton().remove(originalRequest());
        }
    }
#endif

    if (isSynchronous())
        sendReplyToSynchronousRequest(*m_synchronousLoadData, m_bufferedData.get());
    else {
        if (m_bufferedData && !m_bufferedData->isEmpty()) {
            // FIXME: Pass a real value or remove the encoded data size feature.
            bool shouldContinue = sendBufferMaybeAborting(*m_bufferedData, -1);
            if (!shouldContinue)
                return;
        }
        send(Messages::WebResourceLoader::DidFinishResourceLoad(finishTime));
    }

    cleanup();
}

void NetworkResourceLoader::didFailLoading(const ResourceError& error)
{
    ASSERT(!error.isNull());

#if ENABLE(NETWORK_CACHE)
    m_cacheEntryForValidation = nullptr;
#endif

    if (isSynchronous()) {
        m_synchronousLoadData->error = error;
        sendReplyToSynchronousRequest(*m_synchronousLoadData, nullptr);
    } else
        send(Messages::WebResourceLoader::DidFailResourceLoad(error));

    cleanup();
}

void NetworkResourceLoader::willSendRedirectedRequest(const ResourceRequest& request, const ResourceResponse& redirectResponse)
{
#if ENABLE(NETWORK_CACHE)
    updateRedirectChainStatus(m_redirectChainCacheStatus, redirectResponse);
#endif

    if (isSynchronous()) {
        ResourceRequest overridenRequest = request;
        // FIXME: This needs to be fixed to follow the redirect correctly even for cross-domain requests.
        // This includes at least updating host records, and comparing the current request instead of the original request here.
        if (!protocolHostAndPortAreEqual(originalRequest().url(), request.url())) {
            ASSERT(m_synchronousLoadData->error.isNull());
            m_synchronousLoadData->error = SynchronousLoaderClient::platformBadResponseError();
            m_networkLoad->clearCurrentRequest();
            overridenRequest = ResourceRequest();
        }
        continueWillSendRequest(overridenRequest);
        return;
    }
    sendAbortingOnFailure(Messages::WebResourceLoader::WillSendRequest(request, redirectResponse));
}
    
void NetworkResourceLoader::continueWillSendRequest(const ResourceRequest& newRequest)
{
    m_networkLoad->continueWillSendRequest(newRequest);
}

void NetworkResourceLoader::continueDidReceiveResponse()
{
    // FIXME: Remove this check once BlobResourceHandle implements didReceiveResponseAsync correctly.
    // Currently, it does not wait for response, so the load is likely to finish before continueDidReceiveResponse.
    if (m_networkLoad)
        m_networkLoad->continueDidReceiveResponse();
}

void NetworkResourceLoader::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    if (!isSynchronous())
        send(Messages::WebResourceLoader::DidSendData(bytesSent, totalBytesToBeSent));
}

void NetworkResourceLoader::startBufferingTimerIfNeeded()
{
    if (isSynchronous())
        return;
    if (m_bufferingTimer.isActive())
        return;
    m_bufferingTimer.startOneShot(m_parameters.maximumBufferingTime);
}

void NetworkResourceLoader::bufferingTimerFired()
{
    ASSERT(m_bufferedData);
    ASSERT(m_networkLoad);

    if (m_bufferedData->isEmpty())
        return;

    IPC::SharedBufferDataReference dataReference(m_bufferedData.get());
    size_t encodedLength = m_bufferedDataEncodedDataLength;

    m_bufferedData = SharedBuffer::create();
    m_bufferedDataEncodedDataLength = 0;

    sendAbortingOnFailure(Messages::WebResourceLoader::DidReceiveData(dataReference, encodedLength));
}

bool NetworkResourceLoader::sendBufferMaybeAborting(SharedBuffer& buffer, size_t encodedDataLength)
{
    ASSERT(!isSynchronous());

#if PLATFORM(COCOA)
    ShareableResource::Handle shareableResourceHandle;
    NetworkResourceLoader::tryGetShareableHandleFromSharedBuffer(shareableResourceHandle, buffer);
    if (!shareableResourceHandle.isNull()) {
        send(Messages::WebResourceLoader::DidReceiveResource(shareableResourceHandle, currentTime()));
        abort();
        return false;
    }
#endif

    IPC::SharedBufferDataReference dataReference(&buffer);
    return sendAbortingOnFailure(Messages::WebResourceLoader::DidReceiveData(dataReference, encodedDataLength));
}

#if ENABLE(NETWORK_CACHE)
void NetworkResourceLoader::didRetrieveCacheEntry(std::unique_ptr<NetworkCache::Entry> entry)
{
    if (isSynchronous()) {
        m_synchronousLoadData->response = entry->response();
        sendReplyToSynchronousRequest(*m_synchronousLoadData, entry->buffer());
    } else {
        if (entry->response().url() != originalRequest().url()) {
            // This is a cached redirect. Synthesize a minimal redirect so we get things like referer header right.
            // FIXME: We should cache the actual redirects.
            ResourceRequest syntheticRedirectRequest(entry->response().url());
            ResourceResponse syntheticRedirectResponse(originalRequest().url(), { }, 0, { });
            sendAbortingOnFailure(Messages::WebResourceLoader::WillSendRequest(syntheticRedirectRequest, syntheticRedirectResponse));
        }

        bool needsContinueDidReceiveResponseMessage = originalRequest().requester() == ResourceRequest::Requester::Main;
        sendAbortingOnFailure(Messages::WebResourceLoader::DidReceiveResponse(entry->response(), needsContinueDidReceiveResponseMessage));

#if ENABLE(SHAREABLE_RESOURCE)
        if (!entry->shareableResourceHandle().isNull())
            send(Messages::WebResourceLoader::DidReceiveResource(entry->shareableResourceHandle(), currentTime()));
        else {
#endif
            bool shouldContinue = sendBufferMaybeAborting(*entry->buffer(), entry->buffer()->size());
            if (!shouldContinue)
                return;
            send(Messages::WebResourceLoader::DidFinishResourceLoad(currentTime()));
#if ENABLE(SHAREABLE_RESOURCE)
        }
#endif
    }

    cleanup();
}

void NetworkResourceLoader::validateCacheEntry(std::unique_ptr<NetworkCache::Entry> entry)
{
    ASSERT(!m_networkLoad);

    // If the request is already conditional then the revalidation was not triggered by the disk cache
    // and we should not overwrite the existing conditional headers.
    ResourceRequest revalidationRequest = originalRequest();
    if (!revalidationRequest.isConditional()) {
        String eTag = entry->response().httpHeaderField(HTTPHeaderName::ETag);
        String lastModified = entry->response().httpHeaderField(HTTPHeaderName::LastModified);
        if (!eTag.isEmpty())
            revalidationRequest.setHTTPHeaderField(HTTPHeaderName::IfNoneMatch, eTag);
        if (!lastModified.isEmpty())
            revalidationRequest.setHTTPHeaderField(HTTPHeaderName::IfModifiedSince, lastModified);
    }

    m_cacheEntryForValidation = WTF::move(entry);

    startNetworkLoad(revalidationRequest);
}
#endif

IPC::Connection* NetworkResourceLoader::messageSenderConnection()
{
    return connectionToWebProcess().connection();
}

void NetworkResourceLoader::consumeSandboxExtensions()
{
    ASSERT(!m_didConsumeSandboxExtensions);

    for (auto& extension : m_parameters.requestBodySandboxExtensions)
        extension->consume();

    if (auto& extension = m_parameters.resourceSandboxExtension)
        extension->consume();

    for (auto& fileReference : m_fileReferences)
        fileReference->prepareForFileAccess();

    m_didConsumeSandboxExtensions = true;
}

void NetworkResourceLoader::invalidateSandboxExtensions()
{
    if (m_didConsumeSandboxExtensions) {
        for (auto& extension : m_parameters.requestBodySandboxExtensions)
            extension->revoke();
        if (auto& extension = m_parameters.resourceSandboxExtension)
            extension->revoke();
        for (auto& fileReference : m_fileReferences)
            fileReference->revokeFileAccess();

        m_didConsumeSandboxExtensions = false;
    }

    m_fileReferences.clear();
}

template<typename T>
bool NetworkResourceLoader::sendAbortingOnFailure(T&& message, unsigned messageSendFlags)
{
    bool result = messageSenderConnection()->send(std::forward<T>(message), messageSenderDestinationID(), messageSendFlags);
    if (!result)
        abort();
    return result;
}

void NetworkResourceLoader::canAuthenticateAgainstProtectionSpaceAsync(const ProtectionSpace& protectionSpace)
{
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    sendAbortingOnFailure(Messages::WebResourceLoader::CanAuthenticateAgainstProtectionSpace(protectionSpace));
#else
    UNUSED_PARAM(protectionSpace);
#endif
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void NetworkResourceLoader::continueCanAuthenticateAgainstProtectionSpace(bool result)
{
    m_networkLoad->continueCanAuthenticateAgainstProtectionSpace(result);
}
#endif

} // namespace WebKit
