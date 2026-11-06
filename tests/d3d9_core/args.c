/* Argument validation of IDirect3D9 / IDirect3D9Ex methods, measured on Win10 22H2: output pointers are not checked (access violation),
 * presentation parameters are validated before the driver is reached (D3DERR_INVALIDCALL). Each case runs in its own t_try. */
#include "d9c.h"

static IDirect3D9 *g_d; static IDirect3D9Ex *g_dx; static int g_ci; static HRESULT g_hr; static HWND g_hw; static IDirect3DDevice9 *g_dev; static IDirect3DDevice9Ex *g_devx;
static void args_run(void *p) {
    static D3DPRESENT_PARAMETERS pp; IDirect3DDevice9 **pdev = &g_dev; IDirect3DDevice9Ex **pdevx = &g_devx; static D3DDISPLAYMODEEX mex; static D3DDISPLAYMODEFILTER fl = { sizeof(D3DDISPLAYMODEFILTER), D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_UNKNOWN };
    memset(&pp, 0, sizeof pp); pp.BackBufferWidth = 64; pp.BackBufferHeight = 64; pp.BackBufferFormat = D3DFMT_UNKNOWN; pp.BackBufferCount = 1; pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow = g_hw;
    D3DCAPS9 caps; D3DADAPTER_IDENTIFIER9 id; D3DDISPLAYMODE m; UINT q; HMONITOR mon; LUID l; int n = 0;
#define C(nm, expr) if (g_ci == n++) { g_hr = (expr); return; }
    C("GetAdapterIdentifier NULL", IDirect3D9_GetAdapterIdentifier(g_d, 0, 0, NULL))
    C("GetAdapterDisplayMode NULL", IDirect3D9_GetAdapterDisplayMode(g_d, 0, NULL))
    C("EnumAdapterModes NULL", IDirect3D9_EnumAdapterModes(g_d, 0, D3DFMT_X8R8G8B8, 0, NULL))
    C("GetDeviceCaps NULL", IDirect3D9_GetDeviceCaps(g_d, 0, D3DDEVTYPE_HAL, NULL))
    C("CheckDeviceMultiSampleType NULL quality", IDirect3D9_CheckDeviceMultiSampleType(g_d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_NONE, NULL))
    C("CreateDevice NULL pp", IDirect3D9_CreateDevice(g_d, 0, D3DDEVTYPE_HAL, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, NULL, pdev))
    C("CreateDevice NULL out", IDirect3D9_CreateDevice(g_d, 0, D3DDEVTYPE_HAL, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, NULL))
    C("CreateDevice bad adapter", IDirect3D9_CreateDevice(g_d, 99, D3DDEVTYPE_HAL, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, pdev))
    C("CreateDevice bad devtype", IDirect3D9_CreateDevice(g_d, 0, (D3DDEVTYPE)77, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, pdev))
    C("CreateDevice no vp flags", IDirect3D9_CreateDevice(g_d, 0, D3DDEVTYPE_HAL, g_hw, 0, &pp, pdev))
    C("CreateDevice two vp flags", IDirect3D9_CreateDevice(g_d, 0, D3DDEVTYPE_HAL, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, pdev))
    C("CreateDevice NULL hwnd windowed w/ hDeviceWindow", IDirect3D9_CreateDevice(g_d, 0, D3DDEVTYPE_HAL, NULL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, pdev))
    C("CreateDevice bad hwnd", IDirect3D9_CreateDevice(g_d, 0, D3DDEVTYPE_HAL, (HWND)(ULONG_PTR)0x1234, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, pdev))
    C("CreateDevice backbuffer count 99", (pp.BackBufferCount = 99, IDirect3D9_CreateDevice(g_d, 0, D3DDEVTYPE_HAL, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, pdev)))
    C("CreateDevice swapeffect 0", (pp.SwapEffect = 0, IDirect3D9_CreateDevice(g_d, 0, D3DDEVTYPE_HAL, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, pdev)))
    C("CreateDevice swapeffect 77", (pp.SwapEffect = 77, IDirect3D9_CreateDevice(g_d, 0, D3DDEVTYPE_HAL, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, pdev)))
    C("CreateDevice DS enabled UNKNOWN fmt", (pp.EnableAutoDepthStencil = TRUE, IDirect3D9_CreateDevice(g_d, 0, D3DDEVTYPE_HAL, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, pdev)))
    C("CreateDevice backbuffer fmt D16", (pp.BackBufferFormat = D3DFMT_D16, IDirect3D9_CreateDevice(g_d, 0, D3DDEVTYPE_HAL, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, pdev)))
    C("CreateDevice presentation interval 0x7", (pp.PresentationInterval = 7, IDirect3D9_CreateDevice(g_d, 0, D3DDEVTYPE_HAL, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, pdev)))
    C("CreateDevice multisample 77", (pp.MultiSampleType = 77, IDirect3D9_CreateDevice(g_d, 0, D3DDEVTYPE_HAL, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, pdev)))
    C("CreateDevice ok", IDirect3D9_CreateDevice(g_d, 0, D3DDEVTYPE_HAL, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, pdev))
    C("Ex GetAdapterModeCountEx NULL", IDirect3D9Ex_GetAdapterModeCountEx(g_dx, 0, NULL) ? S_OK : S_FALSE)
    C("Ex EnumAdapterModesEx NULL filter", IDirect3D9Ex_EnumAdapterModesEx(g_dx, 0, NULL, 0, &mex))
    C("Ex EnumAdapterModesEx NULL mode", IDirect3D9Ex_EnumAdapterModesEx(g_dx, 0, &fl, 0, NULL))
    C("Ex GetAdapterDisplayModeEx NULL mode", IDirect3D9Ex_GetAdapterDisplayModeEx(g_dx, 0, NULL, NULL))
    C("Ex GetAdapterLUID NULL", IDirect3D9Ex_GetAdapterLUID(g_dx, 0, NULL))
    C("Ex CreateDeviceEx NULL pp", IDirect3D9Ex_CreateDeviceEx(g_dx, 0, D3DDEVTYPE_HAL, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, NULL, NULL, pdevx))
    C("Ex CreateDeviceEx NULL out", IDirect3D9Ex_CreateDeviceEx(g_dx, 0, D3DDEVTYPE_HAL, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, NULL, NULL))
    C("Ex CreateDeviceEx windowed with fullscreen mode", IDirect3D9Ex_CreateDeviceEx(g_dx, 0, D3DDEVTYPE_HAL, g_hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &mex, pdevx))
    C("GetAdapterMonitor bad adapter", IDirect3D9_GetAdapterMonitor(g_d, 99) ? S_OK : S_FALSE)
    C("CheckDeviceType NULL?? unknown adapter fmt", IDirect3D9_CheckDeviceType(g_d, 0, D3DDEVTYPE_HAL, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN, TRUE))
    C("CheckDepthStencilMatch D3DFMT_UNKNOWN", IDirect3D9_CheckDepthStencilMatch(g_d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_UNKNOWN))
}

static const struct { DWORD code; HRESULT hr; } k_exp[] = {
    { 0xC0000005u, 0 }, /* GetAdapterIdentifier NULL */
    { 0xC0000005u, 0 }, /* GetAdapterDisplayMode NULL */
    { 0xC0000005u, 0 }, /* EnumAdapterModes NULL */
    { 0xC0000005u, 0 }, /* GetDeviceCaps NULL */
    { 0, S_OK }, /* CheckDeviceMultiSampleType NULL quality */
    { 0xC0000005u, 0 }, /* CreateDevice NULL pp */
    { 0xC0000005u, 0 }, /* CreateDevice NULL out */
    { 0, D9_INVALIDCALL }, /* CreateDevice bad adapter */
    { 0, D9_INVALIDCALL }, /* CreateDevice bad devtype */
    { 0, D9_INVALIDCALL }, /* CreateDevice no vp flags */
    { 0, D9_INVALIDCALL }, /* CreateDevice two vp flags */
    { 0, S_OK }, /* CreateDevice NULL hwnd windowed w/ hDeviceWindow */
    { 0, D9_INVALIDCALL }, /* CreateDevice bad hwnd */
    { 0, D9_INVALIDCALL }, /* CreateDevice backbuffer count 99 */
    { 0, D9_INVALIDCALL }, /* CreateDevice swapeffect 0 */
    { 0, D9_INVALIDCALL }, /* CreateDevice swapeffect 77 */
    { 0, D9_INVALIDCALL }, /* CreateDevice DS enabled UNKNOWN fmt */
    { 0, D9_INVALIDCALL }, /* CreateDevice backbuffer fmt D16 */
    { 0, D9_INVALIDCALL }, /* CreateDevice presentation interval 0x7 */
    { 0, D9_INVALIDCALL }, /* CreateDevice multisample 77 */
    { 0, S_OK }, /* CreateDevice ok */
    { 0, S_FALSE }, /* Ex GetAdapterModeCountEx NULL */
    { 0, D9_INVALIDCALL }, /* Ex EnumAdapterModesEx NULL filter */
    { 0xC0000005u, 0 }, /* Ex EnumAdapterModesEx NULL mode */
    { 0xC0000005u, 0 }, /* Ex GetAdapterDisplayModeEx NULL mode */
    { 0xC0000005u, 0 }, /* Ex GetAdapterLUID NULL */
    { 0xC0000005u, 0 }, /* Ex CreateDeviceEx NULL pp */
    { 0xC0000005u, 0 }, /* Ex CreateDeviceEx NULL out */
    { 0, D9_INVALIDCALL }, /* Ex CreateDeviceEx windowed with fullscreen mode */
    { 0, S_FALSE }, /* GetAdapterMonitor bad adapter */
    { 0, D9_INVALIDCALL }, /* CheckDeviceType NULL?? unknown adapter fmt */
    { 0, D9_INVALIDCALL }, /* CheckDepthStencilMatch D3DFMT_UNKNOWN */
};
TEST(d3d9_core_args_nullptr_and_presentation_validation) {
    D9_GET(d); D9_GET_EX(dx);
    g_d = d; g_dx = dx; g_hw = d3dh_create_hidden_window();
    /* the positive cases need a working HAL device; the negative ones do not */
    int have_device = 0;
    for (g_ci = 0; g_ci < (int)(sizeof k_exp / sizeof *k_exp); g_ci++) {
        g_hr = 0x12345678; g_dev = NULL; g_devx = NULL;
        DWORD code = t_try(args_run, NULL);
        if (k_exp[g_ci].code == 0 && k_exp[g_ci].hr == S_OK && code == 0 && SUCCEEDED(g_hr) && g_dev) { have_device = 1; }
        if (k_exp[g_ci].code == 0 && k_exp[g_ci].hr == S_OK && !have_device && (code != 0 || FAILED(g_hr))) { if (g_dev) IDirect3DDevice9_Release(g_dev); g_dev = NULL; continue; }
        CHECKF(code == k_exp[g_ci].code, "case %d: exception 0x%08lx want 0x%08lx", g_ci, (unsigned long)code, (unsigned long)k_exp[g_ci].code);
        if (!code && !k_exp[g_ci].code) CHECKF(g_hr == k_exp[g_ci].hr, "case %d: %s want %s", g_ci, d3dh_hr_name(g_hr), d3dh_hr_name(k_exp[g_ci].hr));
        if (g_dev) { IDirect3DDevice9_Release(g_dev); g_dev = NULL; }
        if (g_devx) { IDirect3DDevice9Ex_Release(g_devx); g_devx = NULL; }
    }
    d3dh_destroy_window(g_hw);
    IDirect3D9Ex_Release(dx); IDirect3D9_Release(d);
}
