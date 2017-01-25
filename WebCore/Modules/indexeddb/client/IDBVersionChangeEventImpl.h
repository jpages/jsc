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

#ifndef IDBVersionChangeEventImpl_h
#define IDBVersionChangeEventImpl_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBVersionChangeEvent.h"

namespace WebCore {
namespace IDBClient {

class IDBVersionChangeEvent : public WebCore::IDBVersionChangeEvent {
public:
    static Ref<IDBVersionChangeEvent> create(uint64_t oldVersion = 0, uint64_t newVersion = 0, const AtomicString& eventType = AtomicString())
    {
        return adoptRef(*new IDBVersionChangeEvent(oldVersion, newVersion, eventType));
    }

    virtual uint64_t oldVersion() const override final { return m_oldVersion; }
    virtual uint64_t newVersion(bool& isNull) const override final;

    virtual EventInterface eventInterface() const override final;

private:
    IDBVersionChangeEvent(uint64_t oldVersion, uint64_t newVersion, const AtomicString& eventType);

    uint64_t m_oldVersion { 0 };
    uint64_t m_newVersion { 0 };
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBVersionChangeEventImpl_h
