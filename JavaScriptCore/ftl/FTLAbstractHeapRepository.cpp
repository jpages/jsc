/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
#include "FTLAbstractHeapRepository.h"

#if ENABLE(FTL_JIT)

#include "DirectArguments.h"
#include "FTLAbbreviations.h"
#include "GetterSetter.h"
#include "JSArrowFunction.h"
#include "JSEnvironmentRecord.h"
#include "JSPropertyNameEnumerator.h"
#include "JSScope.h"
#include "JSCInlines.h"
#include "ScopedArguments.h"
#include "ScopedArgumentsTable.h"

namespace JSC { namespace FTL {

AbstractHeapRepository::AbstractHeapRepository(LContext context)
    : root(0, "jscRoot")

#define ABSTRACT_HEAP_INITIALIZATION(name) , name(&root, #name)
    FOR_EACH_ABSTRACT_HEAP(ABSTRACT_HEAP_INITIALIZATION)
#undef ABSTRACT_HEAP_INITIALIZATION

#define ABSTRACT_FIELD_INITIALIZATION(name, offset) , name(&root, #name, offset)
    FOR_EACH_ABSTRACT_FIELD(ABSTRACT_FIELD_INITIALIZATION)
#undef ABSTRACT_FIELD_INITIALIZATION
    
    , JSCell_freeListNext(JSCell_structureID)
    
#define INDEXED_ABSTRACT_HEAP_INITIALIZATION(name, offset, size) , name(context, &root, #name, offset, size)
    FOR_EACH_INDEXED_ABSTRACT_HEAP(INDEXED_ABSTRACT_HEAP_INITIALIZATION)
#undef INDEXED_ABSTRACT_HEAP_INITIALIZATION
    
#define NUMBERED_ABSTRACT_HEAP_INITIALIZATION(name) , name(context, &root, #name)
    FOR_EACH_NUMBERED_ABSTRACT_HEAP(NUMBERED_ABSTRACT_HEAP_INITIALIZATION)
#undef NUMBERED_ABSTRACT_HEAP_INITIALIZATION

    , absolute(context, &root, "absolute")
    , m_context(context)
#if !FTL_USES_B3
    , m_tbaaKind(mdKindID(m_context, "tbaa"))
#endif
{
    // Make sure that our explicit assumptions about the StructureIDBlob match reality.
    RELEASE_ASSERT(!(JSCell_indexingType.offset() & (sizeof(int32_t) - 1)));
    RELEASE_ASSERT(JSCell_indexingType.offset() + 1 == JSCell_typeInfoType.offset());
    RELEASE_ASSERT(JSCell_indexingType.offset() + 2 == JSCell_typeInfoFlags.offset());
    RELEASE_ASSERT(JSCell_indexingType.offset() + 3 == JSCell_cellState.offset());

    JSCell_indexingType.changeParent(&JSCell_usefulBytes);
    JSCell_typeInfoType.changeParent(&JSCell_usefulBytes);
    JSCell_typeInfoFlags.changeParent(&JSCell_usefulBytes);
    JSCell_cellState.changeParent(&JSCell_usefulBytes);

#if !FTL_USES_B3
    root.m_tbaaMetadata = mdNode(m_context, mdString(m_context, root.m_heapName));
    
    RELEASE_ASSERT(m_tbaaKind);
    RELEASE_ASSERT(root.m_tbaaMetadata);
#endif
    
    RELEASE_ASSERT(!JSCell_freeListNext.offset());
}

AbstractHeapRepository::~AbstractHeapRepository()
{
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

