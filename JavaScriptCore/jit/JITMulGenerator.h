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

#ifndef JITMulGenerator_h
#define JITMulGenerator_h

#if ENABLE(JIT)

#include "CCallHelpers.h"
#include "ResultType.h"

namespace JSC {

class JITMulGenerator {
public:
    JITMulGenerator(JSValueRegs result, JSValueRegs left, JSValueRegs right,
        ResultType leftType, ResultType rightType, bool leftIsPositiveConstInt32, bool rightIsPositiveConstInt32,
        int32_t leftConstInt32, int32_t rightConstInt32, FPRReg leftFPR, FPRReg rightFPR,
        GPRReg scratchGPR, FPRReg scratchFPR, uint32_t* profilingCounter)
        : m_result(result)
        , m_left(left)
        , m_right(right)
        , m_leftType(leftType)
        , m_rightType(rightType)
        , m_leftIsPositiveConstInt32(leftIsPositiveConstInt32)
        , m_rightIsPositiveConstInt32(rightIsPositiveConstInt32)
        , m_leftConstInt32(leftConstInt32)
        , m_rightConstInt32(rightConstInt32)
        , m_leftFPR(leftFPR)
        , m_rightFPR(rightFPR)
        , m_scratchGPR(scratchGPR)
        , m_scratchFPR(scratchFPR)
        , m_profilingCounter(profilingCounter)
    {
        ASSERT(!leftIsPositiveConstInt32 || !rightIsPositiveConstInt32);
    }

    void generateFastPath(CCallHelpers&);

    bool didEmitFastPath() const { return m_didEmitFastPath; }
    CCallHelpers::JumpList& endJumpList() { return m_endJumpList; }
    CCallHelpers::JumpList& slowPathJumpList() { return m_slowPathJumpList; }

private:
    JSValueRegs m_result;
    JSValueRegs m_left;
    JSValueRegs m_right;
    ResultType m_leftType;
    ResultType m_rightType;
    bool m_leftIsPositiveConstInt32;
    bool m_rightIsPositiveConstInt32;
    int32_t m_leftConstInt32;
    int32_t m_rightConstInt32;
    FPRReg m_leftFPR;
    FPRReg m_rightFPR;
    GPRReg m_scratchGPR;
    FPRReg m_scratchFPR;
    uint32_t* m_profilingCounter;
    bool m_didEmitFastPath { false };

    CCallHelpers::JumpList m_endJumpList;
    CCallHelpers::JumpList m_slowPathJumpList;
};

} // namespace JSC

#endif // ENABLE(JIT)

#endif // JITMulGenerator_h
