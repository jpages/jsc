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

#ifndef AirArg_h
#define AirArg_h

#if ENABLE(B3_JIT)

#include "AirTmp.h"
#include "B3Common.h"
#include "B3Type.h"

namespace JSC { namespace B3 { namespace Air {

class Special;
class StackSlot;

// This class name is also intentionally terse because we will say it a lot. You'll see code like
// Inst(..., Arg::imm(5), Arg::addr(thing, blah), ...)
class Arg {
public:
    // These enum members are intentionally terse because we have to mention them a lot.
    enum Kind : int8_t {
        Invalid,

        // This is either an unassigned temporary or a register. All unassigned temporaries
        // eventually become registers.
        Tmp,

        // This is an immediate that the instruction will materialize.
        Imm,
        Imm64,

        // These are the addresses. Instructions may load from (Use), store to (Def), or evaluate
        // (UseAddr) addresses.
        Addr,
        Stack,
        CallArg,
        Index,

        // Immediate operands that customize the behavior of an operation. You can think of them as
        // secondary opcodes. They are always "Use"'d.
        RelCond,
        ResCond,
        DoubleCond,
        Special
    };

    enum Role : int8_t {
        // Use means that the Inst will read from this value before doing anything else.
        //
        // For Tmp: The Inst will read this Tmp.
        // For Arg::addr and friends: The Inst will load from this address.
        // For Arg::imm and friends: The Inst will materialize and use this immediate.
        // For RelCond/ResCond/Special: This is the only valid role for these kinds.
        //
        // Note that Use of an address does not mean escape. It only means that the instruction will
        // load from the address before doing anything else. This is a bit tricky; for example
        // Specials could theoretically squirrel away the address and effectively escape it. However,
        // this is not legal. On the other hand, any address other than Stack is presumed to be
        // always escaping, and Stack is presumed to be always escaping if it's Locked.
        Use,

        // LateUse means that the Inst will read from this value after doing its Def's. Note that LateUse
        // on an Addr or Index still means Use on the internal temporaries.
        LateUse,

        // Def means that the Inst will write to this value after doing everything else.
        //
        // For Tmp: The Inst will write to this Tmp.
        // For Arg::addr and friends: The Inst will store to this address.
        // This isn't valid for any other kinds.
        //
        // Like Use of address, Def of address does not mean escape.
        Def,

        // This is a combined Use and Def. It means that both things happen.
        UseDef,

        // This is a special kind of use that is only valid for addresses. It means that the
        // instruction will evaluate the address expression and consume the effective address, but it
        // will neither load nor store. This is an escaping use, because now the address may be
        // passed along to who-knows-where. Note that this isn't really a Use of the Arg, but it does
        // imply that we're Use'ing any registers that the Arg contains.
        UseAddr
    };

    enum Type : int8_t {
        GP,
        FP
    };

    static const unsigned numTypes = 2;

    enum Width : int8_t {
        Width8,
        Width16,
        Width32,
        Width64
    };

    enum Signedness : int8_t {
        Signed,
        Unsigned
    };

    // Returns true if the Role implies that the Inst will Use the Arg. It's deliberately false for
    // UseAddr, since isAnyUse() for an Arg::addr means that we are loading from the address.
    static bool isAnyUse(Role role)
    {
        switch (role) {
        case Use:
        case UseDef:
        case LateUse:
            return true;
        case Def:
        case UseAddr:
            return false;
        }
    }

    // Returns true if the Role implies that the Inst will Use the Arg before doing anything else.
    static bool isEarlyUse(Role role)
    {
        switch (role) {
        case Use:
        case UseDef:
            return true;
        case Def:
        case UseAddr:
        case LateUse:
            return false;
        }
    }

    // Returns true if the Role implies that the Inst will Use the Arg after doing everything else.
    static bool isLateUse(Role role)
    {
        return role == LateUse;
    }

    // Returns true if the Role implies that the Inst will Def the Arg.
    static bool isDef(Role role)
    {
        switch (role) {
        case Use:
        case UseAddr:
        case LateUse:
            return false;
        case Def:
        case UseDef:
            return true;
        }
    }

    static Type typeForB3Type(B3::Type type)
    {
        switch (type) {
        case Void:
            ASSERT_NOT_REACHED();
            return GP;
        case Int32:
        case Int64:
            return GP;
        case Double:
            return FP;
        }
        ASSERT_NOT_REACHED();
        return GP;
    }

    static Width widthForB3Type(B3::Type type)
    {
        switch (type) {
        case Void:
            ASSERT_NOT_REACHED();
            return Width8;
        case Int32:
            return Width32;
        case Int64:
        case Double:
            return Width64;
        }
    }

    Arg()
        : m_kind(Invalid)
    {
    }

    Arg(Air::Tmp tmp)
        : m_kind(Tmp)
        , m_base(tmp)
    {
    }

    static Arg imm(int32_t value)
    {
        Arg result;
        result.m_kind = Imm;
        result.m_offset = value;
        return result;
    }

    static Arg imm64(intptr_t value)
    {
        Arg result;
        result.m_kind = Imm64;
        result.m_offset = value;
        return result;
    }

    static Arg addr(Air::Tmp base, int32_t offset = 0)
    {
        ASSERT(base.isGP());
        Arg result;
        result.m_kind = Addr;
        result.m_base = base;
        result.m_offset = offset;
        return result;
    }

    static Arg stack(StackSlot* value, int32_t offset = 0)
    {
        Arg result;
        result.m_kind = Stack;
        result.m_offset = bitwise_cast<intptr_t>(value);
        result.m_scale = offset; // I know, yuck.
        return result;
    }

    static Arg callArg(int32_t offset)
    {
        Arg result;
        result.m_kind = CallArg;
        result.m_offset = offset;
        return result;
    }

    static bool isValidScale(unsigned scale)
    {
        switch (scale) {
        case 1:
        case 2:
        case 4:
        case 8:
            return true;
        default:
            return false;
        }
    }

    static unsigned logScale(unsigned scale)
    {
        switch (scale) {
        case 1:
            return 0;
        case 2:
            return 1;
        case 4:
            return 2;
        case 8:
            return 3;
        default:
            ASSERT_NOT_REACHED();
            return 0;
        }
    }

    static Arg index(Air::Tmp base, Air::Tmp index, unsigned scale = 1, int32_t offset = 0)
    {
        ASSERT(base.isGP());
        ASSERT(index.isGP());
        ASSERT(isValidScale(scale));
        Arg result;
        result.m_kind = Index;
        result.m_base = base;
        result.m_index = index;
        result.m_scale = static_cast<int32_t>(scale);
        result.m_offset = offset;
        return result;
    }

    static Arg relCond(MacroAssembler::RelationalCondition condition)
    {
        Arg result;
        result.m_kind = RelCond;
        result.m_offset = condition;
        return result;
    }

    static Arg resCond(MacroAssembler::ResultCondition condition)
    {
        Arg result;
        result.m_kind = ResCond;
        result.m_offset = condition;
        return result;
    }

    static Arg doubleCond(MacroAssembler::DoubleCondition condition)
    {
        Arg result;
        result.m_kind = DoubleCond;
        result.m_offset = condition;
        return result;
    }

    static Arg special(Air::Special* special)
    {
        Arg result;
        result.m_kind = Special;
        result.m_offset = bitwise_cast<intptr_t>(special);
        return result;
    }

    bool operator==(const Arg& other) const
    {
        return m_offset == other.m_offset
            && m_kind == other.m_kind
            && m_base == other.m_base
            && m_index == other.m_index
            && m_scale == other.m_scale;
    }

    bool operator!=(const Arg& other) const
    {
        return !(*this == other);
    }

    explicit operator bool() const { return *this != Arg(); }

    Kind kind() const
    {
        return m_kind;
    }

    bool isTmp() const
    {
        return kind() == Tmp;
    }

    bool isImm() const
    {
        return kind() == Imm;
    }

    bool isImm64() const
    {
        return kind() == Imm64;
    }

    bool isAddr() const
    {
        return kind() == Addr;
    }

    bool isStack() const
    {
        return kind() == Stack;
    }

    bool isCallArg() const
    {
        return kind() == CallArg;
    }

    bool isIndex() const
    {
        return kind() == Index;
    }

    bool isRelCond() const
    {
        return kind() == RelCond;
    }

    bool isResCond() const
    {
        return kind() == ResCond;
    }

    bool isDoubleCond() const
    {
        return kind() == DoubleCond;
    }

    bool isCondition() const
    {
        switch (kind()) {
        case RelCond:
        case ResCond:
        case DoubleCond:
            return true;
        default:
            return false;
        }
    }

    bool isSpecial() const
    {
        return kind() == Special;
    }

    bool isAlive() const
    {
        return isTmp() || isStack();
    }

    Air::Tmp tmp() const
    {
        ASSERT(kind() == Tmp);
        return m_base;
    }

    int64_t value() const
    {
        ASSERT(kind() == Imm || kind() == Imm64);
        return m_offset;
    }

    template<typename T>
    bool isRepresentableAs() const
    {
        return B3::isRepresentableAs<T>(value());
    }

    bool isRepresentableAs(Width, Signedness) const;

    template<typename T>
    T asNumber() const
    {
        return static_cast<T>(value());
    }

    void* pointerValue() const
    {
        ASSERT(kind() == Imm64);
        return bitwise_cast<void*>(static_cast<intptr_t>(m_offset));
    }

    Air::Tmp base() const
    {
        ASSERT(kind() == Addr || kind() == Index);
        return m_base;
    }

    bool hasOffset() const
    {
        switch (kind()) {
        case Addr:
        case Stack:
        case CallArg:
        case Index:
            return true;
        default:
            return false;
        }
    }
    
    int32_t offset() const
    {
        if (kind() == Stack)
            return static_cast<int32_t>(m_scale);
        ASSERT(kind() == Addr || kind() == CallArg || kind() == Index);
        return static_cast<int32_t>(m_offset);
    }

    StackSlot* stackSlot() const
    {
        ASSERT(kind() == Stack);
        return bitwise_cast<StackSlot*>(m_offset);
    }

    Air::Tmp index() const
    {
        ASSERT(kind() == Index);
        return m_index;
    }

    unsigned scale() const
    {
        ASSERT(kind() == Index);
        return m_scale;
    }

    unsigned logScale() const
    {
        return logScale(scale());
    }

    Air::Special* special() const
    {
        ASSERT(kind() == Special);
        return bitwise_cast<Air::Special*>(m_offset);
    }

    bool isGPTmp() const
    {
        return isTmp() && tmp().isGP();
    }

    bool isFPTmp() const
    {
        return isTmp() && tmp().isFP();
    }
    
    // Tells us if this Arg can be used in a position that requires a GP value.
    bool isGP() const
    {
        switch (kind()) {
        case Imm:
        case Imm64:
        case Addr:
        case Index:
        case Stack:
        case CallArg:
        case RelCond:
        case ResCond:
        case DoubleCond:
        case Special:
            return true;
        case Tmp:
            return isGPTmp();
        case Invalid:
            return false;
        }
    }

    // Tells us if this Arg can be used in a position that requires a FP value.
    bool isFP() const
    {
        switch (kind()) {
        case Imm:
        case RelCond:
        case ResCond:
        case DoubleCond:
        case Special:
        case Invalid:
            return false;
        case Addr:
        case Index:
        case Stack:
        case CallArg:
        case Imm64: // Yes, we allow Imm64 as a double immediate. We use this for implementing stackmaps.
            return true;
        case Tmp:
            return isFPTmp();
        }
    }

    bool hasType() const
    {
        switch (kind()) {
        case Imm:
        case Special:
        case Tmp:
            return true;
        default:
            return false;
        }
    }
    
    // The type is ambiguous for some arg kinds. Call with care.
    Type type() const
    {
        return isGP() ? GP : FP;
    }

    bool isType(Type type) const
    {
        switch (type) {
        case GP:
            return isGP();
        case FP:
            return isFP();
        }
    }

    bool isGPR() const
    {
        return isTmp() && tmp().isGPR();
    }

    GPRReg gpr() const
    {
        return tmp().gpr();
    }

    bool isFPR() const
    {
        return isTmp() && tmp().isFPR();
    }

    FPRReg fpr() const
    {
        return tmp().fpr();
    }
    
    bool isReg() const
    {
        return isTmp() && tmp().isReg();
    }

    Reg reg() const
    {
        return tmp().reg();
    }

    unsigned gpTmpIndex() const
    {
        return tmp().gpTmpIndex();
    }

    unsigned fpTmpIndex() const
    {
        return tmp().fpTmpIndex();
    }

    unsigned tmpIndex() const
    {
        return tmp().tmpIndex();
    }

    Arg withOffset(int32_t additionalOffset) const
    {
        if (!hasOffset())
            return Arg();
        if (sumOverflows<int32_t>(offset(), additionalOffset))
            return Arg();
        switch (kind()) {
        case Addr:
            return addr(base(), offset() + additionalOffset);
        case Stack:
            return stack(stackSlot(), offset() + additionalOffset);
        case CallArg:
            return callArg(offset() + additionalOffset);
        case Index:
            return index(base(), index(), scale(), offset() + additionalOffset);
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return Arg();
        }
    }

    template<typename Functor>
    void forEachTmpFast(const Functor& functor)
    {
        switch (m_kind) {
        case Tmp:
        case Addr:
            functor(m_base);
            break;
        case Index:
            functor(m_base);
            functor(m_index);
            break;
        default:
            break;
        }
    }

    // This is smart enough to know that an address arg in a Def or UseDef rule will use its
    // tmps and never def them. For example, this:
    //
    // mov %rax, (%rcx)
    //
    // This defs (%rcx) but uses %rcx.
    template<typename Functor>
    void forEachTmp(Role argRole, Type argType, const Functor& functor)
    {
        switch (m_kind) {
        case Tmp:
            ASSERT(isAnyUse(argRole) || isDef(argRole));
            functor(m_base, argRole, argType);
            break;
        case Addr:
            functor(m_base, Use, GP);
            break;
        case Index:
            functor(m_base, Use, GP);
            functor(m_index, Use, GP);
            break;
        default:
            break;
        }
    }

    MacroAssembler::TrustedImm32 asTrustedImm32() const
    {
        ASSERT(isImm());
        return MacroAssembler::TrustedImm32(static_cast<int32_t>(m_offset));
    }

#if USE(JSVALUE64)
    MacroAssembler::TrustedImm64 asTrustedImm64() const
    {
        ASSERT(isImm64());
        return MacroAssembler::TrustedImm64(value());
    }
#endif

    MacroAssembler::TrustedImmPtr asTrustedImmPtr() const
    {
        if (is64Bit())
            ASSERT(isImm64());
        else
            ASSERT(isImm());
        return MacroAssembler::TrustedImmPtr(pointerValue());
    }

    MacroAssembler::Address asAddress() const
    {
        ASSERT(isAddr());
        return MacroAssembler::Address(m_base.gpr(), static_cast<int32_t>(m_offset));
    }

    MacroAssembler::BaseIndex asBaseIndex() const
    {
        ASSERT(isIndex());
        return MacroAssembler::BaseIndex(
            m_base.gpr(), m_index.gpr(), static_cast<MacroAssembler::Scale>(logScale()),
            static_cast<int32_t>(m_offset));
    }

    MacroAssembler::RelationalCondition asRelationalCondition() const
    {
        ASSERT(isRelCond());
        return static_cast<MacroAssembler::RelationalCondition>(m_offset);
    }

    MacroAssembler::ResultCondition asResultCondition() const
    {
        ASSERT(isResCond());
        return static_cast<MacroAssembler::ResultCondition>(m_offset);
    }

    MacroAssembler::DoubleCondition asDoubleCondition() const
    {
        ASSERT(isDoubleCond());
        return static_cast<MacroAssembler::DoubleCondition>(m_offset);
    }

    // Tells you if the Arg is invertible. Only condition arguments are invertible, and even for those, there
    // are a few exceptions - notably Overflow and Signed.
    bool isInvertible() const
    {
        switch (kind()) {
        case RelCond:
        case DoubleCond:
            return true;
        case ResCond:
            return MacroAssembler::isInvertible(asResultCondition());
        default:
            return false;
        }
    }

    // This is valid for condition arguments. It will invert them.
    Arg inverted(bool inverted = true) const
    {
        if (!inverted)
            return *this;
        switch (kind()) {
        case RelCond:
            return relCond(MacroAssembler::invert(asRelationalCondition()));
        case ResCond:
            return resCond(MacroAssembler::invert(asResultCondition()));
        case DoubleCond:
            return doubleCond(MacroAssembler::invert(asDoubleCondition()));
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return Arg();
        }
    }

    Arg flipped(bool flipped = true) const
    {
        if (!flipped)
            return Arg();
        return relCond(MacroAssembler::flip(asRelationalCondition()));
    }

    bool isSignedCond() const
    {
        return isRelCond() && MacroAssembler::isSigned(asRelationalCondition());
    }

    bool isUnsignedCond() const
    {
        return isRelCond() && MacroAssembler::isUnsigned(asRelationalCondition());
    }

    void dump(PrintStream&) const;

    Arg(WTF::HashTableDeletedValueType)
        : m_base(WTF::HashTableDeletedValue)
    {
    }

    bool isHashTableDeletedValue() const
    {
        return *this == Arg(WTF::HashTableDeletedValue);
    }

    unsigned hash() const
    {
        // This really doesn't have to be that great.
        return WTF::IntHash<int64_t>::hash(m_offset) + m_kind + m_scale + m_base.hash() +
            m_index.hash();
    }

private:
    int64_t m_offset { 0 };
    Kind m_kind { Invalid };
    int32_t m_scale { 1 };
    Air::Tmp m_base;
    Air::Tmp m_index;
};

struct ArgHash {
    static unsigned hash(const Arg& key) { return key.hash(); }
    static bool equal(const Arg& a, const Arg& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

} } } // namespace JSC::B3::Air

namespace WTF {

void printInternal(PrintStream&, JSC::B3::Air::Arg::Kind);
void printInternal(PrintStream&, JSC::B3::Air::Arg::Role);
void printInternal(PrintStream&, JSC::B3::Air::Arg::Type);
void printInternal(PrintStream&, JSC::B3::Air::Arg::Width);
void printInternal(PrintStream&, JSC::B3::Air::Arg::Signedness);

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::B3::Air::Arg> {
    typedef JSC::B3::Air::ArgHash Hash;
};

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::B3::Air::Arg> : SimpleClassHashTraits<JSC::B3::Air::Arg> {
    // Because m_scale is 1 in the empty value.
    static const bool emptyValueIsZero = false;
};

} // namespace WTF

#endif // ENABLE(B3_JIT)

#endif // AirArg_h

