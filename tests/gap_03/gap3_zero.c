/* Zero-argument calls into the d3d8thk.dll win32k thunks and the legacy D3DKMT exports of d3d11.dll (Windows 10 22H2 x64).
 * Every call runs in its own child process with all six argument registers zero; the child exits with the low 32 bits of the return value, or
 * G3Z_AV (exit code 0x4d) when the call raises an access violation (the kernel-mode probe of a NULL pointer is not caught by every thunk). */
#include "t.h"
#include <stdio.h>
#define G3Z_AV 0xa5a5a5a5a5a5a5a5ull
typedef ULONG_PTR (WINAPI *g3z_f6)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
static LONG WINAPI g3z_filter(EXCEPTION_POINTERS *e) { (void)e; ExitProcess(0x4d); return 0; }
CHILD(g3z_call) {
    SetUnhandledExceptionFilter(g3z_filter);
    HMODULE m = LoadLibraryA(argv[1]);
    FARPROC p = m ? GetProcAddress(m, argv[2]) : NULL;
    if (!p) return 90;
    ULONG_PTR r = ((g3z_f6)p)(0, 0, 0, 0, 0, 0);
    printf("ZEROARGS %s %016llx\n", argv[2], (unsigned long long)r);
    return (int)(DWORD)r;                  /* the exit code carries the low 32 bits of the return value */
}
static const struct { const char *dll, *name; unsigned long long want; } g3z_tab[] = {
  { "d3d8thk.dll", "OsThunkD3dContextCreate", 0x0ull },
  { "d3d8thk.dll", "OsThunkD3dContextDestroy", 0x0ull },
  { "d3d8thk.dll", "OsThunkD3dContextDestroyAll", 0x0ull },
  { "d3d8thk.dll", "OsThunkD3dDrawPrimitives2", 0x0ull },
  { "d3d8thk.dll", "OsThunkD3dValidateTextureStageState", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdAddAttachedSurface", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdAlphaBlt", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdAttachSurface", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdBeginMoCompFrame", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdBlt", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdCanCreateD3DBuffer", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdCanCreateSurface", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdColorControl", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdCreateD3DBuffer", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdCreateDirectDrawObject", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdCreateMoComp", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdCreateSurface", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdCreateSurfaceEx", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdCreateSurfaceObject", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdDeleteDirectDrawObject", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdDeleteSurfaceObject", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdDestroyD3DBuffer", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdDestroyMoComp", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdDestroySurface", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdEndMoCompFrame", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdFlip", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdFlipToGDISurface", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdGetAvailDriverMemory", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdGetBltStatus", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdGetDC", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdGetDriverInfo", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdGetDriverState", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdGetDxHandle", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdGetFlipStatus", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdGetInternalMoCompInfo", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdGetMoCompBuffInfo", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdGetMoCompFormats", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdGetMoCompGuids", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdGetScanLine", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdLock", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdLockD3D", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdQueryDirectDrawObject", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdQueryMoCompStatus", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdReenableDirectDrawObject", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdReleaseDC", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdRenderMoComp", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdResetVisrgn", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdSetColorKey", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdSetExclusiveMode", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdSetGammaRamp", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdSetOverlayPosition", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdUnattachSurface", 0xc0000001ull },
  { "d3d8thk.dll", "OsThunkDdUnlock", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdUnlockD3D", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdUpdateOverlay", 0x0ull },
  { "d3d8thk.dll", "OsThunkDdWaitForVerticalBlank", 0x0ull },
  { "d3d11.dll", "D3DKMTCloseAdapter", 0x0ull },
  { "d3d11.dll", "D3DKMTCreateAllocation", 0x0ull },
  { "d3d11.dll", "D3DKMTCreateContext", 0x0ull },
  { "d3d11.dll", "D3DKMTCreateDevice", 0x0ull },
  { "d3d11.dll", "D3DKMTCreateSynchronizationObject", 0x0ull },
  { "d3d11.dll", "D3DKMTDestroyAllocation", 0x0ull },
  { "d3d11.dll", "D3DKMTDestroyContext", 0x0ull },
  { "d3d11.dll", "D3DKMTDestroyDevice", 0x0ull },
  { "d3d11.dll", "D3DKMTDestroySynchronizationObject", 0x0ull },
  { "d3d11.dll", "D3DKMTEscape", G3Z_AV },
  { "d3d11.dll", "D3DKMTGetContextSchedulingPriority", 0x0ull },
  { "d3d11.dll", "D3DKMTGetDeviceState", G3Z_AV },
  { "d3d11.dll", "D3DKMTGetDisplayModeList", 0x0ull },
  { "d3d11.dll", "D3DKMTGetMultisampleMethodList", 0x0ull },
  { "d3d11.dll", "D3DKMTGetRuntimeData", 0x0ull },
  { "d3d11.dll", "D3DKMTGetSharedPrimaryHandle", 0x0ull },
  { "d3d11.dll", "D3DKMTLock", 0x0ull },
  { "d3d11.dll", "D3DKMTOpenAdapterFromHdc", G3Z_AV },
  { "d3d11.dll", "D3DKMTOpenResource", 0xc000000dull },
  { "d3d11.dll", "D3DKMTPresent", 0x0ull },
  { "d3d11.dll", "D3DKMTQueryAdapterInfo", G3Z_AV },
  { "d3d11.dll", "D3DKMTQueryAllocationResidency", 0x0ull },
  { "d3d11.dll", "D3DKMTQueryResourceInfo", 0xc000000dull },
  { "d3d11.dll", "D3DKMTRender", 0x0ull },
  { "d3d11.dll", "D3DKMTSetAllocationPriority", 0x0ull },
  { "d3d11.dll", "D3DKMTSetContextSchedulingPriority", 0x0ull },
  { "d3d11.dll", "D3DKMTSetDisplayMode", 0x0ull },
  { "d3d11.dll", "D3DKMTSetDisplayPrivateDriverFormat", 0x0ull },
  { "d3d11.dll", "D3DKMTSetGammaRamp", 0x0ull },
  { "d3d11.dll", "D3DKMTSetVidPnSourceOwner", 0x0ull },
  { "d3d11.dll", "D3DKMTSignalSynchronizationObject", 0x0ull },
  { "d3d11.dll", "D3DKMTUnlock", 0x0ull },
  { "d3d11.dll", "D3DKMTWaitForSynchronizationObject", 0x0ull },
  { "d3d11.dll", "D3DKMTWaitForVerticalBlankEvent", 0x0ull },
};
static void g3z_run(const char *dll)
{
    unsigned i; HMODULE m = t_module(dll);
    if (!m) { t_skip("%s not loadable", dll); return; }
    for (i = 0; i < sizeof g3z_tab / sizeof g3z_tab[0]; i++) {
        char a[200]; DWORD rc;
        if (strcmp(g3z_tab[i].dll, dll)) continue;
        if (!GetProcAddress(m, g3z_tab[i].name)) { CHECKF(0, "%s!%s missing", dll, g3z_tab[i].name); continue; }
        snprintf(a, sizeof a, "%s %s", dll, g3z_tab[i].name);
        rc = t_run_child("g3z_call", a, 30000);
        if (g3z_tab[i].want == G3Z_AV) t_check(rc == 0x4d, __FILE__, __LINE__, "zero-argument call faults", "%s!%s child rc %#lx (want access violation, 0x4d)", dll, g3z_tab[i].name, rc);
        else t_check(rc == (DWORD)g3z_tab[i].want, __FILE__, __LINE__, "zero-argument return value", "%s!%s returned %#lx want %#llx", dll, g3z_tab[i].name, rc, g3z_tab[i].want);
    }
}
TEST(gap3_zero_arg_calls_d3d8thk_thunks) { g3z_run("d3d8thk.dll"); }
TEST(gap3_zero_arg_calls_d3dkmt_exports) { g3z_run("d3d11.dll"); }
