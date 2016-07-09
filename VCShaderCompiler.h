// mupen64plus-video-videocore/VCShaderCompiler.h
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#ifndef VC_SHADER_COMPILER_H
#define VC_SHADER_COMPILER_H

#include "VCCombiner.h"
#include "VCGeometry.h"
#include <stdint.h>

struct VCString;

struct VCShaderSubprogramContext {
    VCColor primColor;
    VCColor envColor;
};

struct VCShaderSubprogramDescriptor {
    VCUnpackedCombiner cycle0;
    VCUnpackedCombiner cycle1;
    VCShaderSubprogramContext context;
};

struct VCShaderSubprogramDescriptorList {
    VCShaderSubprogramDescriptor *descriptors;
    size_t length;
    size_t capacity;
};

struct VCShaderProgramDescriptor {
    VCShaderSubprogramDescriptorList subprogramDescriptors;
    uint16_t id;
    UT_hash_handle hh;
};

struct VCShaderProgramDescriptorLibrary {
    VCShaderProgramDescriptor *shaderProgramDescriptors;
    size_t shaderProgramDescriptorCount;
};

struct VCShaderProgram {
    VCShaderSubprogram *subprograms;
    size_t subprogramCount;
};

VCShaderProgramDescriptorLibrary VCShaderCompiler_CreateShaderProgramDescriptorLibrary();
uint16_t VCShaderCompiler_GetOrCreateShaderProgramDescriptor(
        VCShaderProgramDescriptorLibrary *library,
        VCShaderSubprogramDescriptorList *subprogramDescriptors);
VCShaderSubprogramDescriptorList VCShaderCompiler_CreateSubprogramDescriptorList();
void VCShaderCompiler_GenerateGLSLFragmentShaderForProgram(VCString *shaderSource,
                                                           VCShaderProgram *program);

#endif

