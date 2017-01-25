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

#include "config.h"
#include "IDBObjectStoreImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMRequestState.h"
#include "IDBBindingUtilities.h"
#include "IDBCursorImpl.h"
#include "IDBDatabaseException.h"
#include "IDBError.h"
#include "IDBIndexImpl.h"
#include "IDBKey.h"
#include "IDBKeyRangeData.h"
#include "IDBRequestImpl.h"
#include "IDBTransactionImpl.h"
#include "IndexedDB.h"
#include "Logging.h"
#include "SerializedScriptValue.h"

namespace WebCore {
namespace IDBClient {

Ref<IDBObjectStore> IDBObjectStore::create(const IDBObjectStoreInfo& info, IDBTransaction& transaction)
{
    return adoptRef(*new IDBObjectStore(info, transaction));
}

IDBObjectStore::IDBObjectStore(const IDBObjectStoreInfo& info, IDBTransaction& transaction)
    : m_info(info)
    , m_transaction(transaction)
{
}

IDBObjectStore::~IDBObjectStore()
{
}

const String IDBObjectStore::name() const
{
    return m_info.name();
}

RefPtr<WebCore::IDBAny> IDBObjectStore::keyPathAny() const
{
    return IDBAny::create(m_info.keyPath());
}

const IDBKeyPath IDBObjectStore::keyPath() const
{
    return m_info.keyPath();
}

RefPtr<DOMStringList> IDBObjectStore::indexNames() const
{
    RefPtr<DOMStringList> indexNames = DOMStringList::create();
    for (auto& name : m_info.indexNames())
        indexNames->append(name);
    indexNames->sort();

    return indexNames;
}

RefPtr<WebCore::IDBTransaction> IDBObjectStore::transaction()
{
    return &m_transaction.get();
}

bool IDBObjectStore::autoIncrement() const
{
    return m_info.autoIncrement();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext* context, ExceptionCode& ec)
{
    return openCursor(context, static_cast<IDBKeyRange*>(nullptr), ec);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext* context, IDBKeyRange* keyRange, ExceptionCode& ec)
{
    return openCursor(context, keyRange, IDBCursor::directionNext(), ec);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCode& ec)
{
    return openCursor(context, key, IDBCursor::directionNext(), ec);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext* context, IDBKeyRange* range, const String& directionString, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBObjectStore::openCursor");

    if (m_deleted) {
        ec = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec = IDBDatabaseException::TransactionInactiveError;
        return nullptr;
    }

    IndexedDB::CursorDirection direction = IDBCursor::stringToDirection(directionString, ec);
    if (ec)
        return nullptr;

    auto info = IDBCursorInfo::objectStoreCursor(m_transaction.get(), m_info.identifier(), range, direction);
    Ref<IDBRequest> request = m_transaction->requestOpenCursor(*context, *this, info);
    return WTF::move(request);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, const String& direction, ExceptionCode& ec)
{
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, ec);
    if (ec)
        return 0;

    return openCursor(context, keyRange.get(), direction, ec);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::get(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBObjectStore::get");

    if (!context) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::TransactionInactiveError);
        return nullptr;
    }

    if (m_deleted) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    DOMRequestState requestState(context);
    RefPtr<IDBKey> idbKey = scriptValueToIDBKey(&requestState, key);
    if (!idbKey || idbKey->type() == KeyType::Invalid) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
        return nullptr;
    }

    Ref<IDBRequest> request = m_transaction->requestGetRecord(*context, *this, idbKey.get());
    return WTF::move(request);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::get(ScriptExecutionContext* context, IDBKeyRange* keyRange, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBObjectStore::get");

    if (!context) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::TransactionInactiveError);
        return nullptr;
    }

    if (m_deleted) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    IDBKeyRangeData keyRangeData(keyRange);
    if (!keyRangeData.isValid()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
        return nullptr;
    }

    Ref<IDBRequest> request = m_transaction->requestGetRecord(*context, *this, keyRangeData);
    return WTF::move(request);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::add(JSC::ExecState& state, JSC::JSValue value, ExceptionCode& ec)
{
    return putOrAdd(state, value, nullptr, IndexedDB::ObjectStoreOverwriteMode::NoOverwrite, ec);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::add(JSC::ExecState& execState, JSC::JSValue value, JSC::JSValue key, ExceptionCode& ec)
{
    auto idbKey = scriptValueToIDBKey(execState, key);
    return putOrAdd(execState, value, idbKey, IndexedDB::ObjectStoreOverwriteMode::NoOverwrite, ec);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::put(JSC::ExecState& execState, JSC::JSValue value, JSC::JSValue key, ExceptionCode& ec)
{
    auto idbKey = scriptValueToIDBKey(execState, key);
    return putOrAdd(execState, value, idbKey, IndexedDB::ObjectStoreOverwriteMode::Overwrite, ec);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::put(JSC::ExecState& state, JSC::JSValue value, ExceptionCode& ec)
{
    return putOrAdd(state, value, nullptr, IndexedDB::ObjectStoreOverwriteMode::Overwrite, ec);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::putOrAdd(JSC::ExecState& state, JSC::JSValue value, RefPtr<IDBKey> key, IndexedDB::ObjectStoreOverwriteMode overwriteMode, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBObjectStore::putOrAdd");

    if (m_transaction->isReadOnly()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::ReadOnlyError);
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::TransactionInactiveError);
        return nullptr;
    }

    if (m_deleted) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    RefPtr<SerializedScriptValue> serializedValue = SerializedScriptValue::create(&state, value, nullptr, nullptr);
    if (state.hadException()) {
        ec = DATA_CLONE_ERR;
        return nullptr;
    }

    if (serializedValue->hasBlobURLs()) {
        // FIXME: Add Blob/File/FileList support
        ec = DATA_CLONE_ERR;
        return nullptr;
    }

    if (key && key->type() == KeyType::Invalid) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
        return nullptr;
    }

    bool usesInlineKeys = !m_info.keyPath().isNull();
    bool usesKeyGenerator = autoIncrement();
    if (usesInlineKeys) {
        if (key) {
            ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
            return nullptr;
        }

        RefPtr<IDBKey> keyPathKey = maybeCreateIDBKeyFromScriptValueAndKeyPath(state, value, m_info.keyPath());
        if (keyPathKey && !keyPathKey->isValid()) {
            ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
            return nullptr;
        }

        if (!keyPathKey) {
            if (usesKeyGenerator) {
                if (!canInjectIDBKeyIntoScriptValue(state, value, m_info.keyPath())) {
                    ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
                    return nullptr;
                }
            } else {
                ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
                return nullptr;
            }
        }

        if (keyPathKey) {
            ASSERT(!key);
            key = keyPathKey;
        }
    } else if (!usesKeyGenerator && !key) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
        return nullptr;
    }

    auto context = scriptExecutionContextFromExecState(&state);
    if (!context) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::Unknown);
        return nullptr;
    }

    Ref<IDBRequest> request = m_transaction->requestPutOrAdd(*context, *this, key.get(), *serializedValue, overwriteMode);
    return adoptRef(request.leakRef());
}


RefPtr<WebCore::IDBRequest> IDBObjectStore::deleteFunction(ScriptExecutionContext* context, IDBKeyRange* keyRange, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBObjectStore::deleteFunction");

    if (m_transaction->isReadOnly()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::ReadOnlyError);
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::TransactionInactiveError);
        return nullptr;
    }

    if (m_deleted) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    IDBKeyRangeData keyRangeData(keyRange);
    if (!keyRangeData.isValid()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
        return nullptr;
    }

    Ref<IDBRequest> request = m_transaction->requestDeleteRecord(*context, *this, keyRangeData);
    return WTF::move(request);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::deleteFunction(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCode& ec)
{
    return deleteFunction(context, key.jsValue(), ec);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::deleteFunction(ScriptExecutionContext* context, JSC::JSValue key, ExceptionCode& ec)
{
    DOMRequestState requestState(context);
    RefPtr<IDBKey> idbKey = scriptValueToIDBKey(&requestState, key);
    if (!idbKey || idbKey->type() == KeyType::Invalid) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
        return nullptr;
    }

    return deleteFunction(context, &IDBKeyRange::create(idbKey.get()).get(), ec);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::clear(ScriptExecutionContext* context, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBObjectStore::clear");

    if (m_transaction->isReadOnly()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::ReadOnlyError);
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::TransactionInactiveError);
        return nullptr;
    }

    if (m_deleted) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::InvalidStateError);
        return nullptr;
    }

    Ref<IDBRequest> request = m_transaction->requestClearObjectStore(*context, *this);
    return adoptRef(request.leakRef());
}

RefPtr<WebCore::IDBIndex> IDBObjectStore::createIndex(ScriptExecutionContext* context, const String& name, const IDBKeyPath& keyPath, bool unique, bool multiEntry, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBObjectStore::createIndex %s", name.utf8().data());

    if (!context) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    if (m_deleted) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::InvalidStateError);
        return nullptr;
    }

    if (!m_transaction->isVersionChange()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::InvalidStateError);
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::TransactionInactiveError);
        return nullptr;
    }

    if (!keyPath.isValid()) {
        ec = IDBDatabaseException::SyntaxError;
        return nullptr;
    }

    if (name.isNull()) {
        ec = TypeError;
        return nullptr;
    }

    if (m_info.hasIndex(name)) {
        ec = IDBDatabaseException::ConstraintError;
        return nullptr;
    }

    if (keyPath.type() == IndexedDB::KeyPathType::Array && multiEntry) {
        ec = IDBDatabaseException::InvalidAccessError;
        return nullptr;
    }

    // Install the new Index into the ObjectStore's info.
    IDBIndexInfo info = m_info.createNewIndex(name, keyPath, unique, multiEntry);
    m_transaction->database().didCreateIndexInfo(info);

    // Create the actual IDBObjectStore from the transaction, which also schedules the operation server side.
    Ref<IDBIndex> index = m_transaction->createIndex(*this, info);
    m_referencedIndexes.set(name, &index.get());

    return WTF::move(index);
}

RefPtr<WebCore::IDBIndex> IDBObjectStore::index(const String& indexName, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBObjectStore::index");

    if (indexName.isEmpty()) {
        ec = NOT_FOUND_ERR;
        return nullptr;
    }

    if (m_deleted) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::TransactionInactiveError);
        return nullptr;
    }

    auto iterator = m_referencedIndexes.find(indexName);
    if (iterator != m_referencedIndexes.end())
        return iterator->value;

    auto* info = m_info.infoForExistingIndex(indexName);
    if (!info) {
        ec = NOT_FOUND_ERR;
        return nullptr;
    }

    auto index = IDBIndex::create(*info, *this);
    m_referencedIndexes.set(indexName, &index.get());

    return WTF::move(index);
}

void IDBObjectStore::deleteIndex(const String& name, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBObjectStore::deleteIndex %s", name.utf8().data());

    if (m_deleted) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::InvalidStateError);
        return;
    }

    if (!m_transaction->isVersionChange()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::InvalidStateError);
        return;
    }

    if (!m_transaction->isActive()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::TransactionInactiveError);
        return;
    }

    if (!m_info.hasIndex(name)) {
        ec = IDBDatabaseException::NotFoundError;
        return;
    }

    auto* info = m_info.infoForExistingIndex(name);
    ASSERT(info);
    m_transaction->database().didDeleteIndexInfo(*info);

    m_info.deleteIndex(name);

    if (auto index = m_referencedIndexes.take(name))
        index->markAsDeleted();

    m_transaction->deleteIndex(m_info.identifier(), name);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::count(ScriptExecutionContext* context, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBObjectStore::count");

    if (!context) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    IDBKeyRangeData range;
    range.isNull = false;
    return doCount(*context, range, ec);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::count(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBObjectStore::count");

    if (!context) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    DOMRequestState requestState(context);
    RefPtr<IDBKey> idbKey = scriptValueToIDBKey(&requestState, key);
    if (!idbKey || idbKey->type() == KeyType::Invalid) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
        return nullptr;
    }

    return doCount(*context, IDBKeyRangeData(idbKey.get()), ec);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::count(ScriptExecutionContext* context, IDBKeyRange* range, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBObjectStore::count");

    if (!context) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    return doCount(*context, IDBKeyRangeData(range), ec);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::doCount(ScriptExecutionContext& context, const IDBKeyRangeData& range, ExceptionCode& ec)
{
    if (!m_transaction->isActive()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::TransactionInactiveError);
        return nullptr;
    }

    if (m_deleted) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    if (range.isNull) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
        return nullptr;
    }

    return m_transaction->requestCount(context, *this, range);
}

void IDBObjectStore::markAsDeleted()
{
    m_deleted = true;
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
