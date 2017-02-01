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

#ifndef IDBIndexImpl_h
#define IDBIndexImpl_h

#include "IDBIndex.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBIndexInfo.h"

namespace WebCore {

struct IDBKeyRangeData;

namespace IDBClient {

class IDBObjectStore;

class IDBIndex : public WebCore::IDBIndex {
public:
    static Ref<IDBIndex> create(const IDBIndexInfo&, IDBObjectStore&);

    virtual ~IDBIndex();

    // Implement the IDL
    virtual const String& name() const override final;
    virtual RefPtr<WebCore::IDBObjectStore> objectStore() override final;
    virtual RefPtr<WebCore::IDBAny> keyPathAny() const override final;
    virtual const IDBKeyPath& keyPath() const override final;
    virtual bool unique() const override final;
    virtual bool multiEntry() const override final;

    virtual RefPtr<WebCore::IDBRequest> openCursor(ScriptExecutionContext* context, ExceptionCode& ec) override final { return openCursor(context, static_cast<IDBKeyRange*>(nullptr), ec); }
    virtual RefPtr<WebCore::IDBRequest> openCursor(ScriptExecutionContext* context, IDBKeyRange* keyRange, ExceptionCode& ec) override final { return openCursor(context, keyRange, IDBCursor::directionNext(), ec); }
    virtual RefPtr<WebCore::IDBRequest> openCursor(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCode& ec) override final { return openCursor(context, key, IDBCursor::directionNext(), ec); }
    virtual RefPtr<WebCore::IDBRequest> openCursor(ScriptExecutionContext*, IDBKeyRange*, const String& direction, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> openCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, const String& direction, ExceptionCode&) override final;

    virtual RefPtr<WebCore::IDBRequest> count(ScriptExecutionContext*, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> count(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> count(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCode&) override final;

    virtual RefPtr<WebCore::IDBRequest> openKeyCursor(ScriptExecutionContext* context, ExceptionCode& ec) override final { return openKeyCursor(context, static_cast<IDBKeyRange*>(nullptr), ec); }
    virtual RefPtr<WebCore::IDBRequest> openKeyCursor(ScriptExecutionContext* context, IDBKeyRange* keyRange, ExceptionCode& ec) override final { return openKeyCursor(context, keyRange, IDBCursor::directionNext(), ec); }
    virtual RefPtr<WebCore::IDBRequest> openKeyCursor(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCode& ec) override final { return openKeyCursor(context, key, IDBCursor::directionNext(), ec); }
    virtual RefPtr<WebCore::IDBRequest> openKeyCursor(ScriptExecutionContext*, IDBKeyRange*, const String& direction, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> openKeyCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, const String& direction, ExceptionCode&) override final;

    virtual RefPtr<WebCore::IDBRequest> get(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> get(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> getKey(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> getKey(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCode&) override final;

    const IDBIndexInfo& info() const { return m_info; }

    IDBObjectStore& modernObjectStore() { return m_objectStore.get(); }

    void markAsDeleted();
    bool isDeleted() const { return m_deleted; }

private:
    IDBIndex(const IDBIndexInfo&, IDBObjectStore&);

    RefPtr<WebCore::IDBRequest> doCount(ScriptExecutionContext&, const IDBKeyRangeData&, ExceptionCode&);
    RefPtr<WebCore::IDBRequest> doGet(ScriptExecutionContext&, const IDBKeyRangeData&, ExceptionCode&);
    RefPtr<WebCore::IDBRequest> doGetKey(ScriptExecutionContext&, const IDBKeyRangeData&, ExceptionCode&);

    IDBIndexInfo m_info;
    Ref<IDBObjectStore> m_objectStore;

    bool m_deleted { false };
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBIndexImpl_h