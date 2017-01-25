/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef NetworkSession_h
#define NetworkSession_h

OBJC_CLASS NSURLSession;
OBJC_CLASS NSURLSessionDataTask;
OBJC_CLASS NSOperationQueue;
OBJC_CLASS NetworkSessionDelegate;

#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class AuthenticationChallenge;
class Credential;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class SharedBuffer;
}

namespace WebKit {

enum class AuthenticationChallengeDisposition {
    UseCredential,
    PerformDefaultHandling,
    Cancel,
    RejectProtectionSpace
};

class NetworkSession;

class NetworkSessionTaskClient {
public:
    virtual void willPerformHTTPRedirection(const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, std::function<void(const WebCore::ResourceRequest&)>) = 0;
    virtual void didReceiveChallenge(const WebCore::AuthenticationChallenge&, std::function<void(AuthenticationChallengeDisposition, const WebCore::Credential&)>) = 0;
    virtual void didReceiveResponse(const WebCore::ResourceResponse&, std::function<void(WebCore::PolicyAction)>) = 0;
    virtual void didReceiveData(RefPtr<WebCore::SharedBuffer>&&) = 0;
    virtual void didCompleteWithError(const WebCore::ResourceError&) = 0;

    virtual ~NetworkSessionTaskClient() { }
};

class NetworkDataTask : public RefCounted<NetworkDataTask> {
    friend class NetworkSession;
public:
    void cancel();
    void resume();

    uint64_t taskIdentifier();

    ~NetworkDataTask();

    NetworkSessionTaskClient* client() { return m_client; }
    void clearClient() { m_client = nullptr; }

private:
    NetworkSession& m_session;
    NetworkSessionTaskClient* m_client;
#if PLATFORM(COCOA)
    explicit NetworkDataTask(NetworkSession&, NetworkSessionTaskClient&, RetainPtr<NSURLSessionDataTask>&&);
    RetainPtr<NSURLSessionDataTask> m_task;
#else
    explicit NetworkDataTask(NetworkSession&, NetworkSessionTaskClient&);
#endif
};

class NetworkSession {
    friend class NetworkDataTask;
public:
    enum class Type {
        Normal,
        Ephemeral
    };
    NetworkSession(Type, WebCore::SessionID);
    ~NetworkSession() { ASSERT(m_dataTaskMap.isEmpty()); }

    static NetworkSession& defaultSession();
    
    Ref<NetworkDataTask> createDataTaskWithRequest(const WebCore::ResourceRequest&, NetworkSessionTaskClient&);

    NetworkDataTask* dataTaskForIdentifier(uint64_t);

private:
    WebCore::SessionID m_sessionID;
    HashMap<uint64_t, NetworkDataTask*> m_dataTaskMap;
#if PLATFORM(COCOA)
    RetainPtr<NSURLSession> m_session;
    RetainPtr<NetworkSessionDelegate> m_sessionDelegate;
#endif
};

} // namespace WebKit

#endif // NetworkSession_h
