// mupen64plus-video-videocore/VCShaderCompiler.h
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#ifndef VC_SHADER_COMPILER_H
#define VC_SHADER_COMPILER_H

#include "VCCombiner.h"
#include "VCGeometry.h"
#include "uthash.h"
#include <stdint.h>

struct VCShaderProgram;
struct VCShaderProgramDescriptorLibrary;
struct VCShaderSubprogram;
struct VCShaderSubprogramDescriptorListEntry;
struct VCShaderSubprogramDescriptorTableEntry;
struct VCShaderSubprogramEntry;
struct VCString;

struct VCShaderSubprogramContext {
    VCColor primColor;
    VCColor envColor;
    bool secondCycleEnabled;
};

struct VCShaderSubprogramDescriptor {
    VCUnpackedCombiner cycle0;
    VCUnpackedCombiner cycle1;
    VCShaderSubprogramContext context;
};

struct VCShaderSubprogramDescriptorTable {
    VCShaderSubprogramDescriptorTableEntry *entries;
};

struct VCShaderSubprogramDescriptorList {
    VCShaderSubprogramDescriptorListEntry *entries;
    uint32_t length;
};

struct VCShaderProgramDescriptor {
    VCShaderSubprogramDescriptorList subprogramDescriptors;
    // Lazily initialized pointer to the actual program.
    VCShaderProgram *program;
    uint16_t id;
    UT_hash_handle hh;
};

VCShaderProgramDescriptorLibrary *VCShaderCompiler_CreateShaderProgramDescriptorLibrary();
uint16_t VCShaderCompiler_GetOrCreateShaderProgramID(
        VCShaderProgramDescriptorLibrary *library,
        VCShaderSubprogramDescriptorList *subprogramDescriptors,
        bool *newlyCreated);
VCShaderSubprogramDescriptorTable VCShaderCompiler_CreateSubprogramDescriptorTable();
void VCShaderCompiler_DestroySubprogramDescriptorTable(VCShaderSubprogramDescriptorTable *table);
VCShaderSubprogramDescriptor VCShaderCompiler_CreateSubprogramDescriptorForCurrentCombiner(
        VCShaderSubprogramContext *context);
void VCShaderCompiler_GenerateGLSLFragmentShaderForProgram(VCString *shaderSource,
                                                           VCShaderProgram *program);
VCShaderSubprogramContext VCShaderCompiler_CreateSubprogramContext(VCColor primColor,
                                                                   VCColor envColor,
                                                                   bool secondCycleEnabled);
uint16_t VCShaderCompiler_GetOrCreateSubprogramID(VCShaderSubprogramDescriptorTable *table,
                                                  VCShaderSubprogramDescriptor *descriptor);
VCShaderProgram *VCShaderCompiler_GetOrCreateProgram(VCShaderProgramDescriptor *descriptor);
VCShaderProgramDescriptor *VCShaderCompiler_GetShaderProgramDescriptorByID(
        VCShaderProgramDescriptorLibrary *library,
        uint16_t id);
VCShaderSubprogramDescriptorList VCShaderCompiler_ConvertSubprogramDescriptorTableToList(
        VCShaderSubprogramDescriptorTable *table);
void VCShaderCompiler_DestroySubprogramDescriptorList(VCShaderSubprogramDescriptorList *list);

#endif

