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

#define VC_SHADER_COMPILER_REGISTER_VALUE           0x00
#define VC_SHADER_COMPILER_LEAST_SPECIAL_VALUE      0xf3
#define VC_SHADER_COMPILER_ZERO_VALUE               0xf3
#define VC_SHADER_COMPILER_ONE_VALUE                0xf4
#define VC_SHADER_COMPILER_PRIMITIVE_VALUE          0xf5
#define VC_SHADER_COMPILER_PRIMITIVE_ALPHA_VALUE    0xf6
#define VC_SHADER_COMPILER_ENVIRONMENT_VALUE        0xf7
#define VC_SHADER_COMPILER_ENVIRONMENT_ALPHA_VALUE  0xf8
#define VC_SHADER_COMPILER_TEXEL0_VALUE             0xf9
#define VC_SHADER_COMPILER_TEXEL0_ALPHA_VALUE       0xfa
#define VC_SHADER_COMPILER_TEXEL1_VALUE             0xfb
#define VC_SHADER_COMPILER_TEXEL1_ALPHA_VALUE       0xfc
#define VC_SHADER_COMPILER_SHADE_VALUE              0xfd
#define VC_SHADER_COMPILER_SHADE_ALPHA_VALUE        0xfe
#define VC_SHADER_COMPILER_OUTPUT_VALUE             0xff

#define VC_SHADER_COMPILER_MODE_RGB     0
#define VC_SHADER_COMPILER_MODE_ALPHA   1

#define VC_SHADER_COMPILER_MAX_PROGRAMS         64

// SSA form instruction.
struct VCShaderInstruction {
    uint8_t operation;
    uint8_t destination;
    uint8_t operands[2];
};

struct VCShaderFunction {
    VCShaderInstruction *instructions;
    size_t instructionCount;
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

#if 0
static void VCShaderCompiler_DestroyShaderSubprogramSignature(
        VCShaderSubprogramSignature *signature) {
    VCString_Destroy(&signature->serializedData);
    memset(&signature->source, '\0', sizeof(VCShaderSubprogramSource));
}
#endif

// Does not destroy subprogram signatures, since they are weakly referenced by subprogram signature
// lists.
static void VCShaderCompiler_DestroyShaderSubprogramSignatureList(
        VCShaderSubprogramSignatureList *list) {
    free(list->entries);
    list->entries = NULL;
}

#if 0
static void VCShaderCompiler_DestroyShaderFunction(VCShaderFunction *function) {
    free(function->instructions);
    function->instructions = NULL;
    function->instructionCount = 0;
}

static void VCShaderCompiler_DestroyShaderSubprogram(VCShaderSubprogram *subprogram) {
    memset(&subprogram->source, '\0', sizeof(VCShaderSubprogramSource));
    VCShaderCompiler_DestroyShaderFunction(&subprogram->rgb);
    VCShaderCompiler_DestroyShaderFunction(&subprogram->a);
}

static void VCShaderCompiler_DestroyShaderProgram(VCShaderProgram *program) {
    // Does not destroy subprograms since programs only hold weak references to them.
    free(program->subprograms);
    program->subprograms = NULL;
}
#endif

static void VCShaderCompiler_DestroyShaderProgramDescriptor(
        VCShaderProgramDescriptor *descriptor) {
    // FIXME(tachi): This does not destroy programs, so they leak. We should really duplicate them
    // over to the renderer thread.
    VCShaderCompiler_DestroyShaderSubprogramSignatureList(&descriptor->subprogramSignatures);
    VCShaderCompiler_DestroyShaderProgramSignature(&descriptor->programSignature);
    descriptor->program = NULL;
    descriptor->id = 0;
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
        // Reinsert the program into the hash to maintain the LRU property.
        HASH_DEL(library->shaderProgramDescriptors, programDescriptor);
        HASH_ADD_KEYPTR(hh,
                        library->shaderProgramDescriptors,
                        programDescriptor->programSignature.serializedData.ptr,
                        programDescriptor->programSignature.serializedData.len,
                        programDescriptor);
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

bool VCShaderCompiler_ExpireOldProgramIfNecessary(VCShaderProgramDescriptorLibrary *library,
                                                  uint16_t *shaderProgramIDToDelete) {
    size_t programCount = HASH_COUNT(library->shaderProgramDescriptors);
    if (programCount <= VC_SHADER_COMPILER_MAX_PROGRAMS)
        return false;

    assert(programCount > 0);
    VCShaderProgramDescriptor *programDescriptorToDelete = library->shaderProgramDescriptors;
    HASH_DEL(library->shaderProgramDescriptors, programDescriptorToDelete);
    *shaderProgramIDToDelete = programDescriptorToDelete->id;
    VCShaderCompiler_DestroyShaderProgramDescriptor(programDescriptorToDelete);
    free(programDescriptorToDelete);
    return true;
}

static uint8_t VCShaderCompiler_SpecialRGBValueOrConstant(VCColor *color, uint8_t specialValue) {
    if (color->r == 0 && color->g == 0 && color->b == 0)
        return VC_SHADER_COMPILER_ZERO_VALUE;
    if (color->r == 255 && color->g == 255 && color->b == 255)
        return VC_SHADER_COMPILER_ONE_VALUE;
    return specialValue;
}

static uint8_t VCShaderCompiler_SpecialAlphaValueOrConstant(VCColor *color, uint8_t specialValue) {
    if (color->a == 0)
        return VC_SHADER_COMPILER_ZERO_VALUE;
    if (color->a == 255)
        return VC_SHADER_COMPILER_ONE_VALUE;
    return specialValue;
}

static uint8_t VCShaderCompiler_GetRGBValue(VCShaderSubprogramContext *context,
                                            VCShaderFunction *function,
                                            uint8_t source,
                                            bool allowCombined) {
    switch (source) {
    case VC_COMBINER_CCMUX_COMBINED:
        if (allowCombined)
            return VC_SHADER_COMPILER_REGISTER_VALUE | 2;
        return VC_SHADER_COMPILER_ZERO_VALUE;
    case VC_COMBINER_CCMUX_COMBINED_ALPHA:
        // FIXME(tachi): Support VC_COMBINER_CCMUX_COMBINED_ALPHA!
        fprintf(stderr, "FIXME: VC_COMBINER_CCMUX_COMBINED_ALPHA! Expect bad colors.\n");
        return VC_SHADER_COMPILER_ONE_VALUE;
    case VC_COMBINER_CCMUX_TEXEL0:
        return VC_SHADER_COMPILER_TEXEL0_VALUE;
    case VC_COMBINER_CCMUX_TEXEL0_ALPHA:
        return VC_SHADER_COMPILER_TEXEL0_ALPHA_VALUE;
    case VC_COMBINER_CCMUX_TEXEL1:
        return VC_SHADER_COMPILER_TEXEL1_VALUE;
    case VC_COMBINER_CCMUX_TEXEL1_ALPHA:
        return VC_SHADER_COMPILER_TEXEL1_ALPHA_VALUE;
    case VC_COMBINER_CCMUX_SHADE:
        return VC_SHADER_COMPILER_SHADE_VALUE;
    case VC_COMBINER_CCMUX_SHADE_ALPHA:
        return VC_SHADER_COMPILER_SHADE_ALPHA_VALUE;
    case VC_COMBINER_CCMUX_PRIMITIVE:
        return VCShaderCompiler_SpecialRGBValueOrConstant(&context->primColor,
                                                          VC_SHADER_COMPILER_PRIMITIVE_VALUE);
    case VC_COMBINER_CCMUX_ENVIRONMENT:
        return VCShaderCompiler_SpecialRGBValueOrConstant(&context->envColor,
                                                          VC_SHADER_COMPILER_ENVIRONMENT_VALUE);
    case VC_COMBINER_CCMUX_PRIMITIVE_ALPHA:
        return VCShaderCompiler_SpecialAlphaValueOrConstant(
            &context->primColor,
            VC_SHADER_COMPILER_PRIMITIVE_ALPHA_VALUE);
    case VC_COMBINER_CCMUX_ENV_ALPHA:
        return VCShaderCompiler_SpecialAlphaValueOrConstant(
            &context->envColor,
            VC_SHADER_COMPILER_ENVIRONMENT_ALPHA_VALUE);
    case VC_COMBINER_CCMUX_NOISE:
        return VC_SHADER_COMPILER_ONE_VALUE;
    case VC_COMBINER_CCMUX_0:
        return VC_SHADER_COMPILER_ZERO_VALUE;
    case VC_COMBINER_CCMUX_1:
        return VC_SHADER_COMPILER_ONE_VALUE;
    }
    // FIXME(tachi): Support K4, K5!
    return VC_SHADER_COMPILER_ZERO_VALUE;
}

static uint8_t VCShaderCompiler_GetAlphaValue(VCShaderSubprogramContext *context,
                                              VCShaderFunction *function,
                                              uint8_t source,
                                              bool allowCombined) {
    switch (source) {
    case VC_COMBINER_ACMUX_COMBINED:
        if (allowCombined)
            return VC_SHADER_COMPILER_REGISTER_VALUE | 2;
        return VC_SHADER_COMPILER_ZERO_VALUE;
    case VC_COMBINER_ACMUX_TEXEL0:
        return VC_SHADER_COMPILER_TEXEL0_VALUE;
    case VC_COMBINER_ACMUX_TEXEL1:
        return VC_SHADER_COMPILER_TEXEL1_VALUE;
    case VC_COMBINER_ACMUX_SHADE:
        return VC_SHADER_COMPILER_SHADE_VALUE;
    case VC_COMBINER_ACMUX_PRIMITIVE:
        return VCShaderCompiler_SpecialAlphaValueOrConstant(&context->primColor,
                                                            VC_SHADER_COMPILER_PRIMITIVE_VALUE);
    case VC_COMBINER_ACMUX_ENVIRONMENT:
        return VCShaderCompiler_SpecialAlphaValueOrConstant(&context->envColor,
                                                            VC_SHADER_COMPILER_ENVIRONMENT_VALUE);
    case VC_COMBINER_ACMUX_0:
        return VC_SHADER_COMPILER_ZERO_VALUE;
    case VC_COMBINER_ACMUX_1:
        return VC_SHADER_COMPILER_ONE_VALUE;
    }
    // FIXME(tachi): Support G_ACMUX_PRIM_LOD_FRAC!
    return VC_SHADER_COMPILER_ZERO_VALUE;
}

static uint8_t VCShaderCompiler_GetValue(VCShaderSubprogramContext *context,
                                         uint8_t mode,
                                         VCShaderFunction *function,
                                         uint8_t source,
                                         bool allowCombined) {
    switch (mode) {
    case VC_SHADER_COMPILER_MODE_RGB:
        return VCShaderCompiler_GetRGBValue(context, function, source, allowCombined);
    case VC_SHADER_COMPILER_MODE_ALPHA:
        return VCShaderCompiler_GetAlphaValue(context, function, source, allowCombined);
    default:
        assert(0 && "Unknown shader compiler mode!");
        return 0;
    }
}

static VCShaderInstruction *VCShaderCompiler_AllocateInstruction(VCShaderFunction *function) {
    assert(function->instructionCount < VC_SHADER_MAX_INSTRUCTIONS);
    VCShaderInstruction *instruction = &function->instructions[function->instructionCount];
    function->instructionCount++;
    return instruction;
}

static uint8_t VCShaderCompiler_GenerateInstructionsForCycle(VCShaderSubprogramContext *context,
                                                             uint8_t mode,
                                                             VCShaderFunction *function,
                                                             VCUnpackedCombinerFunction *cycle,
                                                             uint8_t firstRegister,
                                                             bool allowCombined) {
    VCShaderInstruction *instruction = VCShaderCompiler_AllocateInstruction(function);
    instruction->operation = VC_SHADER_INSTRUCTION_SUB;
    instruction->destination = firstRegister;
    instruction->operands[0] = VCShaderCompiler_GetValue(context,
                                                         mode,
                                                         function,
                                                         cycle->sa,
                                                         allowCombined);
    instruction->operands[1] = VCShaderCompiler_GetValue(context,
                                                         mode,
                                                         function,
                                                         cycle->sb,
                                                         allowCombined);

    instruction = VCShaderCompiler_AllocateInstruction(function);
    instruction->operation = VC_SHADER_INSTRUCTION_MUL;
    instruction->destination = firstRegister + 1;
    instruction->operands[0] = firstRegister;
    instruction->operands[1] = VCShaderCompiler_GetValue(context,
                                                         mode,
                                                         function,
                                                         cycle->m,
                                                         allowCombined);

    instruction = VCShaderCompiler_AllocateInstruction(function);
    instruction->operation = VC_SHADER_INSTRUCTION_ADD;
    instruction->destination = firstRegister + 2;
    instruction->operands[0] = firstRegister + 1;
    instruction->operands[1] = VCShaderCompiler_GetValue(context,
                                                         mode,
                                                         function,
                                                         cycle->a,
                                                         allowCombined);

    return firstRegister + 2;
}

static bool VCShaderCompiler_CombineInstructions(VCShaderFunction *function) {
    bool changed = false;
    for (size_t instructionIndex = 0;
         instructionIndex < function->instructionCount;
         instructionIndex++) {
        VCShaderInstruction *instruction = &function->instructions[instructionIndex];
        switch (instruction->operation) {
        case VC_SHADER_INSTRUCTION_ADD:
            if (instruction->operands[0] == VC_SHADER_COMPILER_ZERO_VALUE) {
                instruction->operation = VC_SHADER_INSTRUCTION_MOVE;
                instruction->operands[0] = instruction->operands[1];
                instruction->operands[1] = VC_SHADER_COMPILER_ZERO_VALUE;
                changed = true;
            } else if (instruction->operands[1] == VC_SHADER_COMPILER_ZERO_VALUE) {
                instruction->operation = VC_SHADER_INSTRUCTION_MOVE;
                instruction->operands[1] = VC_SHADER_COMPILER_ZERO_VALUE;
                changed = true;
            }
            break;
        case VC_SHADER_INSTRUCTION_SUB:
            if (instruction->operands[1] == VC_SHADER_COMPILER_ZERO_VALUE) {
                instruction->operation = VC_SHADER_INSTRUCTION_MOVE;
                instruction->operands[1] = VC_SHADER_COMPILER_ZERO_VALUE;
                changed = true;
            } else if (instruction->operands[0] == instruction->operands[1]) {
                instruction->operation = VC_SHADER_INSTRUCTION_MOVE;
                instruction->operands[0] = VC_SHADER_COMPILER_ZERO_VALUE;
                instruction->operands[1] = VC_SHADER_COMPILER_ZERO_VALUE;
                changed = true;
            }
            break;
        case VC_SHADER_INSTRUCTION_MUL:
            if (instruction->operands[0] == VC_SHADER_COMPILER_ONE_VALUE) {
                instruction->operation = VC_SHADER_INSTRUCTION_MOVE;
                instruction->operands[0] = instruction->operands[1];
                instruction->operands[1] = VC_SHADER_COMPILER_ZERO_VALUE;
                changed = true;
            } else if (instruction->operands[1] == VC_SHADER_COMPILER_ONE_VALUE) {
                instruction->operation = VC_SHADER_INSTRUCTION_MOVE;
                instruction->operands[1] = VC_SHADER_COMPILER_ZERO_VALUE;
                changed = true;
            } else if (instruction->operands[0] == VC_SHADER_COMPILER_ZERO_VALUE ||
                       instruction->operands[1] == VC_SHADER_COMPILER_ZERO_VALUE) {
                instruction->operation = VC_SHADER_INSTRUCTION_MOVE;
                instruction->operands[0] = VC_SHADER_COMPILER_ZERO_VALUE;
                instruction->operands[1] = VC_SHADER_COMPILER_ZERO_VALUE;
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
        if (instruction->operands[0] < VC_SHADER_COMPILER_LEAST_SPECIAL_VALUE)
            continue;
        assert(instruction->destination != instruction->operands[0]);
        assert(instruction->operands[1] == VC_SHADER_COMPILER_ZERO_VALUE);
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
        if (instruction->operands[0] >= VC_SHADER_COMPILER_LEAST_SPECIAL_VALUE)
            continue;
        assert(instruction->destination != instruction->operands[0]);
        assert(instruction->operands[1] == VC_SHADER_COMPILER_ZERO_VALUE);
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
        if (instruction->destination < VC_SHADER_COMPILER_LEAST_SPECIAL_VALUE) {
            uint8_t registerNumber = instruction->destination;
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
        if (instruction->operands[0] < VC_SHADER_COMPILER_LEAST_SPECIAL_VALUE)
            usedRegisters[instruction->operands[0]] = true;
        if (instruction->operands[1] < VC_SHADER_COMPILER_LEAST_SPECIAL_VALUE)
            usedRegisters[instruction->operands[1]] = true;
    }

    size_t destinationInstructionIndex = 0;
    for (size_t instructionIndex = 0;
         instructionIndex < function->instructionCount;
         instructionIndex++) {
        VCShaderInstruction *instruction = &function->instructions[instructionIndex];
        if (instruction->destination < VC_SHADER_COMPILER_LEAST_SPECIAL_VALUE &&
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
            if (operand >= VC_SHADER_COMPILER_LEAST_SPECIAL_VALUE)
                continue;
            uint8_t newOperand = newRegisterMapping[operand];
            assert(newOperand != 0xff);
            instruction->operands[operandIndex] = newOperand;
        }
        if (instruction->destination >= VC_SHADER_COMPILER_LEAST_SPECIAL_VALUE)
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
}

static VCShaderFunction VCShaderCompiler_GenerateFunction(VCShaderSubprogramContext *context,
                                                          uint8_t mode,
                                                          VCUnpackedCombinerFunction *cycle0,
                                                          VCUnpackedCombinerFunction *cycle1) {
    VCShaderFunction function;
    function.instructions = (VCShaderInstruction *)malloc(sizeof(VCShaderInstruction) *
                                                           VC_SHADER_MAX_INSTRUCTIONS);
    function.instructionCount = 0;

    uint8_t output = VCShaderCompiler_GenerateInstructionsForCycle(context,
                                                                   mode,
                                                                   &function,
                                                                   cycle0,
                                                                   0,
                                                                   false);
    if (cycle1 != NULL) {
        output = VCShaderCompiler_GenerateInstructionsForCycle(context,
                                                               mode,
                                                               &function,
                                                               cycle1,
                                                               output + 1,
                                                               true); 
    }

    VCShaderInstruction *outputInstruction = VCShaderCompiler_AllocateInstruction(&function);
    outputInstruction->operation = VC_SHADER_INSTRUCTION_MOVE;
    outputInstruction->destination = VC_SHADER_COMPILER_OUTPUT_VALUE;
    outputInstruction->operands[0] = output;
    outputInstruction->operands[1] = VC_SHADER_COMPILER_ZERO_VALUE;

    return function;
}

static void VCShaderCompiler_CreateSubprogramForNormalTriangleMode(
        VCShaderSubprogramSource *source,
        VCShaderSubprogram *subprogram) {
    VCUnpackedCombinerFunction cycle0Function;
    cycle0Function.sa = source->cycle0.saRGB;
    cycle0Function.sb = source->cycle0.sbRGB;
    cycle0Function.m = source->cycle0.mRGB;
    cycle0Function.a = source->cycle0.aRGB;
    if (!source->context.secondCycleEnabled) {
        subprogram->rgb = VCShaderCompiler_GenerateFunction(&source->context,
                                                            VC_SHADER_COMPILER_MODE_RGB,
                                                            &cycle0Function,
                                                            NULL);
    } else {
        VCUnpackedCombinerFunction cycle1Function;
        cycle1Function.sa = source->cycle1.saRGB;
        cycle1Function.sb = source->cycle1.sbRGB;
        cycle1Function.m = source->cycle1.mRGB;
        cycle1Function.a = source->cycle1.aRGB;
        subprogram->rgb = VCShaderCompiler_GenerateFunction(&source->context,
                                                            VC_SHADER_COMPILER_MODE_RGB,
                                                            &cycle0Function,
                                                            &cycle1Function);
    }

    cycle0Function.sa = source->cycle0.saA;
    cycle0Function.sb = source->cycle0.sbA;
    cycle0Function.m = source->cycle0.mA;
    cycle0Function.a = source->cycle0.aA;
    if (!source->context.secondCycleEnabled) {
        subprogram->a = VCShaderCompiler_GenerateFunction(&source->context,
                                                          VC_SHADER_COMPILER_MODE_ALPHA,
                                                          &cycle0Function,
                                                          NULL);
    } else {
        VCUnpackedCombinerFunction cycle1Function;
        cycle1Function.sa = source->cycle1.saA;
        cycle1Function.sb = source->cycle1.sbA;
        cycle1Function.m = source->cycle1.mA;
        cycle1Function.a = source->cycle1.aA;
        subprogram->a = VCShaderCompiler_GenerateFunction(&source->context,
                                                          VC_SHADER_COMPILER_MODE_ALPHA,
                                                          &cycle0Function,
                                                          &cycle1Function);
    }
}

static void VCShaderCompiler_CreateSubprogramForRectFillTriangleMode(
        VCShaderSubprogramSource *source,
        VCShaderSubprogram *subprogram) {
    VCUnpackedCombinerFunction cycle0Function;
    cycle0Function.sa = VC_COMBINER_CCMUX_SHADE;
    cycle0Function.sb = VC_COMBINER_CCMUX_0;
    cycle0Function.m = VC_COMBINER_CCMUX_1;
    cycle0Function.a = VC_COMBINER_CCMUX_0;
    subprogram->rgb = VCShaderCompiler_GenerateFunction(&source->context,
                                                        VC_SHADER_COMPILER_MODE_RGB,
                                                        &cycle0Function,
                                                        NULL);

    cycle0Function.sa = VC_COMBINER_ACMUX_SHADE;
    cycle0Function.sb = VC_COMBINER_ACMUX_0;
    cycle0Function.m = VC_COMBINER_ACMUX_1;
    cycle0Function.a = VC_COMBINER_ACMUX_0;
    subprogram->a = VCShaderCompiler_GenerateFunction(&source->context,
                                                      VC_SHADER_COMPILER_MODE_ALPHA,
                                                      &cycle0Function,
                                                      NULL);
}

static VCShaderSubprogram *VCShaderCompiler_CreateSubprogram(VCShaderSubprogramSource *source) {
    VCShaderSubprogram *subprogram = (VCShaderSubprogram *)malloc(sizeof(VCShaderSubprogram));
    subprogram->source = *source;

    switch (source->context.triangleMode) {
    case VC_TRIANGLE_MODE_NORMAL:
    case VC_TRIANGLE_MODE_TEXTURE_RECTANGLE:
        VCShaderCompiler_CreateSubprogramForNormalTriangleMode(source, subprogram);
        break;
    case VC_TRIANGLE_MODE_RECT_FILL:
        VCShaderCompiler_CreateSubprogramForRectFillTriangleMode(source, subprogram);
        break;
    default:
        assert(0 && "Unknown triangle mode when creating subprogram!");
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
    case VC_SHADER_COMPILER_ZERO_VALUE:
        staticString = "vec4(0.0)";
        break;
    case VC_SHADER_COMPILER_ONE_VALUE:
        staticString = "vec4(1.0)";
        break;
    case VC_SHADER_COMPILER_PRIMITIVE_VALUE:
        staticString = "vPrimitive";
        break;
    case VC_SHADER_COMPILER_PRIMITIVE_ALPHA_VALUE:
        staticString = "vPrimitive.aaaa";
        break;
    case VC_SHADER_COMPILER_ENVIRONMENT_VALUE:
        staticString = "vEnvironment";
        break;
    case VC_SHADER_COMPILER_ENVIRONMENT_ALPHA_VALUE:
        staticString = "vEnvironment.aaaa";
        break;
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
    case VC_SHADER_COMPILER_SHADE_ALPHA_VALUE:
        staticString = "vShade.aaaa";
        break;
    case VC_SHADER_COMPILER_OUTPUT_VALUE:
        staticString = outputLocation;
        break;
    }

    if (staticString != NULL) {
        VCString_AppendCString(shaderSource, staticString);
        return;
    }

    assert(value < VC_SHADER_COMPILER_LEAST_SPECIAL_VALUE);
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

        // Add a fudge factor of 0.5 in each direction because the VideoCore IV sometimes adds some
        // error (due to perspective correction, presumably) to varyings for polygons close to the
        // camera.
        if (subprogramIndex == 0) {
            VCString_AppendCString(shaderSource, "    if (vControl.x < 0.5) {\n");
        } else {
            VCString_AppendFormat(shaderSource, "    } else if (vControl.x < %d.5) {\n",
                                  (int)subprogramIndex);
        }
        VCShaderCompiler_GenerateGLSLForFunction(shaderSource, &subprogram->rgb, "fragRGB");
        VCShaderCompiler_GenerateGLSLForFunction(shaderSource, &subprogram->a, "fragA");
    }
    VCString_AppendCString(shaderSource, "    } else {\n");
    VCString_AppendCString(shaderSource, "        fragRGB = vec4(1.0, 0.0, 0.0, 1.0);\n");
    VCString_AppendCString(shaderSource, "        fragA = vec4(1.0);\n");
    VCString_AppendCString(shaderSource, "    }\n");
    VCString_AppendCString(shaderSource, "    if (fragA.a * 255.0 < vControl.y)\n");
    VCString_AppendCString(shaderSource, "        discard;\n");
    VCString_AppendCString(shaderSource, "    if (vControl.z < 0.5)\n");
    VCString_AppendCString(shaderSource, "        fragA.a = 1.0;\n");
    VCString_AppendCString(shaderSource, "    else if (vControl.z < 1.5)\n");
    VCString_AppendCString(shaderSource, "        fragA.a = 0.0;\n");
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
}

static VCShaderSubprogramSignature *VCShaderCompiler_CreateSignatureForSubprogram(
        VCShaderSubprogram *subprogram) {
    VCShaderSubprogramSignature *signature = (VCShaderSubprogramSignature *)
        malloc(sizeof(VCShaderSubprogramSignature));
    signature->source = subprogram->source;
    memset(&signature->hh, '\0', sizeof(signature->hh));

    signature->serializedData = VCString_Create();
    VCShaderCompiler_AppendFunctionToSignature(&signature->serializedData, &subprogram->rgb);
    VCShaderCompiler_AppendFunctionToSignature(&signature->serializedData, &subprogram->a);
    return signature;
}

VCShaderSubprogramContext VCShaderCompiler_CreateSubprogramContext(VCColor primColor,
                                                                   VCColor envColor,
                                                                   bool secondCycleEnabled,
                                                                   uint8_t triangleMode) {
    VCShaderSubprogramContext subprogramContext;
    subprogramContext.primColor = primColor;
    subprogramContext.envColor = envColor;
    subprogramContext.secondCycleEnabled = secondCycleEnabled;
    subprogramContext.triangleMode = triangleMode;
    return subprogramContext;
}

static size_t VCShaderCompiler_GetSubprogramSignatureCount(
        VCShaderSubprogramSignatureTable *table) {
    return HASH_COUNT(table->entries);
}

uint16_t VCShaderCompiler_GetOrCreateSubprogramID(VCShaderSubprogramSignatureTable *table,
                                                  VCShaderSubprogramSignature *signature) {
    VCShaderSubprogramSignatureTableEntry *entry = NULL;
    HASH_FIND(hh,
              table->entries,
              signature->serializedData.ptr,
              signature->serializedData.len,
              entry);
    if (entry != NULL)
        return entry->id;

    entry = (VCShaderSubprogramSignatureTableEntry *)
        malloc(sizeof(VCShaderSubprogramSignatureTableEntry));
    entry->signature = *signature;
    size_t id = VCShaderCompiler_GetSubprogramSignatureCount(table);
    assert(id <= UINT16_MAX);
    entry->id = (uint16_t)id;
    memset(&entry->hh, '\0', sizeof(entry->hh));
    HASH_ADD_KEYPTR(hh,
                    table->entries,
                    signature->serializedData.ptr,
                    signature->serializedData.len,
                    entry);
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

