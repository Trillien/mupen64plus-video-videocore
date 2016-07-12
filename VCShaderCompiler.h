// mupen64plus-video-videocore/VCShaderCompiler.h
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#ifndef VC_SHADER_COMPILER_H
#define VC_SHADER_COMPILER_H

#include "VCCombiner.h"
#include "VCGeometry.h"
#include "VCUtils.h"
#include "uthash.h"
#include <stdint.h>

struct VCShaderProgram;
struct VCShaderProgramDescriptorLibrary;
struct VCShaderSubprogram;
struct VCShaderSubprogramLibrary;
struct VCShaderSubprogramSignatureListEntry;
struct VCShaderSubprogramSignatureTableEntry;
struct VCShaderSubprogramEntry;

struct VCShaderSubprogramContext {
    VCColor primColor;
    VCColor envColor;
    bool secondCycleEnabled;
    uint8_t triangleMode;
};

struct VCShaderSubprogramSignatureTable {
    VCShaderSubprogramSignatureTableEntry *entries;
};

struct VCShaderSubprogramSignatureList {
    VCShaderSubprogramSignatureListEntry *entries;
    uint32_t length;
};

struct VCShaderSubprogramSource {
    VCUnpackedCombiner cycle0;
    VCUnpackedCombiner cycle1;
    VCShaderSubprogramContext context;
};

struct VCShaderSubprogramSignature {
    VCString serializedData;
    VCShaderSubprogramSource source;
    UT_hash_handle hh;
};

struct VCShaderProgramSignature {
    VCString serializedData;
    UT_hash_handle hh;
};

struct VCShaderProgramDescriptor {
    VCShaderProgramSignature programSignature;
    VCShaderSubprogramSignatureList subprogramSignatures;
    // Lazily initialized pointer to the actual program.
    VCShaderProgram *program;
    uint16_t id;
    UT_hash_handle hh;
};

VCShaderProgramDescriptorLibrary *VCShaderCompiler_CreateShaderProgramDescriptorLibrary();
uint16_t VCShaderCompiler_GetOrCreateShaderProgramID(
        VCShaderProgramDescriptorLibrary *library,
        VCShaderSubprogramSignatureList *subprogramDescriptors,
        bool *newlyCreated);
VCShaderSubprogramSignatureTable VCShaderCompiler_CreateSubprogramSignatureTable();
void VCShaderCompiler_DestroySubprogramSignatureTable(VCShaderSubprogramSignatureTable *table);
VCShaderSubprogramSignature VCShaderCompiler_CreateSubprogramSignatureForCurrentCombiner(
        VCShaderSubprogramContext *context);
void VCShaderCompiler_GenerateGLSLFragmentShaderForProgram(VCString *shaderSource,
                                                           VCShaderProgram *program);
VCShaderSubprogramContext VCShaderCompiler_CreateSubprogramContext(VCColor primColor,
                                                                   VCColor envColor,
                                                                   bool secondCycleEnabled,
                                                                   uint8_t triangleMode);
VCShaderSubprogramLibrary *VCShaderCompiler_CreateSubprogramLibrary();
uint16_t VCShaderCompiler_GetOrCreateSubprogramID(VCShaderSubprogramSignatureTable *table,
                                                  VCShaderSubprogramSignature *descriptor);
VCShaderProgram *VCShaderCompiler_GetOrCreateProgram(
        VCShaderSubprogramLibrary *subprogramLibrary,
        VCShaderProgramDescriptor *programDescriptor);
VCShaderProgramDescriptor *VCShaderCompiler_GetShaderProgramDescriptorByID(
        VCShaderProgramDescriptorLibrary *library,
        uint16_t id);
VCShaderSubprogramSignatureList VCShaderCompiler_ConvertSubprogramSignatureTableToList(
        VCShaderSubprogramSignatureTable *table);
void VCShaderCompiler_DestroySubprogramSignatureList(VCShaderSubprogramSignatureList *list);
VCShaderSubprogramSignature VCShaderCompiler_GetOrCreateSubprogramSignatureForCurrentCombiner(
        VCShaderSubprogramLibrary *library,
        VCShaderSubprogramContext *context);
void VCShaderCompiler_DestroySubprogramSignature(VCShaderSubprogramSignature *signature);
bool VCShaderCompiler_ExpireOldProgramIfNecessary(VCShaderProgramDescriptorLibrary *library,
                                                  uint16_t *shaderProgramIDToDelete);

#endif

