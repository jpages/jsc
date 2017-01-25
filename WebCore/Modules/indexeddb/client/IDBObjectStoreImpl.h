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

#ifndef IDBObjectStoreImpl_h
#define IDBObjectStoreImpl_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBIndexImpl.h"
#include "IDBObjectStore.h"
#include "IDBObjectStoreInfo.h"
#include "IndexedDB.h"

namespace WebCore {

class IDBKey;

struct IDBKeyRangeData;

namespace IDBClient {

class IDBTransaction;

class IDBObjectStore : public WebCore::IDBObjectStore {
public:
    static Ref<IDBObjectStore> create(const IDBObjectStoreInfo&, IDBTransaction&);

    virtual ~IDBObjectStore() override final;

    // Implement the IDBObjectStore IDL
    virtual const String name() const override final;
    virtual RefPtr<WebCore::IDBAny> keyPathAny() const override final;
    virtual const IDBKeyPath keyPath() const override final;
    virtual RefPtr<DOMStringList> indexNames() const override final;
    virtual RefPtr<WebCore::IDBTransaction> transaction() override final;
    virtual bool autoIncrement() const override final;

    virtual RefPtr<WebCore::IDBRequest> add(JSC::ExecState&, JSC::JSValue, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> put(JSC::ExecState&, JSC::JSValue, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> openCursor(ScriptExecutionContext*, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> openCursor(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> openCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> openCursor(ScriptExecutionContext*, IDBKeyRange*, const String& direction, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> openCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, const String& direction, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> get(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> get(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> add(JSC::ExecState&, JSC::JSValue, JSC::JSValue key, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> put(JSC::ExecState&, JSC::JSValue, JSC::JSValue key, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> deleteFunction(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> deleteFunction(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> clear(ScriptExecutionContext*, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBIndex> createIndex(ScriptExecutionContext*, const String& name, const IDBKeyPath&, bool unique, bool multiEntry, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBIndex> index(const String& name, ExceptionCode&) override final;
    virtual void deleteIndex(const String& name, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> count(ScriptExecutionContext*, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> count(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&) override final;
    virtual RefPtr<WebCore::IDBRequest> count(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCode&) override final;

    RefPtr<WebCore::IDBRequest> deleteFunction(ScriptExecutionContext*, JSC::JSValue key, ExceptionCode&);

    void markAsDeleted();
    bool isDeleted() const { return m_deleted; }

    const IDBObjectStoreInfo& info() const { return m_info; }

    // FIXME: After removing LegacyIDB and folding abstract/implementation classes together,
    // this will no longer be necessary.
    IDBTransaction& modernTransaction() { return m_transaction.get(); }

private:
    IDBObjectStore(const IDBObjectStoreInfo&, IDBTransaction&);

    RefPtr<WebCore::IDBRequest> putOrAdd(JSC::ExecState&, JSC::JSValue, RefPtr<IDBKey>, IndexedDB::ObjectStoreOverwriteMode, ExceptionCode&);
    RefPtr<WebCore::IDBRequest> doCount(ScriptExecutionContext&, const IDBKeyRangeData&, ExceptionCode&);

    IDBObjectStoreInfo m_info;
    Ref<IDBTransaction> m_transaction;

    bool m_deleted { false };

    HashMap<String, RefPtr<IDBIndex>> m_referencedIndexes;
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBObjectStoreImpl_h
