// mupen64plus-video-videocore/VCShaderCompiler.cpp
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#include "VCShaderCompiler.h"
#include "VCUtils.h"
#include "uthash.h"

#define VC_SHADER_INSTRUCTION_MOVE          0
#define VC_SHADER_INSTRUCTION_ADD           1
#define VC_SHADER_INSTRUCTION_SUB           2
#define VC_SHADER_INSTRUCTION_MUL           3

#define VC_SHADER_MAX_INSTRUCTIONS          16
#define VC_SHADER_MAX_CONSTANTS             16

#define VC_SHADER_COMPILER_REGISTER_VALUE       0x00
#define VC_SHADER_COMPILER_CONSTANT_VALUE       0x80
#define VC_SHADER_COMPILER_TEXEL0_VALUE         0xfa
#define VC_SHADER_COMPILER_TEXEL0_ALPHA_VALUE   0xfb
#define VC_SHADER_COMPILER_TEXEL1_VALUE         0xfc
#define VC_SHADER_COMPILER_TEXEL1_ALPHA_VALUE   0xfd
#define VC_SHADER_COMPILER_SHADE_VALUE          0xfe
#define VC_SHADER_COMPILER_OUTPUT_VALUE         0xff

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
    VCShaderSubprogramDescriptor descriptor;
    VCShaderFunction rgb;
    VCShaderFunction alpha;
    uint16_t id;
    UT_hash_handle hh;
};

struct VCUnpackedCombinerFunction {
    uint8_t sa;
    uint8_t sb;
    uint8_t m;
    uint8_t a;
};

VCShaderProgramDescriptorLibrary VCShaderCompiler_CreateShaderProgramDescriptorLibrary() {
    VCShaderProgramDescriptorLibrary library = { NULL, 0 };
    return library;
}

static VCShaderProgramDescriptor *VCShaderCompiler_CreateShaderProgramDescriptor(
        uint16_t id,
        VCShaderSubprogramDescriptorList *subprogramDescriptors) {
    VCShaderProgramDescriptor *programDescriptor =
        (VCShaderProgramDescriptor *)malloc(sizeof(VCShaderProgramDescriptor));
    programDescriptor->id = id;
    programDescriptor->subprogramDescriptors = subprogramDescriptors;
    return programDescriptor;
}

uint16_t VCShaderCompiler_GetOrCreateShaderProgramDescriptor(
        VCShaderDescriptorLibrary *library,
        VCShaderSubprogramDescriptorList *subprogramDescriptors) {
    VCShaderProgramDescriptor *programDescriptor = NULL;
    HASH_FIND(hh,
              library->programDescriptors,
              subprogramDescriptors,
              sizeof(VCShaderSubprogramDescriptorList),
              programDescriptor);
    if (programDescriptor != NULL)
        return programDescriptor->id;

    uint16_t id = programDescriptor->programDescriptorCount;
    programDescriptor->programDescriptorCount++;
    programDescriptor = VCShaderCompiler_CreateProgramDescriptor(id, subprogramKeys);
    HASH_ADD(hh,
             library->programDescriptors,
             subprogramDescriptors,
             sizeof(VCShaderSubprogramDescriptorList),
             programDescriptor);
    return id;
}

static uint8_t VCShaderCompiler_GetConstant(VCShaderFunction *function, VCColor *color) {
    size_t constantIndex = function->constantCount;
    assert(constantIndex < VC_SHADER_MAX_CONSTANTS);
    function->constants[constantIndex] = color;
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
        return VCShaderCompiler_GetScalarConstant(function, compiler->primColor.a);
    case G_CCMUX_ENV_ALPHA:
        return VCShaderCompiler_GetScalarConstant(function, compiler->envColor.a);
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

static VCShaderFunction VCShaderCompiler_GenerateFunction(VCShaderSubprogramContext *context,
                                                          VCUnpackedCombinerFunction *cycle0,
                                                          VCUnpackedCombinerFunction *cycle1) {
    VCShaderFunction function;
    function->instructions = (VCShaderInstruction *)malloc(sizeof(VCShaderInstruction) *
                                                           VC_SHADER_MAX_INSTRUCTIONS);
    function->instructionCount = 0;
    function->constants = (VCColor *)malloc(sizeof(VCColor) * VC_SHADER_MAX_CONSTANTS);
    function->constantCount = 0;

    uint8_t output = VCShaderCompiler_GenerateInstructionsForCycle(context,
                                                                   function,
                                                                   cycle0,
                                                                   false);
    if (cycle1 != NULL)
        output = VCShaderCompiler_GenerateInstructionsForCycle(context, function, cycle1, true);

    VCShaderInstruction *outputInstruction = VCShaderCompiler_AllocateInstruction(function);
    outputInstruction->operation = VC_SHADER_INSTRUCTION_MOVE;
    outputInstruction->destination = VC_SHADER_COMPILER_OUTPUT_VALUE;
    outputInstruction->operands[0] = output;
    outputInstruction->operands[1] = VCShaderCompiler_GetScalarConstant(function, 0);

    return function;
}

static VCShaderSubprogram *VCShaderCompiler_CreateSubprogram(VCShaderSubprogramKey *key,
                                                             uint16_t id) {
    VCShaderSubprogram *subprogram = (VCShaderSubprogram *)malloc(sizeof(VCShaderSubprogram));
    subprogram->key = *key;
    subprogram->id = id;

    VCUnpackedCombinerFunction cycle0Function;
    cycle0Function.sa = key->cycle0->saRGB;
    cycle0Function.sb = key->cycle0->sbRGB;
    cycle0Function.m = key->cycle0->mRGB;
    cycle0Function.a = key->cycle0->aRGB;
    if (cycle1 == NULL) {
        combiner.rgb = VCShaderCompiler_GenerateFunction(&key->context, &cycle0Function, NULL);
    } else {
        VCUnpackedCombinerFunction cycle1Function;
        cycle1Function.sa = key->cycle1->saRGB;
        cycle1Function.sb = key->cycle1->sbRGB;
        cycle1Function.m = key->cycle1->mRGB;
        cycle1Function.a = key->cycle1->aRGB;
        combiner.rgb = VCShaderCompiler_GenerateFunction(&key->context,
                                                         &cycle0Function,
                                                         &cycle1Function);
    }

    cycle0Function.sa = key->cycle0->saA;
    cycle0Function.sb = key->cycle0->sbA;
    cycle0Function.m = key->cycle0->mA;
    cycle0Function.a = key->cycle0->aA;
    if (cycle1 == NULL) {
        combiner.a = VCShaderCompiler_GenerateFunction(&key->context, &cycle0Function, NULL);
    } else {
        VCUnpackedCombinerFunction cycle1Function;
        cycle1Function.sa = key->cycle1->saA;
        cycle1Function.sb = key->cycle1->sbA;
        cycle1Function.m = key->cycle1->mA;
        cycle1Function.a = key->cycle1->aA;
        combiner.a = VCShaderCompiler_GenerateFunction(&key->context,
                                                       &cycle0Function,
                                                       &cycle1Function);
    }

    return subprogram;
}

static uint16_t VCShaderCompiler_CreateSubprogram(
        VCShaderProgram *program,
        VCShaderSubprogramDescriptor *subprogramDescriptor) {
    VCShaderSubprogram *subprogram = NULL;
    HASH_FIND(hh, program->subprograms, key, sizeof(VCShaderSubprogramKey), subprogram);
    if (subprogram != NULL)
        return subprogram->id;

    uint16_t id = program->subprogramCount;
    program->subprogramCount++;
    subprogram = VCShaderCompiler_CreateSubprogram(key, id);
    HASH_ADD(hh, program->subprograms, key, sizeof(VCShaderSubprogramKey), subprogram);
    return id;
}

static void VCShaderCompiler_GenerateGLSLForValue(VCString *shaderSource, uint8_t value) {
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
    }

    if (staticString != NULL) {
        VCString_AppendCString(shaderSource, staticString);
        return;
    }
    if ((value & VC_SHADER_COMPILER_CONSTANT_VALUE) != 0) {
        uint8_t constantIndex = (value & ~VC_SHADER_CONSTANT_VALUE);
        assert(constantIndex < function->constantCount);
        VCColor *constant = &function->constants[constantIndex];
        VCString_AppendFormat(shaderSource,
                              "vec4(%f, %f, %f, %f)", 
                              (float)constant->r / 255.0,
                              (float)constant->g / 255.0,
                              (float)constant->b / 255.0,
                              (float)constant->a / 255.0);
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
        VCShaderCompiler_GenerateGLSLForValue(shaderSource, instruction->destination);
        VCShaderCompiler_AppendCString(shaderSource, " = ");
        VCShaderCompiler_GenerateGLSLForValue(shaderSource, instruction->operands[0]);
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
            VCShaderCompiler_AppendFormat(shaderSource, " %c ", glslOperator);
            VCShaderCompiler_GenerateGLSLForValue(shaderSource, instruction->operands[1]);
        }
        VCShaderCompiler_AppendCString(shaderSource, ";\n");
    }
}

void VCShaderCompiler_GenerateGLSLFragmentShaderForProgram(VCString *shaderSource,
                                                           VCShaderProgram *program) {
    assert(program->subprogramCount > 0);
    size_t registerCount = VCShaderCompiler_CountRegistersUsedInProgram(program);
    VCString_AppendCString(&shaderSource, "void main(void) {\n");
    VCString_AppendCString(
            &shaderSource,
            "    vec4 texture0Color = texture2D(uTexture0, AtlasUv(vTexture0Bounds));\n");
    VCString_AppendCString(
            &shaderSource,
            "    vec4 texture1Color = texture2D(uTexture1, AtlasUv(vTexture1Bounds));\n");
    VCString_AppendCString(&shaderSource, "    vec4 fragRGB;");
    VCString_AppendCString(&shaderSource, "    vec4 fragA;");
    for (size_t i = 0; i < registerCount; i++)
        VCString_AppendFormat(&shaderSource, "    vec4 r%d;\n", (int)i);
    for (size_t subprogramIndex = 0;
         subprogramIndex < program->subprogramCount;
         subprogramIndex++) {
        VCShaderSubprogram *subprogram = &program->subprograms[subprogramIndex];
        if (subprogramIndex == 0) {
            VCString_AppendCString(&shaderSource, "    if (vSubprogram == 0.0) {\n");
        } else {
            VCString_AppendFormat(&shaderSource,
                                  "    } else if (vSubprogram == %d.0) {\n",
                                  (int)subprogramIndex);
        }
        VCShaderCompiler_GenerateGLSLForFunction(&shaderSource, &subprogram->rgb, "fragRGB");
        VCShaderCompiler_GenerateGLSLForFunction(&shaderSource, &subprogram->a, "fragA");
        VCString_AppendCString(&shaderSource, "\n");
    }
    VCString_AppendCString(&shaderSource, "    }\n");
    VCString_AppendCString(&shaderSource, "    gl_FragColor = vec4(fragRGB.rgb, fragA.a);\n");
    VCString_AppendCString(&shaderSource, "}\n");
}

VCShaderProgram VCShaderCompiler_CreateProgram(VCShaderSubprogram *subprograms,
                                               size_t subprogramCount) {
    VCShaderProgram program = { subprograms, subprogramCount };
    return program;
}

VCShaderSubprogramDescriptorList VCShaderCompiler_CreateSubprogramDescriptorList() {
    VCShaderSubprogramDescriptorList list = { NULL, 0, 0 };
    return list;
}

