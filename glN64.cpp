#include <string.h>
#include "glN64.h"
#include "Debug.h"
#include "N64.h"
#include "RSP.h"
#include "RDP.h"
#include "VI.h"
#include "Combiner.h"
#include "VCConfig.h"
#include "VCShaderCompiler.h"
#include "m64p_plugin.h"

#define MI_INTR_SP 0x01
#define MI_INTR_DP 0x20

#define VIDEO_PLUGIN_API_VERSION 0x020200

char		pluginName[] = "mupen64plus-video-videocore";
char		*screenDirectory;

void (*CheckInterrupts)( void );

extern "C" {

EXPORT void CALL CaptureScreen ( char * Directory )
{
	screenDirectory = Directory;
}

EXPORT void CALL ChangeWindow (void)
{
}

EXPORT void CALL CloseDLL (void)
{
}

EXPORT void CALL DrawScreen (void)
{
}

EXPORT m64p_error CALL PluginGetVersion(m64p_plugin_type *PluginType,
                                        int *PluginVersion,
                                        int *APIVersion,
                                        const char **PluginNamePtr,
                                        int *Capabilities)
{
    if (PluginType != NULL)
        *PluginType = M64PLUGIN_GFX;
    if (PluginVersion != NULL)
        *PluginVersion = 0x103;
    if (APIVersion != NULL)
        *APIVersion = VIDEO_PLUGIN_API_VERSION;
    if (PluginType != NULL)
        *PluginType = M64PLUGIN_GFX;
    if (PluginNamePtr != NULL)
        *PluginNamePtr = "mupen64plus-video-glvideocore";
    return M64ERR_SUCCESS;
}

EXPORT int CALL InitiateGFX (GFX_INFO Gfx_Info)
{
	DMEM = Gfx_Info.DMEM;
	IMEM = Gfx_Info.IMEM;
	RDRAM = Gfx_Info.RDRAM;

	REG.MI_INTR = Gfx_Info.MI_INTR_REG;
	REG.DPC_START = Gfx_Info.DPC_START_REG;
	REG.DPC_END = Gfx_Info.DPC_END_REG;
	REG.DPC_CURRENT = Gfx_Info.DPC_CURRENT_REG;
	REG.DPC_STATUS = Gfx_Info.DPC_STATUS_REG;
	REG.DPC_CLOCK = Gfx_Info.DPC_CLOCK_REG;
	REG.DPC_BUFBUSY = Gfx_Info.DPC_BUFBUSY_REG;
	REG.DPC_PIPEBUSY = Gfx_Info.DPC_PIPEBUSY_REG;
	REG.DPC_TMEM = Gfx_Info.DPC_TMEM_REG;

	REG.VI_STATUS = Gfx_Info.VI_STATUS_REG;
	REG.VI_ORIGIN = Gfx_Info.VI_ORIGIN_REG;
	REG.VI_WIDTH = Gfx_Info.VI_WIDTH_REG;
	REG.VI_INTR = Gfx_Info.VI_INTR_REG;
	REG.VI_V_CURRENT_LINE = Gfx_Info.VI_V_CURRENT_LINE_REG;
	REG.VI_TIMING = Gfx_Info.VI_TIMING_REG;
	REG.VI_V_SYNC = Gfx_Info.VI_V_SYNC_REG;
	REG.VI_H_SYNC = Gfx_Info.VI_H_SYNC_REG;
	REG.VI_LEAP = Gfx_Info.VI_LEAP_REG;
	REG.VI_H_START = Gfx_Info.VI_H_START_REG;
	REG.VI_V_START = Gfx_Info.VI_V_START_REG;
	REG.VI_V_BURST = Gfx_Info.VI_V_BURST_REG;
	REG.VI_X_SCALE = Gfx_Info.VI_X_SCALE_REG;
	REG.VI_Y_SCALE = Gfx_Info.VI_Y_SCALE_REG;

	CheckInterrupts = Gfx_Info.CheckInterrupts;

	return TRUE;
}

EXPORT void CALL MoveScreen (int xpos, int ypos)
{
}

EXPORT void CALL ProcessDList(void)
{
    uint32_t beforeProcessTimestamp = SDL_GetTicks();
    VCRenderer *renderer = VCRenderer_SharedRenderer();
    VCRenderer_BeginNewFrame(renderer);

	RSP_ProcessDList();

    *REG.MI_INTR |= MI_INTR_DP;
    CheckInterrupts();
    *REG.MI_INTR |= MI_INTR_SP;
    CheckInterrupts();

    VCRenderer_AllocateTexturesAndEnqueueTextureUploadCommands(renderer);
    VCRenderer_CreateNewShaderProgramsIfNecessary(renderer);
    VCRenderer_PopulateTextureBoundsInBatches(renderer);
    VCRenderer_SendBatchesToRenderThread(renderer, SDL_GetTicks() - beforeProcessTimestamp);
    VCRenderer_SubmitCommands(renderer);
    VCRenderer_EndFrame(renderer);
}

EXPORT void CALL ProcessRDPList(void)
{
}

EXPORT void CALL RomClosed (void)
{
#ifdef DEBUG
	CloseDebugDlg();
#endif
}

EXPORT int CALL RomOpen (void)
{
    RSP_Init();
    VCRenderer *renderer = VCRenderer_SharedRenderer();
    VCRenderer_Start(renderer);
    return TRUE;
}

EXPORT void CALL ShowCFB (void)
{	
}

EXPORT void CALL UpdateScreen (void)
{
#ifdef RSPTHREAD
	if (RSP.thread)
	{
		SetEvent( RSP.threadMsg[RSPMSG_UPDATESCREEN] );
		WaitForSingleObject( RSP.threadFinished, INFINITE );
	}
#else
	VI_UpdateScreen();
#endif
}

EXPORT void CALL ViStatusChanged (void)
{
}

EXPORT void CALL ViWidthChanged (void)
{
}

EXPORT void CALL ReadScreen (void **dest, long *width, long *height)
{
}

EXPORT void CALL ReadScreen2(void *dest, int *width, int *height, int front) {
    // TODO
}

EXPORT void CALL SetRenderingCallback(void (*callback)(int)) {
    // TODO
}

EXPORT void CALL ResizeVideoOutput(int width, int height) {
    // TODO
}

EXPORT void CALL FBRead(unsigned int addr) {
    // TODO
}

EXPORT void CALL FBWrite(unsigned int addr, unsigned int size) {
    // TODO
}

EXPORT void CALL FBGetFrameBufferInfo(void *p) {
    // TODO
}

EXPORT m64p_error CALL PluginStartup(m64p_dynlib_handle CoreLibHandle, void *Context,
                                   void (*DebugCallback)(void *, int, const char *))
{
    VCConfig_Read(VCConfig_SharedConfig());
    return M64ERR_SUCCESS;
}

} // extern "C"

