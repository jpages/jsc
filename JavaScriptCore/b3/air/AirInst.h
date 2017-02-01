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

#ifndef AirInst_h
#define AirInst_h

#if ENABLE(B3_JIT)

#include "AirArg.h"
#include "AirOpcode.h"
#include "CCallHelpers.h"

namespace JSC {

class CCallHelpers;
class RegisterSet;

namespace B3 {

class Value;

namespace Air {

struct GenerationContext;

struct Inst {
public:
    typedef Vector<Arg, 3> ArgList;

    Inst()
        : origin(nullptr)
        , opcode(Nop)
    {
    }
    
    Inst(Opcode opcode, Value* origin)
        : origin(origin)
        , opcode(opcode)
    {
    }
    
    template<typename... Arguments>
    Inst(Opcode opcode, Value* origin, Arg arg, Arguments... arguments)
        : args{ arg, arguments... }
        , origin(origin)
        , opcode(opcode)
    {
    }

    Inst(Opcode opcode, Value* origin, const ArgList& arguments)
        : args(arguments)
        , origin(origin)
        , opcode(opcode)
    {
    }

    Inst(Opcode opcode, Value* origin, ArgList&& arguments)
        : args(WTF::move(arguments))
        , origin(origin)
        , opcode(opcode)
    {
    }

    explicit operator bool() const { return origin || opcode != Nop || args.size(); }

    // Note that these functors all avoid using "const" because we want to use them for things that
    // edit IR. IR is meant to be edited; if you're carrying around a "const Inst&" then you're
    // probably doing it wrong.

    // This only walks those Tmps that are explicitly mentioned, and it doesn't tell you their role
    // or type.
    template<typename Functor>
    void forEachTmpFast(const Functor& functor)
    {
        for (Arg& arg : args)
            arg.forEachTmpFast(functor);
    }

    typedef void EachArgCallback(Arg&, Arg::Role, Arg::Type);
    
    // Calls the functor with (arg, role, type). This function is auto-generated by
    // opcode_generator.rb.
    template<typename Functor>
    void forEachArg(const Functor&);

    // Calls the functor with (tmp, role, type).
    template<typename Functor>
    void forEachTmp(const Functor& functor)
    {
        forEachArg(
            [&] (Arg& arg, Arg::Role role, Arg::Type type) {
                arg.forEachTmp(role, type, functor);
            });
    }

    // Thing can be either Arg or Tmp.
    template<typename Thing, typename Functor>
    void forEach(const Functor&);

    // Returns true if this instruction has a Special*. This unlocks some extra functionality.
    bool hasSpecial() const;

    // Reports any additional registers clobbered by this operation. Note that for efficiency,
    // extraClobberedRegs() only works if hasSpecial() returns true.
    const RegisterSet& extraClobberedRegs();

    // Iterate over the Defs and the extra clobbered registers.
    template<typename Functor>
    void forEachDefAndExtraClobberedTmp(Arg::Type, const Functor& functor);

    // Use this to report which registers are live. This should be done just before codegen. Note
    // that for efficiency, reportUsedRegisters() only works if hasSpecial() returns true.
    void reportUsedRegisters(const RegisterSet&);

    // Is this instruction in one of the valid forms right now? This function is auto-generated by
    // opcode_generator.rb.
    bool isValidForm();

    // Assuming this instruction is in a valid form right now, will it still be in one of the valid
    // forms if we put an Addr referencing the stack (or a StackSlot or CallArg, of course) in the
    // given index? Spilling uses this: it walks the args by index to find Tmps that need spilling;
    // if it finds one, it calls this to see if it can replace the Arg::Tmp with an Arg::Addr. If it
    // finds a non-Tmp Arg, then it calls that Arg's forEachTmp to do a replacement that way.
    //
    // This function is auto-generated by opcode_generator.rb.
    bool admitsStack(unsigned argIndex);

    // Returns true if this instruction can have any effects other than control flow or arguments.
    bool hasNonArgNonControlEffects();

    // Returns true if this instruction can have any effects other than what is implied by arguments.
    // For example, "Move $42, (%rax)" will return false because the effect of storing to (%rax) is
    // implied by the second argument.
    bool hasNonArgEffects();

    // Tells you if this operation has arg effects.
    bool hasArgEffects();

    // Generate some code for this instruction. This is, like, literally our backend. If this is the
    // terminal, it returns the jump that needs to be linked for the "then" case, with the "else"
    // case being fall-through. This function is auto-generated by opcode_generator.rb.
    CCallHelpers::Jump generate(CCallHelpers&, GenerationContext&);

    void dump(PrintStream&) const;

    ArgList args;
    Value* origin; // The B3::Value that this originated from.
    Opcode opcode;
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

#endif // AirInst_h
