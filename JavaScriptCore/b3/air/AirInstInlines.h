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

#ifndef AirInstInlines_h
#define AirInstInlines_h

#if ENABLE(B3_JIT)

#include "AirInst.h"
#include "AirOpcodeUtils.h"
#include "AirSpecial.h"
#include "B3Value.h"

namespace JSC { namespace B3 { namespace Air {

template<typename T> struct ForEach;
template<> struct ForEach<Tmp> {
    template<typename Functor>
    static void forEach(Inst& inst, const Functor& functor)
    {
        inst.forEachTmp(functor);
    }
};
template<> struct ForEach<Arg> {
    template<typename Functor>
    static void forEach(Inst& inst, const Functor& functor)
    {
        inst.forEachArg(functor);
    }
};
template<> struct ForEach<StackSlot*> {
    template<typename Functor>
    static void forEach(Inst& inst, const Functor& functor)
    {
        inst.forEachArg(
            [&] (Arg& arg, Arg::Role role, Arg::Type type) {
                if (!arg.isStack())
                    return;
                StackSlot* stackSlot = arg.stackSlot();

                // FIXME: This is way too optimistic about the meaning of "Def". It gets lucky for
                // now because our only use of "Anonymous" stack slots happens to want the optimistic
                // semantics. We could fix this by just changing the comments that describe the
                // semantics of "Anonymous".
                // https://bugs.webkit.org/show_bug.cgi?id=151128
                
                functor(stackSlot, role, type);
                arg = Arg::stack(stackSlot, arg.offset());
            });
    }
};

template<typename Thing, typename Functor>
void Inst::forEach(const Functor& functor)
{
    ForEach<Thing>::forEach(*this, functor);
}

inline bool Inst::hasSpecial() const
{
    return args.size() && args[0].isSpecial();
}

inline const RegisterSet& Inst::extraClobberedRegs()
{
    ASSERT(hasSpecial());
    return args[0].special()->extraClobberedRegs(*this);
}

template<typename Functor>
inline void Inst::forEachDefAndExtraClobberedTmp(Arg::Type type, const Functor& functor)
{
    forEachTmp([&] (Tmp& tmpArg, Arg::Role role, Arg::Type argType) {
        if (argType == type && Arg::isDef(role))
            functor(tmpArg);
    });

    if (!hasSpecial())
        return;

    const RegisterSet& clobberedRegisters = extraClobberedRegs();
    clobberedRegisters.forEach([functor, type] (Reg reg) {
        if (reg.isGPR() == (type == Arg::GP)) {
            Tmp registerTmp(reg);
            functor(registerTmp);
        }
    });
}

inline void Inst::reportUsedRegisters(const RegisterSet& usedRegisters)
{
    ASSERT(hasSpecial());
    args[0].special()->reportUsedRegisters(*this, usedRegisters);
}

inline bool isShiftValid(const Inst& inst)
{
#if CPU(X86) || CPU(X86_64)
    return inst.args[0] == Tmp(X86Registers::ecx);
#else
    UNUSED_PARAM(inst);
    return true;
#endif
}

inline bool isLshift32Valid(const Inst& inst)
{
    return isShiftValid(inst);
}

inline bool isLshift64Valid(const Inst& inst)
{
    return isShiftValid(inst);
}

inline bool isRshift32Valid(const Inst& inst)
{
    return isShiftValid(inst);
}

inline bool isRshift64Valid(const Inst& inst)
{
    return isShiftValid(inst);
}

inline bool isUrshift32Valid(const Inst& inst)
{
    return isShiftValid(inst);
}

inline bool isUrshift64Valid(const Inst& inst)
{
    return isShiftValid(inst);
}

inline bool isX86DivHelperValid(const Inst& inst)
{
#if CPU(X86) || CPU(X86_64)
    return inst.args[0] == Tmp(X86Registers::eax)
        && inst.args[1] == Tmp(X86Registers::edx);
#else
    return false;
#endif
}

inline bool isX86ConvertToDoubleWord32Valid(const Inst& inst)
{
    return isX86DivHelperValid(inst);
}

inline bool isX86ConvertToQuadWord64Valid(const Inst& inst)
{
    return isX86DivHelperValid(inst);
}

inline bool isX86Div32Valid(const Inst& inst)
{
    return isX86DivHelperValid(inst);
}

inline bool isX86Div64Valid(const Inst& inst)
{
    return isX86DivHelperValid(inst);
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

#endif // AirInstInlines_h

