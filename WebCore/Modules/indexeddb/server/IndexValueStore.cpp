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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IndexValueStore.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBError.h"
#include "IDBKeyRangeData.h"
#include "Logging.h"
#include "MemoryIndex.h"

namespace WebCore {
namespace IDBServer {

IndexValueStore::IndexValueStore(bool unique)
    : m_unique(unique)
{
}

const IDBKeyData* IndexValueStore::lowestValueForKey(const IDBKeyData& key) const
{
    const auto& entry = m_records.get(key);
    if (!entry)
        return nullptr;

    return entry->getLowest();
}

uint64_t IndexValueStore::countForKey(const IDBKeyData& key) const
{
    const auto& entry = m_records.get(key);
    if (!entry)
        return 0;

    return entry->getCount();
}

bool IndexValueStore::contains(const IDBKeyData& key) const
{
    const auto& entry = m_records.get(key);
    if (!entry)
        return false;

    ASSERT(entry->getCount());

    return true;
}

IDBError IndexValueStore::addRecord(const IDBKeyData& indexKey, const IDBKeyData& valueKey)
{
    auto result = m_records.add(indexKey, nullptr);

    if (!result.isNewEntry && m_unique)
        return IDBError(IDBExceptionCode::ConstraintError);

    if (result.isNewEntry)
        result.iterator->value = std::make_unique<IndexValueEntry>(m_unique);

    result.iterator->value->addKey(valueKey);
    m_orderedKeys.insert(indexKey);

    return { };
}

void IndexValueStore::removeRecord(const IDBKeyData& indexKey, const IDBKeyData& valueKey)
{
    auto iterator = m_records.find(indexKey);
    if (!iterator->value)
        return;

    if (iterator->value->removeKey(valueKey))
        m_records.remove(iterator);
}

void IndexValueStore::removeEntriesWithValueKey(MemoryIndex& index, const IDBKeyData& valueKey)
{
    HashSet<IDBKeyData*> entryKeysToRemove;

    for (auto& entry : m_records) {
        if (entry.value->removeKey(valueKey))
            index.notifyCursorsOfValueChange(entry.key, valueKey);
        if (!entry.value->getCount())
            entryKeysToRemove.add(&entry.key);
    }

    for (auto* entry : entryKeysToRemove) {
        m_orderedKeys.erase(*entry);
        m_records.remove(*entry);
    }
}

IDBKeyData IndexValueStore::lowestKeyWithRecordInRange(const IDBKeyRangeData& range) const
{
    LOG(IndexedDB, "IndexValueStore::lowestKeyWithRecordInRange - %s", range.loggingString().utf8().data());

    if (range.isExactlyOneKey())
        return m_records.contains(range.lowerKey) ? range.lowerKey : IDBKeyData();

    auto iterator = lowestIteratorInRange(range);
    if (iterator == m_orderedKeys.end())
        return { };

    return *iterator;
}

std::set<IDBKeyData>::iterator IndexValueStore::lowestIteratorInRange(const IDBKeyRangeData& range) const
{
    auto lowestInRange = m_orderedKeys.lower_bound(range.lowerKey);

    if (lowestInRange == m_orderedKeys.end())
        return lowestInRange;

    if (range.lowerOpen && *lowestInRange == range.lowerKey) {
        ++lowestInRange;

        if (lowestInRange == m_orderedKeys.end())
            return lowestInRange;
    }

    if (!range.upperKey.isNull()) {
        if (lowestInRange->compare(range.upperKey) > 0)
            return m_orderedKeys.end();
        if (range.upperOpen && *lowestInRange == range.upperKey)
            return m_orderedKeys.end();
    }

    return lowestInRange;
}

std::set<IDBKeyData>::reverse_iterator IndexValueStore::highestReverseIteratorInRange(const IDBKeyRangeData& range) const
{
    auto highestInRange = std::set<IDBKeyData>::reverse_iterator(m_orderedKeys.upper_bound(range.upperKey));

    if (highestInRange == m_orderedKeys.rend())
        return highestInRange;

    if (range.upperOpen && *highestInRange == range.upperKey) {
        ++highestInRange;

        if (highestInRange == m_orderedKeys.rend())
            return highestInRange;
    }

    if (!range.lowerKey.isNull()) {
        if (highestInRange->compare(range.lowerKey) < 0)
            return m_orderedKeys.rend();
        if (range.lowerOpen && *highestInRange == range.lowerKey)
            return m_orderedKeys.rend();
    }

    return highestInRange;
}

IndexValueStore::Iterator IndexValueStore::find(const IDBKeyData& key, bool open)
{
    IDBKeyRangeData range;
    if (!key.isNull())
        range.lowerKey = key;
    else
        range.lowerKey = IDBKeyData::minimum();
    range.lowerOpen = open;

    auto iterator = lowestIteratorInRange(range);
    if (iterator == m_orderedKeys.end())
        return { };

    auto record = m_records.get(*iterator);
    ASSERT(record);

    auto primaryIterator = record->begin();
    ASSERT(primaryIterator.isValid());
    return { *this, iterator, primaryIterator };
}

IndexValueStore::Iterator IndexValueStore::find(const IDBKeyData& key, const IDBKeyData& primaryKey)
{
    ASSERT(!key.isNull());
    ASSERT(!primaryKey.isNull());

    IDBKeyRangeData range;
    range.lowerKey = key;
    range.lowerOpen = false;

    auto iterator = lowestIteratorInRange(range);
    if (iterator == m_orderedKeys.end())
        return { };

    auto record = m_records.get(*iterator);
    ASSERT(record);

    auto primaryIterator = record->find(primaryKey);
    if (primaryIterator.isValid())
        return { *this, iterator, primaryIterator };

    // If we didn't find a primary key iterator in this entry,
    // we need to move on to start of the next record.
    iterator++;
    if (iterator == m_orderedKeys.end())
        return { };

    record = m_records.get(*iterator);
    ASSERT(record);

    primaryIterator = record->begin();
    ASSERT(primaryIterator.isValid());

    return { *this, iterator, primaryIterator };
}

IndexValueStore::Iterator IndexValueStore::reverseFind(const IDBKeyData& key, bool open)
{
    IDBKeyRangeData range;
    if (!key.isNull())
        range.upperKey = key;
    else
        range.upperKey = IDBKeyData::maximum();
    range.upperOpen = open;

    auto iterator = highestReverseIteratorInRange(range);
    if (iterator == m_orderedKeys.rend())
        return { };

    auto record = m_records.get(*iterator);
    ASSERT(record);

    auto primaryIterator = record->reverseBegin();
    ASSERT(primaryIterator.isValid());
    return { *this, iterator, primaryIterator };
}

IndexValueStore::Iterator IndexValueStore::reverseFind(const IDBKeyData& key, const IDBKeyData& primaryKey)
{
    ASSERT(!key.isNull());
    ASSERT(!primaryKey.isNull());

    IDBKeyRangeData range;
    range.upperKey = key;
    range.upperOpen = false;

    auto iterator = highestReverseIteratorInRange(range);
    if (iterator == m_orderedKeys.rend())
        return { };

    auto record = m_records.get(*iterator);
    ASSERT(record);

    auto primaryIterator = record->reverseFind(primaryKey);
    if (primaryIterator.isValid())
        return { *this, iterator, primaryIterator };

    // If we didn't find a primary key iterator in this entry,
    // we need to move on to start of the next record.
    iterator++;
    if (iterator == m_orderedKeys.rend())
        return { };

    record = m_records.get(*iterator);
    ASSERT(record);

    primaryIterator = record->reverseBegin();
    ASSERT(primaryIterator.isValid());

    return { *this, iterator, primaryIterator };
}


IndexValueStore::Iterator::Iterator(IndexValueStore& store, std::set<IDBKeyData>::iterator iterator, IndexValueEntry::Iterator primaryIterator)
    : m_store(&store)
    , m_forwardIterator(iterator)
    , m_primaryKeyIterator(primaryIterator)
{
}

IndexValueStore::Iterator::Iterator(IndexValueStore& store, std::set<IDBKeyData>::reverse_iterator iterator, IndexValueEntry::Iterator primaryIterator)
    : m_store(&store)
    , m_forward(false)
    , m_reverseIterator(iterator)
    , m_primaryKeyIterator(primaryIterator)
{
}

IndexValueStore::Iterator& IndexValueStore::Iterator::nextIndexEntry()
{
    if (!m_store)
        return *this;

    if (m_forward) {
        ++m_forwardIterator;
        if (m_forwardIterator == m_store->m_orderedKeys.end()) {
            invalidate();
            return *this;
        }

        auto* entry = m_store->m_records.get(*m_forwardIterator);
        ASSERT(entry);

        m_primaryKeyIterator = entry->begin();
        ASSERT(m_primaryKeyIterator.isValid());
    } else {
        ++m_reverseIterator;
        if (m_reverseIterator == m_store->m_orderedKeys.rend()) {
            invalidate();
            return *this;
        }

        auto* entry = m_store->m_records.get(*m_reverseIterator);
        ASSERT(entry);

        m_primaryKeyIterator = entry->reverseBegin();
        ASSERT(m_primaryKeyIterator.isValid());
    }
    
    return *this;
}

IndexValueStore::Iterator& IndexValueStore::Iterator::operator++()
{
    if (!isValid())
        return *this;

    ++m_primaryKeyIterator;
    if (m_primaryKeyIterator.isValid())
        return *this;

    // Ran out of primary key records, so move the main index iterator.
    return nextIndexEntry();
}

void IndexValueStore::Iterator::invalidate()
{
    m_store = nullptr;
    m_primaryKeyIterator.invalidate();
}

bool IndexValueStore::Iterator::isValid()
{
    return m_store && m_primaryKeyIterator.isValid();
}

const IDBKeyData& IndexValueStore::Iterator::key()
{
    ASSERT(isValid());
    return m_forward ? *m_forwardIterator : *m_reverseIterator;
}

const IDBKeyData& IndexValueStore::Iterator::primaryKey()
{
    ASSERT(isValid());
    return m_primaryKeyIterator.key();
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
