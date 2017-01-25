/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "FTLOSRExit.h"

#if ENABLE(FTL_JIT)

#include "CodeBlock.h"
#include "DFGBasicBlock.h"
#include "DFGNode.h"
#include "FTLExitArgument.h"
#include "FTLJITCode.h"
#include "FTLLocation.h"
#include "JSCInlines.h"

namespace JSC { namespace FTL {

using namespace DFG;

OSRExitDescriptor::OSRExitDescriptor(
    ExitKind exitKind, ExceptionType exceptionType, DataFormat profileDataFormat,
    MethodOfGettingAValueProfile valueProfile, CodeOrigin codeOrigin,
    CodeOrigin originForProfile, unsigned numberOfArguments,
    unsigned numberOfLocals)
    : m_kind(exitKind)
    , m_exceptionType(exceptionType)
    , m_codeOrigin(codeOrigin)
    , m_codeOriginForExitProfile(originForProfile)
    , m_profileDataFormat(profileDataFormat)
    , m_valueProfile(valueProfile)
    , m_values(numberOfArguments, numberOfLocals)
    , m_isInvalidationPoint(false)
{
}

bool OSRExitDescriptor::willArriveAtExitFromIndirectExceptionCheck() const
{
    switch (m_exceptionType) {
    case ExceptionType::JSCall:
    case ExceptionType::GetById:
    case ExceptionType::PutById:
    case ExceptionType::LazySlowPath:
    case ExceptionType::SubGenerator:
        return true;
    default:
        return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool OSRExitDescriptor::mightArriveAtOSRExitFromGenericUnwind() const
{
    switch (m_exceptionType) {
    case ExceptionType::JSCall:
    case ExceptionType::GetById:
    case ExceptionType::PutById:
        return true;
    default:
        return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool OSRExitDescriptor::mightArriveAtOSRExitFromCallOperation() const
{
    switch (m_exceptionType) {
    case ExceptionType::GetById:
    case ExceptionType::PutById:
    case ExceptionType::SubGenerator:
        return true;
    default:
        return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool OSRExitDescriptor::needsRegisterRecoveryOnGenericUnwindOSRExitPath() const
{
    // Calls/PutByIds/GetByIds all have a generic unwind osr exit paths.
    // But, GetById and PutById ICs will do register recovery themselves
    // because they're responsible for spilling necessary registers, so
    // they also must recover registers themselves.
    // Calls don't work this way. We compile Calls as patchpoints in LLVM.
    // A call patchpoint might pass us volatile registers for locations
    // we will do value recovery on. Therefore, before we make the call,
    // we must spill these registers. Otherwise, the call will clobber them.
    // Therefore, the corresponding OSR exit for the call will need to
    // recover the spilled registers.
    return m_exceptionType == ExceptionType::JSCall;
}

bool OSRExitDescriptor::isExceptionHandler() const
{
    return m_exceptionType != ExceptionType::None;
}

void OSRExitDescriptor::validateReferences(const TrackedReferences& trackedReferences)
{
    for (unsigned i = m_values.size(); i--;)
        m_values[i].validateReferences(trackedReferences);
    
    for (ExitTimeObjectMaterialization* materialization : m_materializations)
        materialization->validateReferences(trackedReferences);
}


OSRExit::OSRExit(OSRExitDescriptor& descriptor, uint32_t stackmapRecordIndex)
    : OSRExitBase(descriptor.m_kind, descriptor.m_codeOrigin, descriptor.m_codeOriginForExitProfile)
    , m_descriptor(descriptor)
    , m_stackmapRecordIndex(stackmapRecordIndex)
{
    m_isExceptionHandler = descriptor.isExceptionHandler();
}

CodeLocationJump OSRExit::codeLocationForRepatch(CodeBlock* ftlCodeBlock) const
{
#if FTL_USES_B3
    return CodeLocationJump(
        reinterpret_cast<char*>(
            ftlCodeBlock->jitCode()->ftl()->b3Code().code().dataLocation()) +
        m_patchableCodeOffset);
#else // FTL_USES_B3
    return CodeLocationJump(
        reinterpret_cast<char*>(
            ftlCodeBlock->jitCode()->ftl()->exitThunks().dataLocation()) +
        m_patchableCodeOffset);
#endif // FTL_USES_B3
}

void OSRExit::gatherRegistersToSpillForCallIfException(StackMaps& stackmaps, StackMaps::Record& record)
{
    RELEASE_ASSERT(m_descriptor.m_exceptionType == ExceptionType::JSCall);

    RegisterSet volatileRegisters = RegisterSet::volatileRegistersForJSCall();

    auto addNeededRegisters = [&] (const ExitValue& exitValue) {
        auto handleLocation = [&] (const FTL::Location& location) {
            if (location.involvesGPR() && volatileRegisters.get(location.gpr()))
                this->registersToPreserveForCallThatMightThrow.set(location.gpr());
            else if (location.isFPR() && volatileRegisters.get(location.fpr()))
                this->registersToPreserveForCallThatMightThrow.set(location.fpr());
        };

        switch (exitValue.kind()) {
        case ExitValueArgument:
            handleLocation(FTL::Location::forStackmaps(&stackmaps, record.locations[exitValue.exitArgument().argument()]));
            break;
        case ExitValueRecovery:
            handleLocation(FTL::Location::forStackmaps(&stackmaps, record.locations[exitValue.rightRecoveryArgument()]));
            handleLocation(FTL::Location::forStackmaps(&stackmaps, record.locations[exitValue.leftRecoveryArgument()]));
            break;
        default:
            break;
        }
    };
    for (ExitTimeObjectMaterialization* materialization : m_descriptor.m_materializations) {
        for (unsigned propertyIndex = materialization->properties().size(); propertyIndex--;)
            addNeededRegisters(materialization->properties()[propertyIndex].value());
    }
    for (unsigned index = m_descriptor.m_values.size(); index--;)
        addNeededRegisters(m_descriptor.m_values[index]);
}

void OSRExit::spillRegistersToSpillSlot(CCallHelpers& jit, int32_t stackSpillSlot)
{
    RELEASE_ASSERT(m_descriptor.mightArriveAtOSRExitFromGenericUnwind() || m_descriptor.mightArriveAtOSRExitFromCallOperation());
    unsigned count = 0;
    for (GPRReg reg = MacroAssembler::firstRegister(); reg <= MacroAssembler::lastRegister(); reg = MacroAssembler::nextRegister(reg)) {
        if (registersToPreserveForCallThatMightThrow.get(reg)) {
            jit.store64(reg, CCallHelpers::addressFor(stackSpillSlot + count));
            count++;
        }
    }
    for (FPRReg reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = MacroAssembler::nextFPRegister(reg)) {
        if (registersToPreserveForCallThatMightThrow.get(reg)) {
            jit.storeDouble(reg, CCallHelpers::addressFor(stackSpillSlot + count));
            count++;
        }
    }
}

void OSRExit::recoverRegistersFromSpillSlot(CCallHelpers& jit, int32_t stackSpillSlot)
{
    RELEASE_ASSERT(m_descriptor.mightArriveAtOSRExitFromGenericUnwind() || m_descriptor.mightArriveAtOSRExitFromCallOperation());
    unsigned count = 0;
    for (GPRReg reg = MacroAssembler::firstRegister(); reg <= MacroAssembler::lastRegister(); reg = MacroAssembler::nextRegister(reg)) {
        if (registersToPreserveForCallThatMightThrow.get(reg)) {
            jit.load64(CCallHelpers::addressFor(stackSpillSlot + count), reg);
            count++;
        }
    }
    for (FPRReg reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = MacroAssembler::nextFPRegister(reg)) {
        if (registersToPreserveForCallThatMightThrow.get(reg)) {
            jit.loadDouble(CCallHelpers::addressFor(stackSpillSlot + count), reg);
            count++;
        }
    }
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

