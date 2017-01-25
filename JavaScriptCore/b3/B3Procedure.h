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

#ifndef B3Procedure_h
#define B3Procedure_h

#if ENABLE(B3_JIT)

#include "B3Origin.h"
#include "B3Type.h"
#include "PureNaN.h"
#include "RegisterAtOffsetList.h"
#include <wtf/Bag.h>
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/PrintStream.h>
#include <wtf/TriState.h>
#include <wtf/Vector.h>

namespace JSC { namespace B3 {

class BasicBlock;
class BlockInsertionSet;
class OpaqueByproducts;
class Value;

namespace Air { class Code; }

class Procedure {
    WTF_MAKE_NONCOPYABLE(Procedure);
    WTF_MAKE_FAST_ALLOCATED;
public:

    JS_EXPORT_PRIVATE Procedure();
    JS_EXPORT_PRIVATE ~Procedure();

    JS_EXPORT_PRIVATE BasicBlock* addBlock(double frequency = 1);
    
    template<typename ValueType, typename... Arguments>
    ValueType* add(Arguments...);

    Value* addIntConstant(Origin, Type, int64_t value);
    Value* addIntConstant(Value*, int64_t value);

    // Returns null for MixedTriState.
    Value* addBoolConstant(Origin, TriState);

    void resetValueOwners();
    void resetReachability();

    JS_EXPORT_PRIVATE void dump(PrintStream&) const;

    unsigned size() const { return m_blocks.size(); }
    BasicBlock* at(unsigned index) const { return m_blocks[index].get(); }
    BasicBlock* operator[](unsigned index) const { return at(index); }

    class iterator {
    public:
        iterator()
            : m_procedure(nullptr)
            , m_index(0)
        {
        }

        iterator(const Procedure& procedure, unsigned index)
            : m_procedure(&procedure)
            , m_index(findNext(index))
        {
        }

        BasicBlock* operator*()
        {
            return m_procedure->at(m_index);
        }

        iterator& operator++()
        {
            m_index = findNext(m_index + 1);
            return *this;
        }

        bool operator==(const iterator& other) const
        {
            ASSERT(m_procedure == other.m_procedure);
            return m_index == other.m_index;
        }
        
        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }

    private:
        unsigned findNext(unsigned index)
        {
            while (index < m_procedure->size() && !m_procedure->at(index))
                index++;
            return index;
        }

        const Procedure* m_procedure;
        unsigned m_index;
    };

    iterator begin() const { return iterator(*this, 0); }
    iterator end() const { return iterator(*this, size()); }

    Vector<BasicBlock*> blocksInPreOrder();
    Vector<BasicBlock*> blocksInPostOrder();

    class ValuesCollection {
    public:
        ValuesCollection(const Procedure& procedure)
            : m_procedure(procedure)
        {
        }

        class iterator {
        public:
            iterator()
                : m_procedure(nullptr)
                , m_index(0)
            {
            }

            iterator(const Procedure& procedure, unsigned index)
                : m_procedure(&procedure)
                , m_index(findNext(index))
            {
            }

            Value* operator*() const
            {
                return m_procedure->m_values[m_index].get();
            }

            iterator& operator++()
            {
                m_index = findNext(m_index + 1);
                return *this;
            }

            bool operator==(const iterator& other) const
            {
                ASSERT(m_procedure == other.m_procedure);
                return m_index == other.m_index;
            }

            bool operator!=(const iterator& other) const
            {
                return !(*this == other);
            }

        private:
            unsigned findNext(unsigned index)
            {
                while (index < m_procedure->m_values.size() && !m_procedure->m_values[index])
                    index++;
                return index;
            }

            const Procedure* m_procedure;
            unsigned m_index;
        };

        iterator begin() const { return iterator(m_procedure, 0); }
        iterator end() const { return iterator(m_procedure, m_procedure.m_values.size()); }

        unsigned size() const { return m_procedure.m_values.size(); }
        Value* at(unsigned index) const { return m_procedure.m_values[index].get(); }
        Value* operator[](unsigned index) const { return at(index); }
        
    private:
        const Procedure& m_procedure;
    };

    ValuesCollection values() const { return ValuesCollection(*this); }

    void deleteValue(Value*);

    // The name has to be a string literal, since we don't do any memory management for the string.
    void setLastPhaseName(const char* name)
    {
        m_lastPhaseName = name;
    }

    const char* lastPhaseName() const { return m_lastPhaseName; }

    void* addDataSection(size_t size);

    OpaqueByproducts& byproducts() { return *m_byproducts; }

    // Below are methods that make sense to call after you have generated code for the procedure.

    // You have to call this method after calling generate(). The code generated by B3::generate()
    // will require you to keep this object alive for as long as that code is runnable. Usually, this
    // just keeps alive things like the double constant pool and switch lookup tables. If this sounds
    // confusing, you should probably be using the B3::Compilation API to compile code. If you use
    // that API, then you don't have to worry about this.
    std::unique_ptr<OpaqueByproducts> takeByproducts() { return WTF::move(m_byproducts); }

    Air::Code& code() { return *m_code; }

    const RegisterAtOffsetList& calleeSaveRegisters();

private:
    friend class BlockInsertionSet;
    
    JS_EXPORT_PRIVATE size_t addValueIndex();
    
    Vector<std::unique_ptr<BasicBlock>> m_blocks;
    Vector<std::unique_ptr<Value>> m_values;
    Vector<size_t> m_valueIndexFreeList;
    const char* m_lastPhaseName;
    std::unique_ptr<OpaqueByproducts> m_byproducts;
    std::unique_ptr<Air::Code> m_code;
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

#endif // B3Procedure_h

