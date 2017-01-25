#!/usr/bin/env ruby

# Copyright (C) 2015 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

require "pathname"

class Opcode
    attr_reader :name, :special, :overloads
    attr_reader :attributes

    def initialize(name, special)
        @name = name
        @special = special
        @attributes = {}
        unless special
            @overloads = []
        end
    end

    def masmName
        name[0].downcase + name[1..-1]
    end
end

class Arg
    attr_reader :role, :type

    def initialize(role, type)
        @role = role
        @type = type
    end
end

class Overload
    attr_reader :signature, :forms

    def initialize(signature, forms)
        @signature = signature
        @forms = forms
    end
end

class Kind
    attr_reader :name
    attr_accessor :special

    def initialize(name)
        @name = name
        @special = false
    end

    def ==(other)
        if other.is_a? String
            @name == other
        else
            @name == other.name and @special == other.special
        end
    end

    def Kind.argKinds(kind)
        if kind == "Addr"
            ["Addr", "Stack", "CallArg"]
        else
            [kind]
        end
    end

    def argKinds
        Kind.argKinds(kind)
    end
end

class Form
    attr_reader :kinds, :altName

    def initialize(kinds, altName)
        @kinds = kinds
        @altName = altName
    end
end

class Origin
    attr_reader :fileName, :lineNumber
    
    def initialize(fileName, lineNumber)
        @fileName = fileName
        @lineNumber = lineNumber
    end
    
    def to_s
        "#{fileName}:#{lineNumber}"
    end
end

class Token
    attr_reader :origin, :string
    
    def initialize(origin, string)
        @origin = origin
        @string = string
    end
    
    def ==(other)
        if other.is_a? Token
            @string == other.string
        else
            @string == other
        end
    end
    
    def =~(other)
        @string =~ other
    end
    
    def to_s
        "#{@string.inspect} at #{origin}"
    end
    
    def parseError(*comment)
        if comment.empty?
            raise "Parse error: #{to_s}"
        else
            raise "Parse error: #{to_s}: #{comment[0]}"
        end
    end
end

def lex(str, fileName)
    fileName = Pathname.new(fileName)
    result = []
    lineNumber = 1
    while not str.empty?
        case str
        when /\A\#([^\n]*)/
            # comment, ignore
        when /\A\n/
            # newline, ignore
            lineNumber += 1
        when /\A([a-zA-Z0-9_]+)/
            result << Token.new(Origin.new(fileName, lineNumber), $&)
        when /\A([ \t\r]+)/
            # whitespace, ignore
        when /\A[,:*\/]/
            result << Token.new(Origin.new(fileName, lineNumber), $&)
        else
            raise "Lexer error at #{Origin.new(fileName, lineNumber).to_s}, unexpected sequence #{str[0..20].inspect}"
        end
        str = $~.post_match
    end
    result
end

def isUD(token)
    token =~ /\A((U)|(D)|(UD)|(UA))\Z/
end

def isGF(token)
    token =~ /\A((G)|(F))\Z/
end

def isKind(token)
    token =~ /\A((Tmp)|(Imm)|(Imm64)|(Addr)|(Index)|(RelCond)|(ResCond)|(DoubleCond))\Z/
end

def isKeyword(token)
    isUD(token) or isGF(token) or isKind(token) or
        token == "special" or token == "as"
end

def isIdentifier(token)
    token =~ /\A([a-zA-Z0-9_]+)\Z/ and not isKeyword(token)
end

class Parser
    def initialize(data, fileName)
        @tokens = lex(data, fileName)
        @idx = 0
    end

    def token
        @tokens[@idx]
    end

    def advance
        @idx += 1
    end

    def parseError(*comment)
        if token
            token.parseError(*comment)
        else
            if comment.empty?
                raise "Parse error at end of file"
            else
                raise "Parse error at end of file: #{comment[0]}"
            end
        end
    end

    def consume(string)
        parseError("Expected #{string}") unless token == string
        advance
    end

    def consumeIdentifier
        result = token.string
        parseError("Expected identifier") unless isIdentifier(result)
        advance
        result
    end

    def consumeRole
        result = token.string
        parseError("Expected role (U, D, UD, or UA)") unless isUD(result)
        advance
        result
    end

    def consumeType
        result = token.string
        parseError("Expected type (G or F)") unless isGF(result)
        advance
        result
    end

    def consumeKind
        result = token.string
        parseError("Expected kind (Imm, Imm64, Tmp, Addr, Index, RelCond, ResCond, or DoubleCond)") unless isKind(result)
        advance
        result
    end

    def parse
        result = {}
        
        loop {
            break if @idx >= @tokens.length

            if token == "special"
                consume("special")
                opcodeName = consumeIdentifier

                parseError("Cannot overload a special opcode") if result[opcodeName]

                result[opcodeName] = Opcode.new(opcodeName, true)
            else
                opcodeName = consumeIdentifier

                if result[opcodeName]
                    opcode = result[opcodeName]
                    parseError("Cannot overload a special opcode") if opcode.special
                else
                    opcode = Opcode.new(opcodeName, false)
                    result[opcodeName] = opcode
                end

                signature = []
                forms = []
                
                if isUD(token)
                    loop {
                        role = consumeRole
                        consume(":")
                        type = consumeType
                        
                        signature << Arg.new(role, type)
                        
                        break unless token == ","
                        consume(",")
                    }
                end

                while token == "/"
                    consume("/")
                    case token.string
                    when "branch"
                        opcode.attributes[:branch] = true
                    when "terminal"
                        opcode.attributes[:terminal] = true
                    when "effects"
                        opcode.attributes[:effects] = true
                    else
                        parseError("Bad / directive")
                    end
                    advance
                end

                if isKind(token)
                    loop {
                        kinds = []
                        altName = nil
                        loop {
                            kinds << Kind.new(consumeKind)

                            if token == "*"
                                parseError("Can only apply * to Tmp") unless kinds[-1].name == "Tmp"
                                kinds[-1].special = true
                                consume("*")
                            end

                            break unless token == ","
                            consume(",")
                        }

                        if token == "as"
                            consume("as")
                            altName = consumeIdentifier
                        end

                        parseError("Form has wrong number of arguments for overload") unless kinds.length == signature.length
                        kinds.each_with_index {
                            | kind, index |
                            if kind.name == "Imm" or kind.name == "Imm64"
                                if signature[index].role != "U"
                                    parseError("Form has an immediate for a non-use argument")
                                end
                                if signature[index].type != "G"
                                    parseError("Form has an immediate for a non-general-purpose argument")
                                end
                            end
                        }
                        forms << Form.new(kinds, altName)
                        break unless isKind(token)
                    }
                end

                if signature.length == 0
                    raise unless forms.length == 0
                    forms << Form.new([], nil)
                end

                opcode.overloads << Overload.new(signature, forms)
            end
        }

        result
    end
end

$fileName = ARGV[0]

parser = Parser.new(IO::read($fileName), $fileName)
$opcodes = parser.parse

$stderr.puts "Generating code for #{$fileName}."

def writeH(filename)
    File.open("Air#{filename}.h", "w") {
        | outp |
        
        outp.puts "// Generated by opcode_generator.rb from #{$fileName} -- do not edit!"
        
        outp.puts "#ifndef Air#{filename}_h"
        outp.puts "#define Air#{filename}_h"

        yield outp
        
        outp.puts "#endif // Air#{filename}_h"
    }
end

writeH("Opcode") {
    | outp |
    outp.puts "namespace JSC { namespace B3 { namespace Air {"
    outp.puts "enum Opcode : int16_t {"
    $opcodes.keys.sort.each {
        | opcode |
        outp.puts "    #{opcode},"
    }
    outp.puts "};"

    outp.puts "static const unsigned numOpcodes = #{$opcodes.keys.size};"
    outp.puts "} } } // namespace JSC::B3::Air"
    
    outp.puts "namespace WTF {"
    outp.puts "class PrintStream;"
    outp.puts "void printInternal(PrintStream&, JSC::B3::Air::Opcode);"
    outp.puts "} // namespace WTF"
}

# From here on, we don't try to emit properly indented code, since we're using a recursive pattern
# matcher.

def matchForms(outp, speed, forms, columnIndex, columnGetter, filter, callback)
    return if forms.length == 0

    if filter[forms]
        return
    end

    if columnIndex >= forms[0].kinds.length
        raise "Did not reduce to one form: #{forms.inspect}" unless forms.length == 1
        callback[forms[0]]
        return
    end
    
    groups = {}
    forms.each {
        | form |
        kind = form.kinds[columnIndex].name
        if groups[kind]
            groups[kind] << form
        else
            groups[kind] = [form]
        end
    }

    if speed == :fast and groups.length == 1
        matchForms(outp, speed, forms, columnIndex + 1, columnGetter, filter, callback)
        return
    end

    outp.puts "switch (#{columnGetter[columnIndex]}) {"
    groups.each_pair {
        | key, value |
        outp.puts "#if USE(JSVALUE64)" if key == "Imm64"
        Kind.argKinds(key).each {
            | argKind |
            outp.puts "case Arg::#{argKind}:"
        }
        matchForms(outp, speed, value, columnIndex + 1, columnGetter, filter, callback)
        outp.puts "break;"
        outp.puts "#endif // USE(JSVALUE64)" if key == "Imm64"
    }
    outp.puts "default:"
    outp.puts "break;"
    outp.puts "}"
end

def matchInstOverload(outp, speed, inst)
    outp.puts "switch (#{inst}->opcode) {"
    $opcodes.values.each {
        | opcode |
        outp.puts "case #{opcode.name}:"
        if opcode.special
            yield opcode, nil
        else
            needOverloadSwitch = ((opcode.overloads.size != 1) or speed == :safe)
            outp.puts "switch (#{inst}->args.size()) {" if needOverloadSwitch
            opcode.overloads.each {
                | overload |
                outp.puts "case #{overload.signature.length}:" if needOverloadSwitch
                yield opcode, overload
                outp.puts "break;" if needOverloadSwitch
            }
            if needOverloadSwitch
                outp.puts "default:"
                outp.puts "break;"
                outp.puts "}"
            end
        end
        outp.puts "break;"
    }
    outp.puts "default:"
    outp.puts "break;"
    outp.puts "}"
end
    
def matchInstOverloadForm(outp, speed, inst)
    matchInstOverload(outp, speed, inst) {
        | opcode, overload |
        if opcode.special
            yield opcode, nil, nil
        else
            columnGetter = proc {
                | columnIndex |
                "#{inst}->args[#{columnIndex}].kind()"
            }
            filter = proc { false }
            callback = proc {
                | form |
                yield opcode, overload, form
            }
            matchForms(outp, speed, overload.forms, 0, columnGetter, filter, callback)
        end
    }
end

writeH("OpcodeUtils") {
    | outp |
    outp.puts "#include \"AirInst.h\""
    outp.puts "#include \"AirSpecial.h\""
    outp.puts "namespace JSC { namespace B3 { namespace Air {"
    
    outp.puts "inline bool opgenHiddenTruth() { return true; }"
    outp.puts "template<typename T>"
    outp.puts "inline T* opgenHiddenPtrIdentity(T* pointer) { return pointer; }"
    outp.puts "#define OPGEN_RETURN(value) do {\\"
    outp.puts "    if (opgenHiddenTruth())\\"
    outp.puts "        return value;\\"
    outp.puts "} while (false)"

    outp.puts "template<typename Functor>"
    outp.puts "void Inst::forEachArg(const Functor& functor)"
    outp.puts "{"
    matchInstOverload(outp, :fast, "this") {
        | opcode, overload |
        if opcode.special
            outp.puts "functor(args[0], Arg::Use, Arg::GP); // This is basically bogus, but it works f analyses model Special as an immediate."
            outp.puts "args[0].special()->forEachArg(*this, scopedLambda<EachArgCallback>(functor));"
        else
            overload.signature.each_with_index {
                | arg, index |
                role = nil
                case arg.role
                when "U"
                    role = "Use"
                when "D"
                    role = "Def"
                when "UD"
                    role = "UseDef"
                when "UA"
                    role = "UseAddr"
                else
                    raise
                end
                
                outp.puts "functor(args[#{index}], Arg::#{role}, Arg::#{arg.type}P);"
            }
        end
    }
    outp.puts "}"

    outp.puts "template<typename... Arguments>"
    outp.puts "ALWAYS_INLINE bool isValidForm(Opcode opcode, Arguments... arguments)"
    outp.puts "{"
    outp.puts "Arg::Kind kinds[sizeof...(Arguments)] = { arguments... };"
    outp.puts "switch (opcode) {"
    $opcodes.values.each {
        | opcode |
        outp.puts "case #{opcode.name}:"
        outp.puts "switch (sizeof...(Arguments)) {"
        unless opcode.special
            opcode.overloads.each {
                | overload |
                outp.puts "case #{overload.signature.length}:"
                columnGetter = proc { | columnIndex | "opgenHiddenPtrIdentity(kinds)[#{columnIndex}]" }
                filter = proc { false }
                callback = proc {
                    | form |
                    special = (not form.kinds.detect { | kind | kind.special })
                    outp.puts "OPGEN_RETURN(#{special});"
                }
                matchForms(outp, :safe, overload.forms, 0, columnGetter, filter, callback)
                outp.puts "break;"
            }
        end
        outp.puts "default:"
        outp.puts "break;"
        outp.puts "}"
        outp.puts "break;"
    }
    outp.puts "default:"
    outp.puts "break;"
    outp.puts "}"
    outp.puts "return false; "
    outp.puts "}"

    outp.puts "inline bool isTerminal(Opcode opcode)"
    outp.puts "{"
    outp.puts "switch (opcode) {"
    $opcodes.values.each {
        | opcode |
        if opcode.attributes[:terminal] or opcode.attributes[:branch]
            outp.puts "case #{opcode.name}:"
        end
    }
    outp.puts "return true;"
    outp.puts "default:"
    outp.puts "return false;"
    outp.puts "}"
    outp.puts "}"
    
    outp.puts "} } } // namespace JSC::B3::Air"
}

writeH("OpcodeGenerated") {
    | outp |
    outp.puts "#include \"AirInstInlines.h\""
    outp.puts "#include \"wtf/PrintStream.h\""
    outp.puts "namespace WTF {"
    outp.puts "using namespace JSC::B3::Air;"
    outp.puts "void printInternal(PrintStream& out, Opcode opcode)"
    outp.puts "{"
    outp.puts "    switch (opcode) {"
    $opcodes.keys.each {
        | opcode |
        outp.puts "    case #{opcode}:"
        outp.puts "        out.print(\"#{opcode}\");"
        outp.puts "        return;"
    }
    outp.puts "    }"
    outp.puts "    RELEASE_ASSERT_NOT_REACHED();"
    outp.puts "}"
    outp.puts "} // namespace WTF"
    outp.puts "namespace JSC { namespace B3 { namespace Air {"
    outp.puts "bool Inst::isValidForm()"
    outp.puts "{"
    matchInstOverloadForm(outp, :safe, "this") {
        | opcode, overload, form |
        if opcode.special
            outp.puts "if (args.size() < 1)"
            outp.puts "return false;"
            outp.puts "if (!args[0].isSpecial())"
            outp.puts "return false;"
            outp.puts "OPGEN_RETURN(args[0].special()->isValid(*this));"
        else
            needsMoreValidation = false
            overload.signature.length.times {
                | index |
                role = overload.signature[index].role
                type = overload.signature[index].type
                kind = form.kinds[index]
                needsMoreValidation |= kind.special
                
                # We already know that the form matches. We don't have to validate the role, since
                # kind implies role. So, the only thing left to validate is the type. And we only have
                # to validate the type if we have a Tmp.
                if kind.name == "Tmp"
                    outp.puts "if (!args[#{index}].tmp().is#{type}P())"
                    outp.puts "OPGEN_RETURN(false);"
                end
            }
            if needsMoreValidation
                outp.puts "if (!is#{opcode.name}Valid(*this))"
                outp.puts "OPGEN_RETURN(false);"
            end
            outp.puts "OPGEN_RETURN(true);"
        end
    }
    outp.puts "return false;"
    outp.puts "}"

    outp.puts "bool Inst::admitsStack(unsigned argIndex)"
    outp.puts "{"
    outp.puts "switch (opcode) {"
    $opcodes.values.each {
        | opcode |
        outp.puts "case #{opcode.name}:"

        if opcode.special
            outp.puts "if (!argIndex)"
            outp.puts "return false;"
            outp.puts "OPGEN_RETURN(args[0].special()->admitsStack(*this, argIndex));"
        else
            # Switch on the argIndex.
            outp.puts "switch (argIndex) {"

            numArgs = opcode.overloads.map {
                | overload |
                overload.signature.length
            }.max
            
            numArgs.times {
                | argIndex |
                outp.puts "case #{argIndex}:"

                # Check if all of the forms of all of the overloads either do, or don't, admit an address
                # at this index. We expect this to be a very common case.
                numYes = 0
                numNo = 0
                opcode.overloads.each {
                    | overload |
                    overload.forms.each {
                        | form |
                        if form.kinds[argIndex] == "Addr"
                            numYes += 1
                        else
                            numNo += 1
                        end
                    }
                }

                # Note that we deliberately test numYes first because if we end up with no forms, we want
                # to say that Address is inadmissible.
                if numYes == 0
                    outp.puts "OPGEN_RETURN(false);"
                elsif numNo == 0
                    outp.puts "OPGEN_RETURN(true);"
                else
                    # Now do the full test.

                    needOverloadSwitch = (opcode.overloads.size != 1)

                    outp.puts "switch (args.size()) {" if needOverloadSwitch
                    opcode.overloads.each {
                        | overload |

                        # Again, check if all of them do what we want.
                        numYes = 0
                        numNo = 0
                        overload.forms.each {
                            | form |
                            if form.kinds[argIndex] == "Addr"
                                numYes += 1
                            else
                                numNo += 1
                            end
                        }

                        if numYes == 0
                        # Don't emit anything, just drop to default.
                        elsif numNo == 0
                            outp.puts "case #{overload.signature.length}:" if needOverloadSwitch
                            outp.puts "OPGEN_RETURN(true);"
                            outp.puts "break;" if needOverloadSwitch
                        else
                            outp.puts "case #{overload.signature.length}:" if needOverloadSwitch

                            # This is how we test the hypothesis that changing this argument to an
                            # address yields a valid form.
                            columnGetter = proc {
                                | columnIndex |
                                if columnIndex == argIndex
                                    "Arg::Addr"
                                else
                                    "args[#{columnIndex}].kind()"
                                end
                            }
                            filter = proc {
                                | forms |
                                numYes = 0

                                forms.each {
                                    | form |
                                    if form.kinds[argIndex] == "Addr"
                                        numYes += 1
                                    end
                                }

                                if numYes == 0
                                    # Drop down, emit no code, since we cannot match.
                                    true
                                else
                                    # Keep going.
                                    false
                                end
                            }
                            callback = proc {
                                outp.puts "OPGEN_RETURN(true);"
                            }
                            matchForms(outp, :safe, overload.forms, 0, columnGetter, filter, callback)

                            outp.puts "break;" if needOverloadSwitch
                        end
                    }
                    if needOverloadSwitch
                        outp.puts "default:"
                        outp.puts "break;"
                        outp.puts "}"
                    end
                end
                
                outp.puts "break;"
            }
            
            outp.puts "default:"
            outp.puts "break;"
            outp.puts "}"
        end
        
        outp.puts "break;"
    }
    outp.puts "default:";
    outp.puts "break;"
    outp.puts "}"
    outp.puts "return false;"
    outp.puts "}"

    outp.puts "bool Inst::hasNonArgNonControlEffects()"
    outp.puts "{"
    outp.puts "switch (opcode) {"
    foundTrue = false
    $opcodes.values.each {
        | opcode |
        if opcode.attributes[:effects]
            outp.puts "case #{opcode.name}:"
            foundTrue = true
        end
    }
    if foundTrue
        outp.puts "return true;"
    end
    foundTrue = false
    $opcodes.values.each {
        | opcode |
        if opcode.special
            outp.puts "case #{opcode.name}:"
            foundTrue = true
        end
    }
    if foundTrue
        outp.puts "return args[0].special()->hasNonArgNonControlEffects();"
    end
    outp.puts "default:"
    outp.puts "return false;"
    outp.puts "}"
    outp.puts "}"
    
    outp.puts "bool Inst::hasNonArgEffects()"
    outp.puts "{"
    outp.puts "switch (opcode) {"
    foundTrue = false
    $opcodes.values.each {
        | opcode |
        if opcode.attributes[:branch] or opcode.attributes[:terminal] or opcode.attributes[:effects]
            outp.puts "case #{opcode.name}:"
            foundTrue = true
        end
    }
    if foundTrue
        outp.puts "return true;"
    end
    foundTrue = false
    $opcodes.values.each {
        | opcode |
        if opcode.special
            outp.puts "case #{opcode.name}:"
            foundTrue = true
        end
    }
    if foundTrue
        outp.puts "return args[0].special()->hasNonArgNonControlEffects();"
    end
    outp.puts "default:"
    outp.puts "return false;"
    outp.puts "}"
    outp.puts "}"
    
    outp.puts "CCallHelpers::Jump Inst::generate(CCallHelpers& jit, GenerationContext& context)"
    outp.puts "{"
    outp.puts "UNUSED_PARAM(jit);"
    outp.puts "UNUSED_PARAM(context);"
    outp.puts "CCallHelpers::Jump result;"
    matchInstOverloadForm(outp, :fast, "this") {
        | opcode, overload, form |
        if opcode.special
            outp.puts "OPGEN_RETURN(args[0].special()->generate(*this, jit, context));"
        else
            if form.altName
                methodName = form.altName
            else
                methodName = opcode.masmName
            end
            if opcode.attributes[:branch]
                outp.print "result = "
            end
            outp.print "jit.#{methodName}("

            form.kinds.each_with_index {
                | kind, index |
                if index != 0
                    outp.print ", "
                end
                case kind.name
                when "Tmp"
                    if overload.signature[index].type == "G"
                        outp.print "args[#{index}].gpr()"
                    else
                        outp.print "args[#{index}].fpr()"
                    end
                when "Imm"
                    outp.print "args[#{index}].asTrustedImm32()"
                when "Imm64"
                    outp.print "args[#{index}].asTrustedImm64()"
                when "Addr"
                    outp.print "args[#{index}].asAddress()"
                when "Index"
                    outp.print "args[#{index}].asBaseIndex()"
                when "RelCond"
                    outp.print "args[#{index}].asRelationalCondition()"
                when "ResCond"
                    outp.print "args[#{index}].asResultCondition()"
                when "DoubleCond"
                    outp.print "args[#{index}].asDoubleCondition()"
                end
            }

            outp.puts ");"
            outp.puts "OPGEN_RETURN(result);"
        end
    }
    outp.puts "RELEASE_ASSERT_NOT_REACHED();"
    outp.puts "return result;"
    outp.puts "}"

    outp.puts "} } } // namespace JSC::B3::Air"
}

