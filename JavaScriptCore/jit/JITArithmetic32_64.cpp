/*
* Copyright (C) 2008, 2015 Apple Inc. All rights reserved.
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

#if ENABLE(JIT)
#if USE(JSVALUE32_64)
#include "JIT.h"

#include "CodeBlock.h"
#include "JITInlines.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "Interpreter.h"
#include "JSCInlines.h"
#include "ResultType.h"
#include "SamplingTool.h"
#include "SlowPathCall.h"


namespace JSC {

void JIT::emit_op_negate(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);

    Jump srcNotInt = branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag));
    addSlowCase(branchTest32(Zero, regT0, TrustedImm32(0x7fffffff)));
    neg32(regT0);
    emitStoreInt32(dst, regT0, (dst == src));

    Jump end = jump();

    srcNotInt.link(this);
    addSlowCase(branch32(Above, regT1, TrustedImm32(JSValue::LowestTag)));

    xor32(TrustedImm32(1 << 31), regT1);
    store32(regT1, tagFor(dst));
    if (dst != src)
        store32(regT0, payloadFor(dst));

    end.link(this);
}

void JIT::emitSlow_op_negate(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter); // 0x7fffffff check
    linkSlowCase(iter); // double check

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_negate);
    slowPathCall.call();
}

void JIT::emit_compareAndJump(OpcodeID opcode, int op1, int op2, unsigned target, RelationalCondition condition)
{
    JumpList notInt32Op1;
    JumpList notInt32Op2;

    // Character less.
    if (isOperandConstantChar(op1)) {
        emitLoad(op2, regT1, regT0);
        addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag)));
        JumpList failures;
        emitLoadCharacterString(regT0, regT0, failures);
        addSlowCase(failures);
        addJump(branch32(commute(condition), regT0, Imm32(asString(getConstantOperand(op1))->tryGetValue()[0])), target);
        return;
    }
    if (isOperandConstantChar(op2)) {
        emitLoad(op1, regT1, regT0);
        addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag)));
        JumpList failures;
        emitLoadCharacterString(regT0, regT0, failures);
        addSlowCase(failures);
        addJump(branch32(condition, regT0, Imm32(asString(getConstantOperand(op2))->tryGetValue()[0])), target);
        return;
    } 
    if (isOperandConstantInt(op1)) {
        emitLoad(op2, regT3, regT2);
        notInt32Op2.append(branch32(NotEqual, regT3, TrustedImm32(JSValue::Int32Tag)));
        addJump(branch32(commute(condition), regT2, Imm32(getConstantOperand(op1).asInt32())), target);
    } else if (isOperandConstantInt(op2)) {
        emitLoad(op1, regT1, regT0);
        notInt32Op1.append(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
        addJump(branch32(condition, regT0, Imm32(getConstantOperand(op2).asInt32())), target);
    } else {
        emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
        notInt32Op1.append(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
        notInt32Op2.append(branch32(NotEqual, regT3, TrustedImm32(JSValue::Int32Tag)));
        addJump(branch32(condition, regT0, regT2), target);
    }

    if (!supportsFloatingPoint()) {
        addSlowCase(notInt32Op1);
        addSlowCase(notInt32Op2);
        return;
    }
    Jump end = jump();

    // Double less.
    emitBinaryDoubleOp(opcode, target, op1, op2, OperandTypes(), notInt32Op1, notInt32Op2, !isOperandConstantInt(op1), isOperandConstantInt(op1) || !isOperandConstantInt(op2));
    end.link(this);
}

void JIT::emit_compareAndJumpSlow(int op1, int op2, unsigned target, DoubleCondition, size_t (JIT_OPERATION *operation)(ExecState*, EncodedJSValue, EncodedJSValue), bool invert, Vector<SlowCaseEntry>::iterator& iter)
{
    if (isOperandConstantChar(op1) || isOperandConstantChar(op2)) {
        linkSlowCase(iter);
        linkSlowCase(iter);
        linkSlowCase(iter);
        linkSlowCase(iter);
    } else {
        if (!supportsFloatingPoint()) {
            if (!isOperandConstantInt(op1) && !isOperandConstantInt(op2))
                linkSlowCase(iter); // int32 check
            linkSlowCase(iter); // int32 check
        } else {
            if (!isOperandConstantInt(op1)) {
                linkSlowCase(iter); // double check
                linkSlowCase(iter); // int32 check
            }
            if (isOperandConstantInt(op1) || !isOperandConstantInt(op2))
                linkSlowCase(iter); // double check
        }
    }
    emitLoad(op1, regT1, regT0);
    emitLoad(op2, regT3, regT2);
    callOperation(operation, regT1, regT0, regT3, regT2);
    emitJumpSlowToHot(branchTest32(invert ? Zero : NonZero, returnValueGPR), target);
}

// LeftShift (<<)

void JIT::emit_op_lshift(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int op1 = currentInstruction[2].u.operand;
    int op2 = currentInstruction[3].u.operand;

    if (isOperandConstantInt(op2)) {
        emitLoad(op1, regT1, regT0);
        addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
        lshift32(Imm32(getConstantOperand(op2).asInt32()), regT0);
        emitStoreInt32(dst, regT0, dst == op1);
        return;
    }

    emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
    if (!isOperandConstantInt(op1))
        addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
    addSlowCase(branch32(NotEqual, regT3, TrustedImm32(JSValue::Int32Tag)));
    lshift32(regT2, regT0);
    emitStoreInt32(dst, regT0, dst == op1 || dst == op2);
}

void JIT::emitSlow_op_lshift(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int op1 = currentInstruction[2].u.operand;
    int op2 = currentInstruction[3].u.operand;

    if (!isOperandConstantInt(op1) && !isOperandConstantInt(op2))
        linkSlowCase(iter); // int32 check
    linkSlowCase(iter); // int32 check

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_lshift);
    slowPathCall.call();
}

// RightShift (>>) and UnsignedRightShift (>>>) helper

void JIT::emitRightShift(Instruction* currentInstruction, bool isUnsigned)
{
    int dst = currentInstruction[1].u.operand;
    int op1 = currentInstruction[2].u.operand;
    int op2 = currentInstruction[3].u.operand;

    // Slow case of rshift makes assumptions about what registers hold the
    // shift arguments, so any changes must be updated there as well.
    if (isOperandConstantInt(op2)) {
        emitLoad(op1, regT1, regT0);
        addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
        int shift = getConstantOperand(op2).asInt32() & 0x1f;
        if (shift) {
            if (isUnsigned)
                urshift32(Imm32(shift), regT0);
            else
                rshift32(Imm32(shift), regT0);
        }
        emitStoreInt32(dst, regT0, dst == op1);
    } else {
        emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
        if (!isOperandConstantInt(op1))
            addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
        addSlowCase(branch32(NotEqual, regT3, TrustedImm32(JSValue::Int32Tag)));
        if (isUnsigned)
            urshift32(regT2, regT0);
        else
            rshift32(regT2, regT0);
        emitStoreInt32(dst, regT0, dst == op1);
    }
}

void JIT::emitRightShiftSlowCase(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter, bool isUnsigned)
{
    int dst = currentInstruction[1].u.operand;
    int op1 = currentInstruction[2].u.operand;
    int op2 = currentInstruction[3].u.operand;
    if (isOperandConstantInt(op2)) {
        int shift = getConstantOperand(op2).asInt32() & 0x1f;
        // op1 = regT1:regT0
        linkSlowCase(iter); // int32 check
        if (supportsFloatingPointTruncate()) {
            JumpList failures;
            failures.append(branch32(AboveOrEqual, regT1, TrustedImm32(JSValue::LowestTag)));
            emitLoadDouble(op1, fpRegT0);
            failures.append(branchTruncateDoubleToInt32(fpRegT0, regT0));
            if (shift) {
                if (isUnsigned)
                    urshift32(Imm32(shift), regT0);
                else
                    rshift32(Imm32(shift), regT0);
            }
            move(TrustedImm32(JSValue::Int32Tag), regT1);
            emitStoreInt32(dst, regT0, false);
            emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_rshift));
            failures.link(this);
        }
    } else {
        // op1 = regT1:regT0
        // op2 = regT3:regT2
        if (!isOperandConstantInt(op1)) {
            linkSlowCase(iter); // int32 check -- op1 is not an int
            if (supportsFloatingPointTruncate()) {
                JumpList failures;
                failures.append(branch32(Above, regT1, TrustedImm32(JSValue::LowestTag))); // op1 is not a double
                emitLoadDouble(op1, fpRegT0);
                failures.append(branch32(NotEqual, regT3, TrustedImm32(JSValue::Int32Tag))); // op2 is not an int
                failures.append(branchTruncateDoubleToInt32(fpRegT0, regT0));
                if (isUnsigned)
                    urshift32(regT2, regT0);
                else
                    rshift32(regT2, regT0);
                move(TrustedImm32(JSValue::Int32Tag), regT1);
                emitStoreInt32(dst, regT0, false);
                emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_rshift));
                failures.link(this);
            }
        }

        linkSlowCase(iter); // int32 check - op2 is not an int
    }

    JITSlowPathCall slowPathCall(this, currentInstruction, isUnsigned ? slow_path_urshift : slow_path_rshift);
    slowPathCall.call();
}

// RightShift (>>)

void JIT::emit_op_rshift(Instruction* currentInstruction)
{
    emitRightShift(currentInstruction, false);
}

void JIT::emitSlow_op_rshift(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitRightShiftSlowCase(currentInstruction, iter, false);
}

// UnsignedRightShift (>>>)

void JIT::emit_op_urshift(Instruction* currentInstruction)
{
    emitRightShift(currentInstruction, true);
}

void JIT::emitSlow_op_urshift(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitRightShiftSlowCase(currentInstruction, iter, true);
}

void JIT::emit_op_unsigned(Instruction* currentInstruction)
{
    int result = currentInstruction[1].u.operand;
    int op1 = currentInstruction[2].u.operand;
    
    emitLoad(op1, regT1, regT0);
    
    addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
    addSlowCase(branch32(LessThan, regT0, TrustedImm32(0)));
    emitStoreInt32(result, regT0, result == op1);
}

void JIT::emitSlow_op_unsigned(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    linkSlowCase(iter);
    
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_unsigned);
    slowPathCall.call();
}

// BitAnd (&)

void JIT::emit_op_bitand(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int op1 = currentInstruction[2].u.operand;
    int op2 = currentInstruction[3].u.operand;

    int op;
    int32_t constant;
    if (getOperandConstantInt(op1, op2, op, constant)) {
        emitLoad(op, regT1, regT0);
        addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
        and32(Imm32(constant), regT0);
        emitStoreInt32(dst, regT0, dst == op);
        return;
    }

    emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
    addSlowCase(branch32(NotEqual, regT3, TrustedImm32(JSValue::Int32Tag)));
    and32(regT2, regT0);
    emitStoreInt32(dst, regT0, op1 == dst || op2 == dst);
}

void JIT::emitSlow_op_bitand(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int op1 = currentInstruction[2].u.operand;
    int op2 = currentInstruction[3].u.operand;

    if (!isOperandConstantInt(op1) && !isOperandConstantInt(op2))
        linkSlowCase(iter); // int32 check
    linkSlowCase(iter); // int32 check

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_bitand);
    slowPathCall.call();
}

// BitOr (|)

void JIT::emit_op_bitor(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int op1 = currentInstruction[2].u.operand;
    int op2 = currentInstruction[3].u.operand;

    int op;
    int32_t constant;
    if (getOperandConstantInt(op1, op2, op, constant)) {
        emitLoad(op, regT1, regT0);
        addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
        or32(Imm32(constant), regT0);
        emitStoreInt32(dst, regT0, op == dst);
        return;
    }

    emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
    addSlowCase(branch32(NotEqual, regT3, TrustedImm32(JSValue::Int32Tag)));
    or32(regT2, regT0);
    emitStoreInt32(dst, regT0, op1 == dst || op2 == dst);
}

void JIT::emitSlow_op_bitor(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int op1 = currentInstruction[2].u.operand;
    int op2 = currentInstruction[3].u.operand;

    if (!isOperandConstantInt(op1) && !isOperandConstantInt(op2))
        linkSlowCase(iter); // int32 check
    linkSlowCase(iter); // int32 check

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_bitor);
    slowPathCall.call();
}

// BitXor (^)

void JIT::emit_op_bitxor(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int op1 = currentInstruction[2].u.operand;
    int op2 = currentInstruction[3].u.operand;

    int op;
    int32_t constant;
    if (getOperandConstantInt(op1, op2, op, constant)) {
        emitLoad(op, regT1, regT0);
        addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
        xor32(Imm32(constant), regT0);
        emitStoreInt32(dst, regT0, op == dst);
        return;
    }

    emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
    addSlowCase(branch32(NotEqual, regT3, TrustedImm32(JSValue::Int32Tag)));
    xor32(regT2, regT0);
    emitStoreInt32(dst, regT0, op1 == dst || op2 == dst);
}

void JIT::emitSlow_op_bitxor(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int op1 = currentInstruction[2].u.operand;
    int op2 = currentInstruction[3].u.operand;

    if (!isOperandConstantInt(op1) && !isOperandConstantInt(op2))
        linkSlowCase(iter); // int32 check
    linkSlowCase(iter); // int32 check

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_bitxor);
    slowPathCall.call();
}

void JIT::emit_op_inc(Instruction* currentInstruction)
{
    int srcDst = currentInstruction[1].u.operand;

    emitLoad(srcDst, regT1, regT0);

    addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
    addSlowCase(branchAdd32(Overflow, TrustedImm32(1), regT0));
    emitStoreInt32(srcDst, regT0, true);
}

void JIT::emitSlow_op_inc(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter); // int32 check
    linkSlowCase(iter); // overflow check

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_inc);
    slowPathCall.call();
}

void JIT::emit_op_dec(Instruction* currentInstruction)
{
    int srcDst = currentInstruction[1].u.operand;

    emitLoad(srcDst, regT1, regT0);

    addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
    addSlowCase(branchSub32(Overflow, TrustedImm32(1), regT0));
    emitStoreInt32(srcDst, regT0, true);
}

void JIT::emitSlow_op_dec(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter); // int32 check
    linkSlowCase(iter); // overflow check

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_dec);
    slowPathCall.call();
}

// Addition (+)

void JIT::emitBinaryDoubleOp(OpcodeID opcodeID, int dst, int op1, int op2, OperandTypes types, JumpList& notInt32Op1, JumpList& notInt32Op2, bool op1IsInRegisters, bool op2IsInRegisters)
{
    JumpList end;

    if (!notInt32Op1.empty()) {
        // Double case 1: Op1 is not int32; Op2 is unknown.
        notInt32Op1.link(this);

        ASSERT(op1IsInRegisters);

        // Verify Op1 is double.
        if (!types.first().definitelyIsNumber())
            addSlowCase(branch32(Above, regT1, TrustedImm32(JSValue::LowestTag)));

        if (!op2IsInRegisters)
            emitLoad(op2, regT3, regT2);

        Jump doubleOp2 = branch32(Below, regT3, TrustedImm32(JSValue::LowestTag));

        if (!types.second().definitelyIsNumber())
            addSlowCase(branch32(NotEqual, regT3, TrustedImm32(JSValue::Int32Tag)));

        convertInt32ToDouble(regT2, fpRegT0);
        Jump doTheMath = jump();

        // Load Op2 as double into double register.
        doubleOp2.link(this);
        emitLoadDouble(op2, fpRegT0);

        // Do the math.
        doTheMath.link(this);
        switch (opcodeID) {
            case op_div: {
                emitLoadDouble(op1, fpRegT1);
                divDouble(fpRegT0, fpRegT1);

                // Is the result actually an integer? The DFG JIT would really like to know. If it's
                // not an integer, we increment a count. If this together with the slow case counter
                // are below threshold then the DFG JIT will compile this division with a specualtion
                // that the remainder is zero.
                
                // As well, there are cases where a double result here would cause an important field
                // in the heap to sometimes have doubles in it, resulting in double predictions getting
                // propagated to a use site where it might cause damage (such as the index to an array
                // access). So if we are DFG compiling anything in the program, we want this code to
                // ensure that it produces integers whenever possible.
                
                // FIXME: This will fail to convert to integer if the result is zero. We should
                // distinguish between positive zero and negative zero here.
                
                JumpList notInteger;
                branchConvertDoubleToInt32(fpRegT1, regT2, notInteger, fpRegT0);
                // If we've got an integer, we might as well make that the result of the division.
                emitStoreInt32(dst, regT2);
                Jump isInteger = jump();
                notInteger.link(this);
                add32(TrustedImm32(1), AbsoluteAddress(&m_codeBlock->specialFastCaseProfileForBytecodeOffset(m_bytecodeOffset)->m_counter));
                emitStoreDouble(dst, fpRegT1);
                isInteger.link(this);
                break;
            }
            case op_jless:
                emitLoadDouble(op1, fpRegT2);
                addJump(branchDouble(DoubleLessThan, fpRegT2, fpRegT0), dst);
                break;
            case op_jlesseq:
                emitLoadDouble(op1, fpRegT2);
                addJump(branchDouble(DoubleLessThanOrEqual, fpRegT2, fpRegT0), dst);
                break;
            case op_jgreater:
                emitLoadDouble(op1, fpRegT2);
                addJump(branchDouble(DoubleGreaterThan, fpRegT2, fpRegT0), dst);
                break;
            case op_jgreatereq:
                emitLoadDouble(op1, fpRegT2);
                addJump(branchDouble(DoubleGreaterThanOrEqual, fpRegT2, fpRegT0), dst);
                break;
            case op_jnless:
                emitLoadDouble(op1, fpRegT2);
                addJump(branchDouble(DoubleLessThanOrEqualOrUnordered, fpRegT0, fpRegT2), dst);
                break;
            case op_jnlesseq:
                emitLoadDouble(op1, fpRegT2);
                addJump(branchDouble(DoubleLessThanOrUnordered, fpRegT0, fpRegT2), dst);
                break;
            case op_jngreater:
                emitLoadDouble(op1, fpRegT2);
                addJump(branchDouble(DoubleGreaterThanOrEqualOrUnordered, fpRegT0, fpRegT2), dst);
                break;
            case op_jngreatereq:
                emitLoadDouble(op1, fpRegT2);
                addJump(branchDouble(DoubleGreaterThanOrUnordered, fpRegT0, fpRegT2), dst);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
        }

        if (!notInt32Op2.empty())
            end.append(jump());
    }

    if (!notInt32Op2.empty()) {
        // Double case 2: Op1 is int32; Op2 is not int32.
        notInt32Op2.link(this);

        ASSERT(op2IsInRegisters);

        if (!op1IsInRegisters)
            emitLoadPayload(op1, regT0);

        convertInt32ToDouble(regT0, fpRegT0);

        // Verify op2 is double.
        if (!types.second().definitelyIsNumber())
            addSlowCase(branch32(Above, regT3, TrustedImm32(JSValue::LowestTag)));

        // Do the math.
        switch (opcodeID) {
            case op_div: {
                emitLoadDouble(op2, fpRegT2);
                divDouble(fpRegT2, fpRegT0);
                // Is the result actually an integer? The DFG JIT would really like to know. If it's
                // not an integer, we increment a count. If this together with the slow case counter
                // are below threshold then the DFG JIT will compile this division with a specualtion
                // that the remainder is zero.
                
                // As well, there are cases where a double result here would cause an important field
                // in the heap to sometimes have doubles in it, resulting in double predictions getting
                // propagated to a use site where it might cause damage (such as the index to an array
                // access). So if we are DFG compiling anything in the program, we want this code to
                // ensure that it produces integers whenever possible.
                
                // FIXME: This will fail to convert to integer if the result is zero. We should
                // distinguish between positive zero and negative zero here.
                
                JumpList notInteger;
                branchConvertDoubleToInt32(fpRegT0, regT2, notInteger, fpRegT1);
                // If we've got an integer, we might as well make that the result of the division.
                emitStoreInt32(dst, regT2);
                Jump isInteger = jump();
                notInteger.link(this);
                add32(TrustedImm32(1), AbsoluteAddress(&m_codeBlock->specialFastCaseProfileForBytecodeOffset(m_bytecodeOffset)->m_counter));
                emitStoreDouble(dst, fpRegT0);
                isInteger.link(this);
                break;
            }
            case op_jless:
                emitLoadDouble(op2, fpRegT1);
                addJump(branchDouble(DoubleLessThan, fpRegT0, fpRegT1), dst);
                break;
            case op_jlesseq:
                emitLoadDouble(op2, fpRegT1);
                addJump(branchDouble(DoubleLessThanOrEqual, fpRegT0, fpRegT1), dst);
                break;
            case op_jgreater:
                emitLoadDouble(op2, fpRegT1);
                addJump(branchDouble(DoubleGreaterThan, fpRegT0, fpRegT1), dst);
                break;
            case op_jgreatereq:
                emitLoadDouble(op2, fpRegT1);
                addJump(branchDouble(DoubleGreaterThanOrEqual, fpRegT0, fpRegT1), dst);
                break;
            case op_jnless:
                emitLoadDouble(op2, fpRegT1);
                addJump(branchDouble(DoubleLessThanOrEqualOrUnordered, fpRegT1, fpRegT0), dst);
                break;
            case op_jnlesseq:
                emitLoadDouble(op2, fpRegT1);
                addJump(branchDouble(DoubleLessThanOrUnordered, fpRegT1, fpRegT0), dst);
                break;
            case op_jngreater:
                emitLoadDouble(op2, fpRegT1);
                addJump(branchDouble(DoubleGreaterThanOrEqualOrUnordered, fpRegT1, fpRegT0), dst);
                break;
            case op_jngreatereq:
                emitLoadDouble(op2, fpRegT1);
                addJump(branchDouble(DoubleGreaterThanOrUnordered, fpRegT1, fpRegT0), dst);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
        }
    }

    end.link(this);
}

// Division (/)

void JIT::emit_op_div(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int op1 = currentInstruction[2].u.operand;
    int op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    m_codeBlock->addSpecialFastCaseProfile(m_bytecodeOffset);

    if (!supportsFloatingPoint()) {
        addSlowCase(jump());
        return;
    }

    // Int32 divide.
    JumpList notInt32Op1;
    JumpList notInt32Op2;

    JumpList end;

    emitLoad2(op1, regT1, regT0, op2, regT3, regT2);

    notInt32Op1.append(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
    notInt32Op2.append(branch32(NotEqual, regT3, TrustedImm32(JSValue::Int32Tag)));

    convertInt32ToDouble(regT0, fpRegT0);
    convertInt32ToDouble(regT2, fpRegT1);
    divDouble(fpRegT1, fpRegT0);
    // Is the result actually an integer? The DFG JIT would really like to know. If it's
    // not an integer, we increment a count. If this together with the slow case counter
    // are below threshold then the DFG JIT will compile this division with a specualtion
    // that the remainder is zero.
    
    // As well, there are cases where a double result here would cause an important field
    // in the heap to sometimes have doubles in it, resulting in double predictions getting
    // propagated to a use site where it might cause damage (such as the index to an array
    // access). So if we are DFG compiling anything in the program, we want this code to
    // ensure that it produces integers whenever possible.
    
    // FIXME: This will fail to convert to integer if the result is zero. We should
    // distinguish between positive zero and negative zero here.
    
    JumpList notInteger;
    branchConvertDoubleToInt32(fpRegT0, regT2, notInteger, fpRegT1);
    // If we've got an integer, we might as well make that the result of the division.
    emitStoreInt32(dst, regT2);
    end.append(jump());
    notInteger.link(this);
    add32(TrustedImm32(1), AbsoluteAddress(&m_codeBlock->specialFastCaseProfileForBytecodeOffset(m_bytecodeOffset)->m_counter));
    emitStoreDouble(dst, fpRegT0);
    end.append(jump());

    // Double divide.
    emitBinaryDoubleOp(op_div, dst, op1, op2, types, notInt32Op1, notInt32Op2);
    end.link(this);
}

void JIT::emitSlow_op_div(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    if (!supportsFloatingPoint())
        linkSlowCase(iter);
    else {
        if (!types.first().definitelyIsNumber())
            linkSlowCase(iter); // double check

        if (!types.second().definitelyIsNumber()) {
            linkSlowCase(iter); // int32 check
            linkSlowCase(iter); // double check
        }
    }

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_div);
    slowPathCall.call();
}

// Mod (%)

/* ------------------------------ BEGIN: OP_MOD ------------------------------ */

void JIT::emit_op_mod(Instruction* currentInstruction)
{
#if CPU(X86)
    int dst = currentInstruction[1].u.operand;
    int op1 = currentInstruction[2].u.operand;
    int op2 = currentInstruction[3].u.operand;

    // Make sure registers are correct for x86 IDIV instructions.
    ASSERT(regT0 == X86Registers::eax);
    ASSERT(regT1 == X86Registers::edx);
    ASSERT(regT2 == X86Registers::ecx);
    ASSERT(regT3 == X86Registers::ebx);

    emitLoad2(op1, regT0, regT3, op2, regT1, regT2);
    addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
    addSlowCase(branch32(NotEqual, regT0, TrustedImm32(JSValue::Int32Tag)));

    move(regT3, regT0);
    addSlowCase(branchTest32(Zero, regT2));
    Jump denominatorNotNeg1 = branch32(NotEqual, regT2, TrustedImm32(-1));
    addSlowCase(branch32(Equal, regT0, TrustedImm32(-2147483647-1)));
    denominatorNotNeg1.link(this);
    x86ConvertToDoubleWord32();
    x86Div32(regT2);
    Jump numeratorPositive = branch32(GreaterThanOrEqual, regT3, TrustedImm32(0));
    addSlowCase(branchTest32(Zero, regT1));
    numeratorPositive.link(this);
    emitStoreInt32(dst, regT1, (op1 == dst || op2 == dst));
#else
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_mod);
    slowPathCall.call();
#endif
}

void JIT::emitSlow_op_mod(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
#if CPU(X86) || CPU(X86_64)
    linkSlowCase(iter);
    linkSlowCase(iter);
    linkSlowCase(iter);
    linkSlowCase(iter);
    linkSlowCase(iter);
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_mod);
    slowPathCall.call();
#else
    UNUSED_PARAM(currentInstruction);
    UNUSED_PARAM(iter);
    // We would have really useful assertions here if it wasn't for the compiler's
    // insistence on attribute noreturn.
    // RELEASE_ASSERT_NOT_REACHED();
#endif
}

/* ------------------------------ END: OP_MOD ------------------------------ */

} // namespace JSC

#endif // USE(JSVALUE32_64)
#endif // ENABLE(JIT)
