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
#include "AirGenerate.h"

#if ENABLE(B3_JIT)

#include "AirAllocateStack.h"
#include "AirCode.h"
#include "AirEliminateDeadCode.h"
#include "AirGenerationContext.h"
#include "AirHandleCalleeSaves.h"
#include "AirIteratedRegisterCoalescing.h"
#include "AirOptimizeBlockOrder.h"
#include "AirReportUsedRegisters.h"
#include "AirSimplifyCFG.h"
#include "AirSpillEverything.h"
#include "AirValidate.h"
#include "B3Common.h"
#include "B3IndexMap.h"
#include "B3TimingScope.h"
#include "CCallHelpers.h"

namespace JSC { namespace B3 { namespace Air {

void prepareForGeneration(Code& code)
{
    TimingScope timingScope("Air::prepareForGeneration");
    
    // We don't expect the incoming code to have predecessors computed.
    code.resetReachability();
    
    if (shouldValidateIR())
        validate(code);

    // If we're doing super verbose dumping, the phase scope of any phase will already do a dump.
    if (shouldDumpIR() && !shouldDumpIRAtEachPhase()) {
        dataLog("Initial air:\n");
        dataLog(code);
    }

    // This is where we run our optimizations and transformations.
    // FIXME: Add Air optimizations.
    // https://bugs.webkit.org/show_bug.cgi?id=150456
    
    eliminateDeadCode(code);

    // Register allocation for all the Tmps that do not have a corresponding machine register.
    // After this phase, every Tmp has a reg.
    //
    // For debugging, you can use spillEverything() to put everything to the stack between each Inst.
    if (false)
        spillEverything(code);
    else
        iteratedRegisterCoalescing(code);

    // Prior to this point the prologue and epilogue is implicit. This makes it explicit. It also
    // does things like identify which callee-saves we're using and saves them.
    handleCalleeSaves(code);

    // This turns all Stack and CallArg Args into Addr args that use the frame pointer. It does
    // this by first-fit allocating stack slots. It should be pretty darn close to optimal, so we
    // shouldn't have to worry about this very much.
    allocateStack(code);

    // If we coalesced moves then we can unbreak critical edges. This is the main reason for this
    // phase.
    simplifyCFG(code);

    // This sorts the basic blocks in Code to achieve an ordering that maximizes the likelihood that a high
    // frequency successor is also the fall-through target.
    optimizeBlockOrder(code);

    // This is needed to satisfy a requirement of B3::StackmapValue.
    reportUsedRegisters(code);

    if (shouldValidateIR())
        validate(code);

    // Do a final dump of Air. Note that we have to do this even if we are doing per-phase dumping,
    // since the final generation is not a phase.
    if (shouldDumpIR()) {
        dataLog("Air after ", code.lastPhaseName(), ", before generation:\n");
        dataLog(code);
    }
}

void generate(Code& code, CCallHelpers& jit)
{
    TimingScope timingScope("Air::generate");

    // And now, we generate code.
    jit.emitFunctionPrologue();
    jit.addPtr(CCallHelpers::TrustedImm32(-code.frameSize()), MacroAssembler::stackPointerRegister);

    GenerationContext context;
    context.code = &code;
    IndexMap<BasicBlock, CCallHelpers::Label> blockLabels(code.size());
    IndexMap<BasicBlock, CCallHelpers::JumpList> blockJumps(code.size());

    auto link = [&] (CCallHelpers::Jump jump, BasicBlock* target) {
        if (blockLabels[target].isSet()) {
            jump.linkTo(blockLabels[target], &jit);
            return;
        }

        blockJumps[target].append(jump);
    };

    for (BasicBlock* block : code) {
        blockJumps[block].link(&jit);
        blockLabels[block] = jit.label();
        ASSERT(block->size() >= 1);
        for (unsigned i = 0; i < block->size() - 1; ++i) {
            CCallHelpers::Jump jump = block->at(i).generate(jit, context);
            ASSERT_UNUSED(jump, !jump.isSet());
        }

        if (block->last().opcode == Jump
            && block->successorBlock(0) == code.findNextBlock(block))
            continue;

        if (block->last().opcode == Ret) {
            // We currently don't represent the full prologue/epilogue in Air, so we need to
            // have this override.
            jit.emitFunctionEpilogue();
            jit.ret();
            continue;
        }
        
        CCallHelpers::Jump jump = block->last().generate(jit, context);
        switch (block->numSuccessors()) {
        case 0:
            ASSERT(!jump.isSet());
            break;
        case 1:
            link(jump, block->successorBlock(0));
            break;
        case 2:
            link(jump, block->successorBlock(0));
            if (block->successorBlock(1) != code.findNextBlock(block))
                link(jit.jump(), block->successorBlock(1));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    for (auto& latePath : context.latePaths)
        latePath->run(jit, context);
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
