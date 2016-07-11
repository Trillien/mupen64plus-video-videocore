// mupen64plus-video-videocore/VCShaderCompiler.cpp
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#include "Combiner.h"
#include "VCCombiner.h"
#include "VCShaderCompiler.h"
#include "VCUtils.h"
#include "uthash.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>

#define VC_SHADER_INSTRUCTION_MOVE          0
#define VC_SHADER_INSTRUCTION_ADD           1
#define VC_SHADER_INSTRUCTION_SUB           2
#define VC_SHADER_INSTRUCTION_MUL           3

#define VC_SHADER_MAX_INSTRUCTIONS          16
#define VC_SHADER_MAX_CONSTANTS             16

#define VC_SHADER_COMPILER_REGISTER_VALUE       0x00
#define VC_SHADER_COMPILER_CONSTANT_VALUE       0x80
#define VC_SHADER_COMPILER_LEAST_SPECIAL_VALUE  0xfa
#define VC_SHADER_COMPILER_TEXEL0_VALUE         0xfa
#define VC_SHADER_COMPILER_TEXEL0_ALPHA_VALUE   0xfb
#define VC_SHADER_COMPILER_TEXEL1_VALUE         0xfc
#define VC_SHADER_COMPILER_TEXEL1_ALPHA_VALUE   0xfd
#define VC_SHADER_COMPILER_SHADE_VALUE          0xfe
#define VC_SHADER_COMPILER_OUTPUT_VALUE         0xff
#define VC_SHADER_COMPILER_TYPE_MASK            0x80

// SSA form instruction.
struct VCShaderInstruction {
    uint8_t operation;
    uint8_t destination;
    uint8_t operands[2];
};

struct VCShaderFunction {
    VCShaderInstruction *instructions;
    size_t instructionCount;
    VCColor *constants;
    size_t constantCount;
};

struct VCShaderSubprogram {
    VCShaderSubprogramSource source;
    VCShaderFunction rgb;
    VCShaderFunction a;
};

struct VCShaderSubprogramEntry {
    VCShaderSubprogramSignature signature;
    VCShaderSubprogram *subprogram;
    UT_hash_handle hh;
};

struct VCShaderSubprogramLibrary {
    VCShaderSubprogramSignature *signatures;
    VCShaderSubprogramEntry *subprograms;
};

struct VCShaderProgram {
    VCShaderSubprogram **subprograms;
    size_t subprogramCount;
};

struct VCUnpackedCombinerFunction {
    uint8_t sa;
    uint8_t sb;
    uint8_t m;
    uint8_t a;
};

struct VCShaderSubprogramSignatureTableEntry {
    VCShaderSubprogramSignature signature;
    uint16_t id;
    UT_hash_handle hh;
};

struct VCShaderSubprogramSignatureListEntry {
    VCShaderSubprogramSignature signature;
};

struct VCShaderProgramDescriptorLibrary {
    VCShaderProgramDescriptor *shaderProgramDescriptors;
    size_t shaderProgramDescriptorCount;
};

struct VCShaderConstantMergePoolEntry {
    VCColor constant;
    uint8_t oldIndex;
    uint8_t newIndex;
    UT_hash_handle hh;
};

VCShaderProgramDescriptorLibrary *VCShaderCompiler_CreateShaderProgramDescriptorLibrary() {
    VCShaderProgramDescriptorLibrary *library =
        (VCShaderProgramDescriptorLibrary *)malloc(sizeof(VCShaderProgramDescriptorLibrary));
    library->shaderProgramDescriptors = NULL;
    library->shaderProgramDescriptorCount = 0;
    return library;
}

static VCShaderProgramSignature VCShaderCompiler_CreateShaderProgramSignature(
        VCShaderSubprogramSignatureList *list) {
    VCShaderProgramSignature signature;
    signature.serializedData = VCString_Create();
    VCString_AppendBytes(&signature.serializedData, &list->length, sizeof(list->length));
    for (size_t subprogramSignatureIndex = 0;
         subprogramSignatureIndex < list->length;
         subprogramSignatureIndex++) {
        VCString_AppendString(&signature.serializedData,
                              &list->entries[subprogramSignatureIndex].signature.serializedData);
    }
    return signature;
}

static void VCShaderCompiler_DestroyShaderProgramSignature(VCShaderProgramSignature *signature) {
    VCString_Destroy(&signature->serializedData);
}

static VCShaderSubprogramSignatureList VCShaderCompiler_DuplicateShaderSubprogramSignatureList(
        VCShaderSubprogramSignatureList *list) {
    VCShaderSubprogramSignatureList newList;
    newList.length = list->length;
    size_t byteSize = sizeof(VCShaderSubprogramSignatureListEntry) * newList.length;
    newList.entries = (VCShaderSubprogramSignatureListEntry *)malloc(byteSize);
    memcpy(newList.entries, list->entries, byteSize);
    return newList;
}

// Takes ownership of `programSignature`.
static VCShaderProgramDescriptor *VCShaderCompiler_CreateShaderProgramDescriptor(
        uint16_t id,
        VCShaderProgramSignature *programSignature,
        VCShaderSubprogramSignatureList *subprogramSignatures) {
    VCShaderProgramDescriptor *programDescriptor =
        (VCShaderProgramDescriptor *)malloc(sizeof(VCShaderProgramDescriptor));
    programDescriptor->id = id;
    programDescriptor->program = NULL;
    programDescriptor->programSignature = *programSignature;
    programDescriptor->subprogramSignatures =
        VCShaderCompiler_DuplicateShaderSubprogramSignatureList(subprogramSignatures);
    memset(programSignature, '\0', sizeof(*programSignature));
    return programDescriptor;
}

uint16_t VCShaderCompiler_GetOrCreateShaderProgramID(
        VCShaderProgramDescriptorLibrary *library,
        VCShaderSubprogramSignatureList *subprogramSignatures,
        bool *newlyCreated) {
    VCShaderProgramSignature programSignature =
        VCShaderCompiler_CreateShaderProgramSignature(subprogramSignatures);
    VCShaderProgramDescriptor *programDescriptor = NULL;
    HASH_FIND(hh,
              library->shaderProgramDescriptors,
              programSignature.serializedData.ptr,
              programSignature.serializedData.len,
              programDescriptor);
    *newlyCreated = programDescriptor == NULL;
    if (programDescriptor != NULL) {
        VCShaderCompiler_DestroyShaderProgramSignature(&programSignature);
        return programDescriptor->id;
    }

    uint16_t id = library->shaderProgramDescriptorCount;
    library->shaderProgramDescriptorCount++;
    programDescriptor = VCShaderCompiler_CreateShaderProgramDescriptor(id,
                                                                       &programSignature,
                                                                       subprogramSignatures);
    HASH_ADD_KEYPTR(hh,
                    library->shaderProgramDescriptors,
                    programDescriptor->programSignature.serializedData.ptr,
                    programDescriptor->programSignature.serializedData.len,
                    programDescriptor);
    return id;
}

static uint8_t VCShaderCompiler_GetConstant(VCShaderFunction *function, VCColor *color) {
    size_t constantIndex = function->constantCount;
    assert(constantIndex < VC_SHADER_MAX_CONSTANTS);
    function->constants[constantIndex] = *color;
    function->constantCount++;
    return VC_SHADER_COMPILER_CONSTANT_VALUE | (uint8_t)constantIndex;
}

static uint8_t VCShaderCompiler_GetScalarConstant(VCShaderFunction *function, uint8_t k) {
    VCColor color = { k, k, k, k };
    return VCShaderCompiler_GetConstant(function, &color);
}

static uint8_t VCShaderCompiler_GetValue(VCShaderSubprogramContext *context,
                                         VCShaderFunction *function,
                                         uint8_t source,
                                         bool allowCombined) {
    switch (source) {
    case G_CCMUX_COMBINED:
    case G_CCMUX_COMBINED_ALPHA:
        // FIXME(tachi): Support G_CCMUX_COMBINED_ALPHA!
        if (allowCombined)
            return VC_SHADER_COMPILER_REGISTER_VALUE | 2;
        return VCShaderCompiler_GetScalarConstant(function, 0);
    case G_CCMUX_TEXEL0:
        return VC_SHADER_COMPILER_TEXEL0_VALUE;
    case G_CCMUX_TEXEL0_ALPHA:
        return VC_SHADER_COMPILER_TEXEL0_ALPHA_VALUE;
    case G_CCMUX_TEXEL1:
        return VC_SHADER_COMPILER_TEXEL1_VALUE;
    case G_CCMUX_TEXEL1_ALPHA:
        return VC_SHADER_COMPILER_TEXEL1_ALPHA_VALUE;
    case G_CCMUX_SHADE:
        return VC_SHADER_COMPILER_SHADE_VALUE;
    case G_CCMUX_PRIMITIVE:
        return VCShaderCompiler_GetConstant(function, &context->primColor);
    case G_CCMUX_ENVIRONMENT:
        return VCShaderCompiler_GetConstant(function, &context->envColor);
    case G_CCMUX_PRIMITIVE_ALPHA:
        return VCShaderCompiler_GetScalarConstant(function, context->primColor.a);
    case G_CCMUX_ENV_ALPHA:
        return VCShaderCompiler_GetScalarConstant(function, context->envColor.a);
    case G_CCMUX_0:
        return VCShaderCompiler_GetScalarConstant(function, 0);
    case G_CCMUX_1:
        return VCShaderCompiler_GetScalarConstant(function, 255);
    }
    // FIXME(tachi): Support K5!
    return VCShaderCompiler_GetScalarConstant(function, 0);
}

static VCShaderInstruction *VCShaderCompiler_AllocateInstruction(VCShaderFunction *function) {
    assert(function->instructionCount < VC_SHADER_MAX_INSTRUCTIONS);
    VCShaderInstruction *instruction = &function->instructions[function->instructionCount];
    function->instructionCount++;
    return instruction;
}

static uint8_t VCShaderCompiler_GenerateInstructionsForCycle(VCShaderSubprogramContext *context,
                                                             VCShaderFunction *function,
                                                             VCUnpackedCombinerFunction *cycle,
                                                             uint8_t firstRegister,
                                                             bool allowCombined) {
    VCShaderInstruction *instruction = VCShaderCompiler_AllocateInstruction(function);
    instruction->operation = VC_SHADER_INSTRUCTION_SUB;
    instruction->destination = firstRegister;
    instruction->operands[0] = VCShaderCompiler_GetValue(context,
                                                         function,
                                                         cycle->sa,
                                                         allowCombined);
    instruction->operands[1] = VCShaderCompiler_GetValue(context,
                                                         function,
                                                         cycle->sb,
                                                         allowCombined);

    instruction = VCShaderCompiler_AllocateInstruction(function);
    instruction->operation = VC_SHADER_INSTRUCTION_MUL;
    instruction->destination = firstRegister + 1;
    instruction->operands[0] = firstRegister;
    instruction->operands[1] = VCShaderCompiler_GetValue(context,
                                                         function,
                                                         cycle->m,
                                                         allowCombined);

    instruction = VCShaderCompiler_AllocateInstruction(function);
    instruction->operation = VC_SHADER_INSTRUCTION_ADD;
    instruction->destination = firstRegister + 2;
    instruction->operands[0] = firstRegister + 1;
    instruction->operands[1] = VCShaderCompiler_GetValue(context,
                                                         function,
                                                         cycle->a,
                                                         allowCombined);

    return firstRegister + 2;
}

static bool VCShaderCompiler_OperandIsEqualToScalarConstant(VCShaderFunction *function,
                                                            uint8_t operand,
                                                            uint8_t scalar) {
    if ((operand & VC_SHADER_COMPILER_CONSTANT_VALUE) == 0)
        return false;
    if (operand >= VC_SHADER_COMPILER_LEAST_SPECIAL_VALUE)
        return false;
    VCColor color = function->constants[operand & ~VC_SHADER_COMPILER_CONSTANT_VALUE];
    return color.r == scalar && color.g == scalar && color.b == scalar && color.a == scalar;
}

static bool VCShaderCompiler_CombineInstructions(VCShaderFunction *function) {
    bool changed = false;
    for (size_t instructionIndex = 0;
         instructionIndex < function->instructionCount;
         instructionIndex++) {
        VCShaderInstruction *instruction = &function->instructions[instructionIndex];
        switch (instruction->operation) {
        case VC_SHADER_INSTRUCTION_ADD:
            if (VCShaderCompiler_OperandIsEqualToScalarConstant(function,
                                                                instruction->operands[0],
                                                                0)) {
                instruction->operation = VC_SHADER_INSTRUCTION_MOVE;
                instruction->operands[0] = instruction->operands[1];
                changed = true;
            } else if (VCShaderCompiler_OperandIsEqualToScalarConstant(function,
                                                                       instruction->operands[1],
                                                                       0)) {
                instruction->operation = VC_SHADER_INSTRUCTION_MOVE;
                changed = true;
            }
            break;
        case VC_SHADER_INSTRUCTION_SUB:
            if (VCShaderCompiler_OperandIsEqualToScalarConstant(function,
                                                                instruction->operands[1],
                                                                0)) {
                instruction->operation = VC_SHADER_INSTRUCTION_MOVE;
            } else if (instruction->operands[0] == instruction->operands[1]) {
                instruction->operation = VC_SHADER_INSTRUCTION_MOVE;
                instruction->operands[0] = VCShaderCompiler_GetScalarConstant(function, 0);
                changed = true;
            }
            break;
        case VC_SHADER_INSTRUCTION_MUL:
            if (VCShaderCompiler_OperandIsEqualToScalarConstant(function,
                                                                instruction->operands[0],
                                                                255)) {
                instruction->operation = VC_SHADER_INSTRUCTION_MOVE;
                instruction->operands[0] = instruction->operands[1];
                changed = true;
            } else if (VCShaderCompiler_OperandIsEqualToScalarConstant(function,
                                                                       instruction->operands[1],
                                                                       255)) {
                instruction->operation = VC_SHADER_INSTRUCTION_MOVE;
                changed = true;
            } else if (VCShaderCompiler_OperandIsEqualToScalarConstant(function,
                                                                       instruction->operands[0],
                                                                       0) ||
                       VCShaderCompiler_OperandIsEqualToScalarConstant(function,
                                                                       instruction->operands[1],
                                                                       0)) {
                instruction->operation = VC_SHADER_INSTRUCTION_MOVE;
                instruction->operands[0] = VCShaderCompiler_GetScalarConstant(function, 0);
                changed = true;
            }
            break;
        }
    }
    return changed;
}

// Forward constant propagation.
static bool VCShaderCompiler_PropagateConstants(VCShaderFunction *function) {
    bool changed = false;
    for (size_t instructionIndex = 0;
         instructionIndex < function->instructionCount;
         instructionIndex++) {
        VCShaderInstruction *instruction = &function->instructions[instructionIndex];
        if (instruction->operation != VC_SHADER_INSTRUCTION_MOVE)
            continue;
        if ((instruction->operands[0] & VC_SHADER_COMPILER_CONSTANT_VALUE) == 0)
            continue;
        assert(instruction->destination != instruction->operands[0]);
        for (size_t destinationInstructionIndex = instructionIndex + 1;
             destinationInstructionIndex < function->instructionCount;
             destinationInstructionIndex++) {
            VCShaderInstruction *destinationInstruction =
                &function->instructions[destinationInstructionIndex];
            for (uint8_t operandIndex = 0; operandIndex < 2; operandIndex++) { 
                uint8_t operand = destinationInstruction->operands[operandIndex];
                if (operand != instruction->destination)
                    continue;
                destinationInstruction->operands[operandIndex] = instruction->operands[0];
                changed = true;
            }
        }
    }
    return changed;
}

// Backward value propagation.
static bool VCShaderCompiler_PropagateValues(VCShaderFunction *function) {
    bool changed = false;
    size_t instructionIndex = function->instructionCount;
    while (instructionIndex > 1) {
        instructionIndex--;
        VCShaderInstruction *instruction = &function->instructions[instructionIndex];
        if (instruction->operation != VC_SHADER_INSTRUCTION_MOVE)
            continue;
        if ((instruction->operands[0] & VC_SHADER_COMPILER_CONSTANT_VALUE) != 0) {
            continue;
        }
        assert(instruction->destination != instruction->operands[0]);
        size_t sourceInstructionIndex = instructionIndex;
        while (true) {
            assert(sourceInstructionIndex != 0);
            sourceInstructionIndex--;
            VCShaderInstruction *sourceInstruction =
                &function->instructions[sourceInstructionIndex];
            if (sourceInstruction->destination != instruction->operands[0])
                continue;
            instruction->operation = sourceInstruction->operation;
            instruction->operands[0] = sourceInstruction->operands[0];
            instruction->operands[1] = sourceInstruction->operands[1];
            changed = true;
            break;
        }
    }
    return changed;
}

static size_t VCShaderCompiler_CountRegistersUsedInFunction(VCShaderFunction *function) {
    size_t maxRegisterNumber = 0;
    for (size_t instructionIndex = 0;
         instructionIndex < function->instructionCount;
         instructionIndex++) {
        VCShaderInstruction *instruction = &function->instructions[instructionIndex];
        if ((instruction->destination & VC_SHADER_COMPILER_TYPE_MASK) ==
                VC_SHADER_COMPILER_REGISTER_VALUE) {
            uint8_t registerNumber = (instruction->destination & ~VC_SHADER_COMPILER_TYPE_MASK);
            if (maxRegisterNumber < (uint8_t)(registerNumber + 1)) {
                maxRegisterNumber = (uint8_t)(registerNumber + 1);
            }
        }
    }
    return maxRegisterNumber;
}

static bool VCShaderCompiler_EliminateDeadCode(VCShaderFunction *function) {
    size_t registerCount = VCShaderCompiler_CountRegistersUsedInFunction(function);
    bool *usedRegisters = (bool *)malloc(sizeof(bool) * registerCount);
    for (size_t registerIndex = 0; registerIndex < registerCount; registerIndex++)
        usedRegisters[registerIndex] = 0;

    for (size_t instructionIndex = 0;
         instructionIndex < function->instructionCount;
         instructionIndex++) {
        VCShaderInstruction *instruction = &function->instructions[instructionIndex];
        if ((instruction->operands[0] & VC_SHADER_COMPILER_CONSTANT_VALUE) == 0)
            usedRegisters[instruction->operands[0]] = true;
        if ((instruction->operands[1] & VC_SHADER_COMPILER_CONSTANT_VALUE) == 0)
            usedRegisters[instruction->operands[1]] = true;
    }

    size_t destinationInstructionIndex = 0;
    for (size_t instructionIndex = 0;
         instructionIndex < function->instructionCount;
         instructionIndex++) {
        VCShaderInstruction *instruction = &function->instructions[instructionIndex];
        if ((instruction->destination & VC_SHADER_COMPILER_CONSTANT_VALUE) == 0 &&
                !usedRegisters[instruction->destination]) {
            // Zap this instruction.
            continue;
        }
        function->instructions[destinationInstructionIndex] =
            function->instructions[instructionIndex];
        destinationInstructionIndex++;
    }

    free(usedRegisters);

    bool changed = function->instructionCount != destinationInstructionIndex;
    if (changed)
        function->instructionCount = destinationInstructionIndex;
    return changed;
}

static void VCShaderCompiler_MergeConstants(VCShaderFunction *function) {
    VCShaderConstantMergePoolEntry *pool = NULL;
    size_t newConstantCount = 0;
    uint8_t *constantIndexMapping = (uint8_t *)malloc(sizeof(uint8_t) * function->constantCount);
    memset(constantIndexMapping, 0xff, sizeof(uint8_t) * function->constantCount);

    for (size_t instructionIndex = 0;
         instructionIndex < function->instructionCount;
         instructionIndex++) {
        VCShaderInstruction *instruction = &function->instructions[instructionIndex];
        for (uint8_t operandIndex = 0; operandIndex < 2; operandIndex++) {
            uint8_t operand = instruction->operands[operandIndex];
            if ((operand & VC_SHADER_COMPILER_CONSTANT_VALUE) == 0)
                continue;
            if (operand >= VC_SHADER_COMPILER_LEAST_SPECIAL_VALUE)
                continue;
            uint8_t constantIndex = (operand & ~VC_SHADER_COMPILER_CONSTANT_VALUE);
            VCColor constant = function->constants[constantIndex];
            VCShaderConstantMergePoolEntry *entry = NULL;
            HASH_FIND(hh, pool, &constant, sizeof(VCColor), entry);
            if (entry != NULL) {
                constantIndexMapping[constantIndex] = entry->newIndex;
                continue;
            }

            entry = (VCShaderConstantMergePoolEntry *)
                malloc(sizeof(VCShaderConstantMergePoolEntry));
            entry->constant = constant;
            entry->oldIndex = constantIndex;
            entry->newIndex = newConstantCount;
            memset(&entry->hh, '\0', sizeof(entry->hh));
            HASH_ADD(hh, pool, constant, sizeof(VCColor), entry);
            constantIndexMapping[constantIndex] = entry->newIndex;
            newConstantCount++;
        }
    }

    assert(newConstantCount <= function->constantCount);
    if (newConstantCount == function->constantCount)
        return;

    VCShaderConstantMergePoolEntry *entry = NULL, *tempEntry = NULL;
    HASH_ITER(hh, pool, entry, tempEntry) {
        free(entry);
    }

    for (size_t instructionIndex = 0;
         instructionIndex < function->instructionCount;
         instructionIndex++) {
        VCShaderInstruction *instruction = &function->instructions[instructionIndex];
        for (uint8_t operandIndex = 0; operandIndex < 2; operandIndex++) {
            uint8_t operand = instruction->operands[operandIndex];
            if ((operand & VC_SHADER_COMPILER_CONSTANT_VALUE) == 0)
                continue;
            if (operand >= VC_SHADER_COMPILER_LEAST_SPECIAL_VALUE)
                continue;
            uint8_t oldConstantIndex = (operand & ~VC_SHADER_COMPILER_CONSTANT_VALUE);
            uint8_t newConstantIndex = constantIndexMapping[oldConstantIndex];
            assert(newConstantIndex != 0xff);
            instruction->operands[operandIndex] =
                (newConstantIndex | VC_SHADER_COMPILER_CONSTANT_VALUE);
        }
    }

    free(constantIndexMapping);
    function->constantCount = newConstantCount;
}

static void VCShaderCompiler_RenumberRegisters(VCShaderFunction *function) {
    size_t registerCount = VCShaderCompiler_CountRegistersUsedInFunction(function);
    uint8_t *newRegisterMapping = (uint8_t *)malloc(sizeof(uint8_t) * registerCount);
    memset(newRegisterMapping, 0xff, sizeof(uint8_t) * registerCount);

    uint8_t newRegisterCount = 0;
    for (size_t instructionIndex = 0;
         instructionIndex < function->instructionCount;
         instructionIndex++) {
        VCShaderInstruction *instruction = &function->instructions[instructionIndex];
        for (uint8_t operandIndex = 0; operandIndex < 2; operandIndex++) {
            uint8_t operand = instruction->operands[operandIndex];
            if ((operand & VC_SHADER_COMPILER_CONSTANT_VALUE) != 0)
                continue;
            uint8_t newOperand = newRegisterMapping[operand];
            assert(newOperand != 0xff);
            instruction->operands[operandIndex] = newOperand;
        }
        if ((instruction->destination & VC_SHADER_COMPILER_CONSTANT_VALUE) != 0)
            continue;
        newRegisterMapping[instruction->destination] = newRegisterCount;
        instruction->destination = newRegisterCount;
        newRegisterCount++;
    }

    free(newRegisterMapping);
}

static void VCShaderCompiler_OptimizeFunction(VCShaderFunction *function) {
    bool changed = true;
    while (changed) {
        changed = false;
        changed = VCShaderCompiler_CombineInstructions(function);
        changed = VCShaderCompiler_PropagateValues(function) || changed;
        changed = VCShaderCompiler_PropagateConstants(function) || changed;
        changed = VCShaderCompiler_EliminateDeadCode(function) || changed;
    }

    VCShaderCompiler_RenumberRegisters(function);
    VCShaderCompiler_MergeConstants(function);
}

static VCShaderFunction VCShaderCompiler_GenerateFunction(VCShaderSubprogramContext *context,
                                                          VCUnpackedCombinerFunction *cycle0,
                                                          VCUnpackedCombinerFunction *cycle1) {
    VCShaderFunction function;
    function.instructions = (VCShaderInstruction *)malloc(sizeof(VCShaderInstruction) *
                                                           VC_SHADER_MAX_INSTRUCTIONS);
    function.instructionCount = 0;
    function.constants = (VCColor *)malloc(sizeof(VCColor) * VC_SHADER_MAX_CONSTANTS);
    function.constantCount = 0;

    uint8_t output = VCShaderCompiler_GenerateInstructionsForCycle(context,
                                                                   &function,
                                                                   cycle0,
                                                                   0,
                                                                   false);
    if (cycle1 != NULL) {
        output = VCShaderCompiler_GenerateInstructionsForCycle(context,
                                                               &function,
                                                               cycle1,
                                                               output + 1,
                                                               true); 
    }

    VCShaderInstruction *outputInstruction = VCShaderCompiler_AllocateInstruction(&function);
    outputInstruction->operation = VC_SHADER_INSTRUCTION_MOVE;
    outputInstruction->destination = VC_SHADER_COMPILER_OUTPUT_VALUE;
    outputInstruction->operands[0] = output;
    outputInstruction->operands[1] = VCShaderCompiler_GetScalarConstant(&function, 0);

    return function;
}

static VCShaderSubprogram *VCShaderCompiler_CreateSubprogram(VCShaderSubprogramSource *source) {
    VCShaderSubprogram *subprogram = (VCShaderSubprogram *)malloc(sizeof(VCShaderSubprogram));
    subprogram->source = *source;

    VCUnpackedCombinerFunction cycle0Function;
    cycle0Function.sa = source->cycle0.saRGB;
    cycle0Function.sb = source->cycle0.sbRGB;
    cycle0Function.m = source->cycle0.mRGB;
    cycle0Function.a = source->cycle0.aRGB;
    if (!source->context.secondCycleEnabled) {
        subprogram->rgb = VCShaderCompiler_GenerateFunction(&source->context,
                                                            &cycle0Function,
                                                            NULL);
    } else {
        VCUnpackedCombinerFunction cycle1Function;
        cycle1Function.sa = source->cycle1.saRGB;
        cycle1Function.sb = source->cycle1.sbRGB;
        cycle1Function.m = source->cycle1.mRGB;
        cycle1Function.a = source->cycle1.aRGB;
        subprogram->rgb = VCShaderCompiler_GenerateFunction(&source->context,
                                                            &cycle0Function,
                                                            &cycle1Function);
    }

    cycle0Function.sa = source->cycle0.saA;
    cycle0Function.sb = source->cycle0.sbA;
    cycle0Function.m = source->cycle0.mA;
    cycle0Function.a = source->cycle0.aA;
    if (!source->context.secondCycleEnabled) {
        subprogram->a = VCShaderCompiler_GenerateFunction(&source->context,
                                                          &cycle0Function,
                                                          NULL);
    } else {
        VCUnpackedCombinerFunction cycle1Function;
        cycle1Function.sa = source->cycle1.saA;
        cycle1Function.sb = source->cycle1.sbA;
        cycle1Function.m = source->cycle1.mA;
        cycle1Function.a = source->cycle1.aA;
        subprogram->a = VCShaderCompiler_GenerateFunction(&source->context,
                                                          &cycle0Function,
                                                          &cycle1Function);
    }

    VCShaderCompiler_OptimizeFunction(&subprogram->rgb);
    VCShaderCompiler_OptimizeFunction(&subprogram->a);

    return subprogram;
}

static void VCShaderCompiler_GenerateGLSLForValue(VCString *shaderSource,
                                                  VCShaderFunction *function,
                                                  const char *outputLocation,
                                                  uint8_t value) {
    const char *staticString = NULL;
    switch (value) {
    case VC_SHADER_COMPILER_TEXEL0_VALUE:
        staticString = "texture0Color";
        break;
    case VC_SHADER_COMPILER_TEXEL0_ALPHA_VALUE:
        staticString = "texture0Color.aaaa";
        break;
    case VC_SHADER_COMPILER_TEXEL1_VALUE:
        staticString = "texture1Color";
        break;
    case VC_SHADER_COMPILER_TEXEL1_ALPHA_VALUE:
        staticString = "texture1Color.aaaa";
        break;
    case VC_SHADER_COMPILER_SHADE_VALUE:
        staticString = "vShade";
        break;
    case VC_SHADER_COMPILER_OUTPUT_VALUE:
        staticString = outputLocation;
        break;
    }

    if (staticString != NULL) {
        VCString_AppendCString(shaderSource, staticString);
        return;
    }

    if ((value & VC_SHADER_COMPILER_CONSTANT_VALUE) != 0) {
        uint8_t constantIndex = (value & ~VC_SHADER_COMPILER_CONSTANT_VALUE);
        assert(constantIndex < function->constantCount);
        VCColor *constant = &function->constants[constantIndex];
        VCString_AppendFormat(shaderSource,
                              "vec4(%f, %f, %f, %f)", 
                              (float)constant->r / 255.0,
                              (float)constant->g / 255.0,
                              (float)constant->b / 255.0,
                              (float)constant->a / 255.0);
        return;
    }

    VCString_AppendFormat(shaderSource, "r%d", (int)value);
}

static void VCShaderCompiler_GenerateGLSLForFunction(VCString *shaderSource,
                                                     VCShaderFunction *function,
                                                     const char *outputLocation) {
    for (size_t instructionIndex = 0;
         instructionIndex < function->instructionCount;
         instructionIndex++) {
        VCShaderInstruction *instruction = &function->instructions[instructionIndex];
        VCString_AppendCString(shaderSource, "        ");
        VCShaderCompiler_GenerateGLSLForValue(shaderSource,
                                              function,
                                              outputLocation,
                                              instruction->destination);
        VCString_AppendCString(shaderSource, " = ");
        VCShaderCompiler_GenerateGLSLForValue(shaderSource,
                                              function,
                                              outputLocation,
                                              instruction->operands[0]);
        if (instruction->operation != VC_SHADER_INSTRUCTION_MOVE) {
            char glslOperator;
            switch (instruction->operation) {
            case VC_SHADER_INSTRUCTION_ADD:
                glslOperator = '+';
                break;
            case VC_SHADER_INSTRUCTION_SUB:
                glslOperator = '-';
                break;
            case VC_SHADER_INSTRUCTION_MUL:
                glslOperator = '*';
                break;
            default:
                fprintf(stderr, "Unexpected operation in shader compilation!");
                abort();
            }
            VCString_AppendFormat(shaderSource, " %c ", glslOperator);
            VCShaderCompiler_GenerateGLSLForValue(shaderSource,
                                                  function,
                                                  outputLocation,
                                                  instruction->operands[1]);
        }
        VCString_AppendCString(shaderSource, ";\n");
    }
}

static size_t VCShaderCompiler_CountRegistersUsedInProgram(VCShaderProgram *program) {
    size_t maxRegisterCount = 0;
    for (size_t subprogramIndex = 0;
         subprogramIndex < program->subprogramCount;
         subprogramIndex++) {
        VCShaderSubprogram *subprogram = program->subprograms[subprogramIndex];
        size_t registerCount = VCShaderCompiler_CountRegistersUsedInFunction(&subprogram->rgb);
        if (maxRegisterCount < registerCount)
            maxRegisterCount = registerCount;
        registerCount = VCShaderCompiler_CountRegistersUsedInFunction(&subprogram->a);
        if (maxRegisterCount < registerCount)
            maxRegisterCount = registerCount;
    }
    return maxRegisterCount;
}

void VCShaderCompiler_GenerateGLSLFragmentShaderForProgram(VCString *shaderSource,
                                                           VCShaderProgram *program) {
    assert(program->subprogramCount > 0);
    size_t registerCount = VCShaderCompiler_CountRegistersUsedInProgram(program);
    VCString_AppendCString(shaderSource, "void main(void) {\n");
    VCString_AppendCString(
            shaderSource,
            "    vec4 texture0Color = texture2D(uTexture0, AtlasUv(vTexture0Bounds));\n");
    VCString_AppendCString(
            shaderSource,
            "    vec4 texture1Color = texture2D(uTexture1, AtlasUv(vTexture1Bounds));\n");
    VCString_AppendCString(shaderSource, "    vec4 fragRGB;\n");
    VCString_AppendCString(shaderSource, "    vec4 fragA;\n");
    for (size_t i = 0; i < registerCount; i++)
        VCString_AppendFormat(shaderSource, "    vec4 r%d;\n", (int)i);
    for (size_t subprogramIndex = 0;
         subprogramIndex < program->subprogramCount;
         subprogramIndex++) {
        VCShaderSubprogram *subprogram = program->subprograms[subprogramIndex];
        if (subprogramIndex == 0) {
            VCString_AppendCString(shaderSource, "    if (vSubprogram == 0.0) {\n");
        } else {
            VCString_AppendFormat(shaderSource,
                                  "    } else if (vSubprogram == %d.0) {\n",
                                  (int)subprogramIndex);
        }
        VCShaderCompiler_GenerateGLSLForFunction(shaderSource, &subprogram->rgb, "fragRGB");
        VCShaderCompiler_GenerateGLSLForFunction(shaderSource, &subprogram->a, "fragA");
    }
    VCString_AppendCString(shaderSource, "    }\n");
    VCString_AppendCString(shaderSource, "    gl_FragColor = vec4(fragRGB.rgb, fragA.a);\n");
    VCString_AppendCString(shaderSource, "}\n");
}

VCShaderSubprogramSignatureTable VCShaderCompiler_CreateSubprogramSignatureTable() {
    VCShaderSubprogramSignatureTable table = { NULL };
    return table;
}

void VCShaderCompiler_DestroySubprogramSignatureTable(VCShaderSubprogramSignatureTable *list) {
    VCShaderSubprogramSignatureTableEntry *entry = NULL, *tempEntry = NULL;
    HASH_ITER(hh, list->entries, entry, tempEntry) {
        HASH_DEL(list->entries, entry);
        free(entry);
    }
}

static void VCShaderCompiler_AppendFunctionToSignature(VCString *signatureData,
                                                       VCShaderFunction *function) {
    VCString_AppendBytes(signatureData,
                         function->instructions,
                         function->instructionCount * sizeof(VCShaderInstruction));
    VCString_AppendBytes(signatureData,
                         function->constants,
                         function->constantCount * sizeof(VCColor));
}

static VCShaderSubprogramSignature *VCShaderCompiler_CreateSignatureForSubprogram(
        VCShaderSubprogram *subprogram) {
    VCShaderSubprogramSignature *signature = (VCShaderSubprogramSignature *)
        malloc(sizeof(VCShaderSubprogramSignature));
    signature->source = subprogram->source;
    memset(&signature->hh, '\0', sizeof(signature->hh));

    signature->serializedData = VCString_Create();
    VCString_AppendBytes(&signature->serializedData,
                         &subprogram->source,
                         sizeof(subprogram->source));
    VCShaderCompiler_AppendFunctionToSignature(&signature->serializedData, &subprogram->rgb);
    VCShaderCompiler_AppendFunctionToSignature(&signature->serializedData, &subprogram->a);
    return signature;
}

VCShaderSubprogramContext VCShaderCompiler_CreateSubprogramContext(VCColor primColor,
                                                                   VCColor envColor,
                                                                   bool secondCycleEnabled) {
    VCShaderSubprogramContext subprogramContext;
    subprogramContext.primColor = primColor;
    subprogramContext.envColor = envColor;
    subprogramContext.secondCycleEnabled = true;
    return subprogramContext;
}

static size_t VCShaderCompiler_GetSubprogramSignatureCount(
        VCShaderSubprogramSignatureTable *table) {
    return HASH_COUNT(table->entries);
}

uint16_t VCShaderCompiler_GetOrCreateSubprogramID(VCShaderSubprogramSignatureTable *table,
                                                  VCShaderSubprogramSignature *signature) {
    VCShaderSubprogramSignatureTableEntry *entry = NULL;
    HASH_FIND(hh, table->entries, signature, sizeof(VCShaderSubprogramSignature), entry);
    if (entry != NULL)
        return entry->id;

    entry = (VCShaderSubprogramSignatureTableEntry *)
        malloc(sizeof(VCShaderSubprogramSignatureTableEntry));
    entry->signature = *signature;
    size_t id = VCShaderCompiler_GetSubprogramSignatureCount(table);
    assert(id <= UINT16_MAX);
    entry->id = (uint16_t)id;
    memset(&entry->hh, '\0', sizeof(entry->hh));
    HASH_ADD(hh, table->entries, signature, sizeof(VCShaderSubprogramSignature), entry);
    return entry->id;
}

static VCShaderSubprogram *VCShaderCompiler_GetSubprogramForSignature(
        VCShaderSubprogramLibrary *library,
        VCShaderSubprogramSignature *signature) {
    VCShaderSubprogramEntry *subprogramEntry = NULL;
    HASH_FIND(hh,
              library->subprograms,
              signature->serializedData.ptr,
              signature->serializedData.len,
              subprogramEntry);
    assert(subprogramEntry != NULL);
    return subprogramEntry->subprogram;
}

VCShaderProgram *VCShaderCompiler_GetOrCreateProgram(
        VCShaderSubprogramLibrary *subprogramLibrary,
        VCShaderProgramDescriptor *programDescriptor) {
    if (programDescriptor->program != NULL)
        return programDescriptor->program;

    VCShaderProgram *program = (VCShaderProgram *)malloc(sizeof(VCShaderProgram));
    program->subprogramCount = programDescriptor->subprogramSignatures.length;
    program->subprograms = (VCShaderSubprogram **)
        malloc(sizeof(VCShaderSubprogram *) * program->subprogramCount);

    for (size_t subprogramID = 0; subprogramID < program->subprogramCount; subprogramID++) {
        VCShaderSubprogramSignatureListEntry *entry =
            &programDescriptor->subprogramSignatures.entries[subprogramID];
        assert(subprogramID < program->subprogramCount);
        program->subprograms[subprogramID] =
            VCShaderCompiler_GetSubprogramForSignature(subprogramLibrary, &entry->signature);
    }

    programDescriptor->program = program;
    return program;
}

VCShaderProgramDescriptor *VCShaderCompiler_GetShaderProgramDescriptorByID(
        VCShaderProgramDescriptorLibrary *library,
        uint16_t id) {
    assert(id < library->shaderProgramDescriptorCount);
    VCShaderProgramDescriptor *descriptor = NULL, *tempDescriptor = NULL;
    HASH_ITER(hh, library->shaderProgramDescriptors, descriptor, tempDescriptor) {
        if (descriptor->id == id)
            return descriptor;
    }
    assert(0 && "Didn't find shader program descriptor with that ID!");
    abort(); 
}

VCShaderSubprogramSignatureList VCShaderCompiler_ConvertSubprogramSignatureTableToList(
        VCShaderSubprogramSignatureTable *table) {
    VCShaderSubprogramSignatureList list = { NULL, 0 };
    list.length = HASH_COUNT(table->entries);
    list.entries = (VCShaderSubprogramSignatureListEntry *)
        malloc(sizeof(VCShaderSubprogramSignatureListEntry) * list.length);
    VCShaderSubprogramSignatureTableEntry *entry = NULL, *tempEntry = NULL;
    HASH_ITER(hh, table->entries, entry, tempEntry) {
        assert(entry->id < list.length);
        list.entries[entry->id].signature = entry->signature;
    }
    return list;
}

void VCShaderCompiler_DestroySubprogramSignatureList(VCShaderSubprogramSignatureList *list) {
    free(list->entries);
    list->entries = NULL;
    list->length = 0;
}

VCShaderSubprogramLibrary *VCShaderCompiler_CreateSubprogramLibrary() {
    VCShaderSubprogramLibrary *library = (VCShaderSubprogramLibrary *)
        malloc(sizeof(VCShaderSubprogramLibrary));
    library->signatures = NULL;
    library->subprograms = NULL;
    return library;
}

static VCShaderSubprogramSignature VCShaderCompiler_DuplicateSignature(
        VCShaderSubprogramSignature *signature) {
    VCShaderSubprogramSignature newSignature;
    newSignature.source = signature->source;
    newSignature.serializedData = VCString_Duplicate(&signature->serializedData);
    memset(&newSignature.hh, '\0', sizeof(newSignature.hh));
    return newSignature;
}

VCShaderSubprogramSignature VCShaderCompiler_GetOrCreateSubprogramSignatureForCurrentCombiner(
        VCShaderSubprogramLibrary *library,
        VCShaderSubprogramContext *context) {
    VCShaderSubprogramSource source;
    VCCombiner_UnpackCurrentRGBCombiner(&source.cycle0, &source.cycle1);
    VCCombiner_UnpackCurrentACombiner(&source.cycle0, &source.cycle1);
    source.context = *context;

    VCShaderSubprogramSignature *signature = NULL;
    HASH_FIND(hh, library->signatures, &source, sizeof(VCShaderSubprogramSource), signature);
    if (signature != NULL)
        return *signature;

    VCShaderSubprogram *subprogram = VCShaderCompiler_CreateSubprogram(&source);
    signature = VCShaderCompiler_CreateSignatureForSubprogram(subprogram);
    HASH_ADD(hh, library->signatures, source, sizeof(VCShaderSubprogramSource), signature);

    VCShaderSubprogramEntry *entry = (VCShaderSubprogramEntry *)
        malloc(sizeof(VCShaderSubprogramEntry));
    entry->signature = VCShaderCompiler_DuplicateSignature(signature);
    entry->subprogram = subprogram;
    HASH_ADD_KEYPTR(hh, 
                    library->subprograms,
                    signature->serializedData.ptr,
                    signature->serializedData.len, 
                    entry);

    return *signature;
}

void VCShaderCompiler_DestroySubprogramSignature(VCShaderSubprogramSignature *signature) {
    VCString_Destroy(&signature->serializedData);
    memset(signature, '\0', sizeof(*signature));
}

