#include "d3dh.h"

/* ---------------- dynamic loading ---------------- */
#define PCACHE 128
static struct { char dll[40], name[56]; FARPROC p; } g_pc[PCACHE];
static int g_npc;
FARPROC d3dh_getproc(const char *dll, const char *name) {
    for (int i = 0; i < g_npc; i++)
        if (!lstrcmpiA(g_pc[i].dll, dll) && !strcmp(g_pc[i].name, name)) return g_pc[i].p;
    FARPROC p = t_proc_in(dll, name);
    if (g_npc < PCACHE && strlen(dll) < sizeof g_pc[0].dll && strlen(name) < sizeof g_pc[0].name) {
        strcpy(g_pc[g_npc].dll, dll); strcpy(g_pc[g_npc].name, name); g_pc[g_npc].p = p; g_npc++;
    }
    return p;
}
int d3dh_have_dll(const char *dll) { return t_module(dll) != NULL; }

/* ---------------- hidden window ---------------- */
static LRESULT CALLBACK d3dh_wndproc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcA(h, m, w, l); }
HWND d3dh_create_hidden_window_sized(int w, int h) {
    static ATOM atom;
    if (!atom) {
        WNDCLASSA wc; memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = d3dh_wndproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "d3dh_hidden";
        atom = RegisterClassA(&wc);
        if (!atom && GetLastError() == ERROR_CLASS_ALREADY_EXISTS) atom = 1;
    }
    HWND hw = CreateWindowExA(0, "d3dh_hidden", "d3dh", WS_POPUP, 0, 0, w, h, NULL, NULL, GetModuleHandleA(NULL), NULL);
    /* Headless Wine has no desktop window: top-level creation fails (1400). Fall back to a message-only window. */
    if (!hw) hw = CreateWindowExA(0, "d3dh_hidden", "d3dh", WS_POPUP, 0, 0, w, h, HWND_MESSAGE, NULL, GetModuleHandleA(NULL), NULL);
    if (hw) ShowWindow(hw, SW_HIDE);
    return hw;
}
HWND d3dh_create_hidden_window(void) { return d3dh_create_hidden_window_sized(64, 64); }
void d3dh_destroy_window(HWND h) { if (h) DestroyWindow(h); }

/* ---------------- refcount ---------------- */
ULONG d3dh_refcount(void *unk) {
    IUnknown *u = (IUnknown *)unk;
    if (!u) return 0;
    u->lpVtbl->AddRef(u);
    return u->lpVtbl->Release(u);
}

/* ---------------- HRESULT names ---------------- */
#define N(x) { (HRESULT)(x), #x }
static const struct { HRESULT hr; const char *n; } g_hr[] = {
    N(S_OK), N(S_FALSE), N(E_FAIL), N(E_INVALIDARG), N(E_OUTOFMEMORY), N(E_NOTIMPL), N(E_NOINTERFACE), N(E_POINTER),
    N(E_UNEXPECTED), N(E_ABORT), N(E_ACCESSDENIED), N(E_HANDLE), N(E_PENDING), N(E_NOT_VALID_STATE),
    N(E_BOUNDS), N(CLASS_E_NOAGGREGATION), N(REGDB_E_CLASSNOTREG),
    N(DXGI_STATUS_OCCLUDED), N(DXGI_STATUS_CLIPPED), N(DXGI_STATUS_NO_REDIRECTION), N(DXGI_STATUS_NO_DESKTOP_ACCESS),
    N(DXGI_STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE), N(DXGI_STATUS_MODE_CHANGED), N(DXGI_STATUS_MODE_CHANGE_IN_PROGRESS),
    N(DXGI_ERROR_INVALID_CALL), N(DXGI_ERROR_NOT_FOUND), N(DXGI_ERROR_MORE_DATA), N(DXGI_ERROR_UNSUPPORTED),
    N(DXGI_ERROR_DEVICE_REMOVED), N(DXGI_ERROR_DEVICE_HUNG), N(DXGI_ERROR_DEVICE_RESET), N(DXGI_ERROR_WAS_STILL_DRAWING),
    N(DXGI_ERROR_FRAME_STATISTICS_DISJOINT), N(DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE), N(DXGI_ERROR_DRIVER_INTERNAL_ERROR),
    N(DXGI_ERROR_NONEXCLUSIVE), N(DXGI_ERROR_NOT_CURRENTLY_AVAILABLE), N(DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED),
    N(DXGI_ERROR_REMOTE_OUTOFMEMORY), N(DXGI_ERROR_ACCESS_LOST), N(DXGI_ERROR_WAIT_TIMEOUT), N(DXGI_ERROR_SESSION_DISCONNECTED),
    N(DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE), N(DXGI_ERROR_CANNOT_PROTECT_CONTENT), N(DXGI_ERROR_ACCESS_DENIED),
    N(DXGI_ERROR_NAME_ALREADY_EXISTS), N(DXGI_ERROR_SDK_COMPONENT_MISSING),
    N(D3D11_ERROR_FILE_NOT_FOUND), N(D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS),
    N(D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS), N(D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD),
    N(D3D10_ERROR_FILE_NOT_FOUND), N(D3D10_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS),
    N(D3D12_ERROR_ADAPTER_NOT_FOUND), N(D3D12_ERROR_DRIVER_VERSION_MISMATCH),
    N(D3DERR_WRONGTEXTUREFORMAT), N(D3DERR_UNSUPPORTEDCOLOROPERATION), N(D3DERR_UNSUPPORTEDCOLORARG),
    N(D3DERR_UNSUPPORTEDALPHAOPERATION), N(D3DERR_UNSUPPORTEDALPHAARG), N(D3DERR_TOOMANYOPERATIONS),
    N(D3DERR_CONFLICTINGTEXTUREFILTER), N(D3DERR_UNSUPPORTEDFACTORVALUE), N(D3DERR_CONFLICTINGRENDERSTATE),
    N(D3DERR_UNSUPPORTEDTEXTUREFILTER), N(D3DERR_CONFLICTINGTEXTUREPALETTE), N(D3DERR_DRIVERINTERNALERROR),
    N(D3DERR_NOTFOUND), N(D3DERR_MOREDATA), N(D3DERR_DEVICELOST), N(D3DERR_DEVICENOTRESET), N(D3DERR_NOTAVAILABLE),
    N(D3DERR_OUTOFVIDEOMEMORY), N(D3DERR_INVALIDDEVICE), N(D3DERR_INVALIDCALL), N(D3DERR_DRIVERINVALIDCALL),
    N(D3DERR_WASSTILLDRAWING), N(D3DERR_DEVICEREMOVED), N(D3DERR_DEVICEHUNG), N(D3DERR_UNSUPPORTEDOVERLAY),
    N(D3DERR_UNSUPPORTEDOVERLAYFORMAT), N(D3DERR_CANNOTPROTECTCONTENT), N(D3DERR_UNSUPPORTEDCRYPTO),
    N(D3DERR_PRESENT_STATISTICS_DISJOINT), N(S_PRESENT_OCCLUDED), N(S_PRESENT_MODE_CHANGED), N(S_NOT_RESIDENT), N(S_RESIDENT_IN_SHARED_MEMORY),
    N(DDERR_ALREADYINITIALIZED), N(DDERR_CANNOTATTACHSURFACE), N(DDERR_CANNOTDETACHSURFACE), N(DDERR_CURRENTLYNOTAVAIL),
    N(DDERR_EXCEPTION), N(DDERR_GENERIC), N(DDERR_HEIGHTALIGN), N(DDERR_INCOMPATIBLEPRIMARY), N(DDERR_INVALIDCAPS),
    N(DDERR_INVALIDCLIPLIST), N(DDERR_INVALIDMODE), N(DDERR_INVALIDOBJECT), N(DDERR_INVALIDPARAMS), N(DDERR_INVALIDPIXELFORMAT),
    N(DDERR_INVALIDRECT), N(DDERR_LOCKEDSURFACES), N(DDERR_NO3D), N(DDERR_NOALPHAHW), N(DDERR_NOCLIPLIST), N(DDERR_NOCOLORCONVHW),
    N(DDERR_NOCOOPERATIVELEVELSET), N(DDERR_NOCOLORKEY), N(DDERR_NOCOLORKEYHW), N(DDERR_NODIRECTDRAWSUPPORT), N(DDERR_NOEXCLUSIVEMODE),
    N(DDERR_NOFLIPHW), N(DDERR_NOGDI), N(DDERR_NOMIRRORHW), N(DDERR_NOTFOUND), N(DDERR_NOOVERLAYHW), N(DDERR_NORASTEROPHW),
    N(DDERR_NOROTATIONHW), N(DDERR_NOSTRETCHHW), N(DDERR_NOT4BITCOLOR), N(DDERR_NOT4BITCOLORINDEX), N(DDERR_NOT8BITCOLOR),
    N(DDERR_NOTEXTUREHW), N(DDERR_NOVSYNCHW), N(DDERR_NOZBUFFERHW), N(DDERR_NOZOVERLAYHW), N(DDERR_OUTOFCAPS), N(DDERR_OUTOFMEMORY),
    N(DDERR_OUTOFVIDEOMEMORY), N(DDERR_OVERLAYCANTCLIP), N(DDERR_OVERLAYCOLORKEYONLYONEACTIVE), N(DDERR_PALETTEBUSY),
    N(DDERR_COLORKEYNOTSET), N(DDERR_SURFACEALREADYATTACHED), N(DDERR_SURFACEALREADYDEPENDENT), N(DDERR_SURFACEBUSY),
    N(DDERR_CANTLOCKSURFACE), N(DDERR_SURFACEISOBSCURED), N(DDERR_SURFACELOST), N(DDERR_SURFACENOTATTACHED),
    N(DDERR_TOOBIGHEIGHT), N(DDERR_TOOBIGSIZE), N(DDERR_TOOBIGWIDTH), N(DDERR_UNSUPPORTED), N(DDERR_UNSUPPORTEDFORMAT),
    N(DDERR_UNSUPPORTEDMASK), N(DDERR_VERTICALBLANKINPROGRESS), N(DDERR_WASSTILLDRAWING), N(DDERR_WRONGMODE), N(DDERR_NOTLOCKED),
    N(DDERR_NOTFLIPPABLE), N(DDERR_NOTPALETTIZED), N(DDERR_NOTINITIALIZED), N(DDERR_NOPALETTEATTACHED), N(DDERR_NOPALETTEHW),
    N(DDERR_NOBLTHW), N(DDERR_NODDROPSHW), N(DDERR_NOEMULATION), N(DDERR_NOHWND), N(DDERR_EXCLUSIVEMODEALREADYSET),
    N(DDERR_PRIMARYSURFACEALREADYEXISTS), N(DDERR_NODC), N(DDERR_WRONGMODE),
    N(HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND)), N(HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND)),
    N(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER)), N(HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)),
    N(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)), N(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)),
    N(HRESULT_FROM_WIN32(ERROR_MORE_DATA)), N(HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW)),
};
#undef N
static char g_ring[4][64];
static int g_ri;
static char *ringbuf(void) { char *b = g_ring[g_ri]; g_ri = (g_ri + 1) & 3; return b; }
const char *d3dh_hr_name(HRESULT hr) {
    for (size_t i = 0; i < sizeof g_hr / sizeof g_hr[0]; i++) if (g_hr[i].hr == hr) return g_hr[i].n;
    char *b = ringbuf(); snprintf(b, 64, "0x%08lx", (unsigned long)(ULONG)hr); return b;
}

/* ---------------- GUIDs ---------------- */
#define G(x) { &x, #x }
static const struct { const GUID *g; const char *n; } g_guids[] = {
    G(IID_IUnknown), G(IID_IDXGIObject), G(IID_IDXGIDeviceSubObject), G(IID_IDXGIResource), G(IID_IDXGISurface),
    G(IID_IDXGIAdapter), G(IID_IDXGIOutput), G(IID_IDXGISwapChain), G(IID_IDXGIFactory), G(IID_IDXGIDevice),
    G(IID_IDXGIFactory1), G(IID_IDXGIAdapter1), G(IID_IDXGIDevice1), G(IID_IDXGIFactory2), G(IID_IDXGIFactory4),
    G(IID_ID3D11Device), G(IID_ID3D11DeviceContext), G(IID_ID3D11Texture2D), G(IID_ID3D11Buffer), G(IID_ID3D10Device),
    G(IID_ID3D10Device1), G(IID_ID3D12Device), G(IID_ID3D12Resource), G(IID_ID3D12CommandQueue), G(IID_ID3D12Fence),
    G(IID_ID3DBlob), G(IID_ID3D11ShaderReflection),
};
#undef G
const char *d3dh_guid_str(const GUID *g) {
    char *b = ringbuf();
    if (!g) { snprintf(b, 64, "(null)"); return b; }
    snprintf(b, 64, "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", g->Data1, g->Data2, g->Data3, g->Data4[0], g->Data4[1],
             g->Data4[2], g->Data4[3], g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
    for (size_t i = 0; i < sizeof g_guids / sizeof g_guids[0]; i++)
        if (IsEqualGUID(g_guids[i].g, g)) { size_t n = strlen(b); snprintf(b + n, 64 - n, " %.16s", g_guids[i].n); break; }
    return b;
}

/* ---------------- PRNG / hashes ---------------- */
void d3dh_rng_seed(d3dh_rng *r, uint64_t seed) { r->s = seed; }
static uint64_t rng64(d3dh_rng *r) {
    uint64_t z = (r->s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31);
}
uint32_t d3dh_rng_u32(d3dh_rng *r) { return (uint32_t)(rng64(r) >> 32); }
float d3dh_rng_float(d3dh_rng *r) { return (float)(d3dh_rng_u32(r) >> 8) / 16777216.0f; }
uint32_t d3dh_fnv1a(const void *data, size_t len) {
    const unsigned char *p = data; uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}
uint32_t d3dh_crc32(const void *data, size_t len) {
    static uint32_t tab[256];
    if (!tab[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1; tab[i] = c; }
    const unsigned char *p = data; uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) c = tab[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

/* ---------------- formats / images ---------------- */
UINT d3dh_format_bytes(DXGI_FORMAT f) {
    switch (f) {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS: case DXGI_FORMAT_R32G32B32A32_FLOAT: case DXGI_FORMAT_R32G32B32A32_UINT: case DXGI_FORMAT_R32G32B32A32_SINT: return 16;
    case DXGI_FORMAT_R32G32B32_TYPELESS: case DXGI_FORMAT_R32G32B32_FLOAT: case DXGI_FORMAT_R32G32B32_UINT: case DXGI_FORMAT_R32G32B32_SINT: return 12;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: case DXGI_FORMAT_R16G16B16A16_FLOAT: case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT: case DXGI_FORMAT_R16G16B16A16_SNORM: case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS: case DXGI_FORMAT_R32G32_FLOAT: case DXGI_FORMAT_R32G32_UINT: case DXGI_FORMAT_R32G32_SINT: return 8;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: case DXGI_FORMAT_R10G10B10A2_UNORM: case DXGI_FORMAT_R10G10B10A2_UINT: case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM: case DXGI_FORMAT_R8G8B8A8_SINT: case DXGI_FORMAT_R16G16_TYPELESS: case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM: case DXGI_FORMAT_R16G16_UINT: case DXGI_FORMAT_R16G16_SNORM: case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_TYPELESS: case DXGI_FORMAT_D32_FLOAT: case DXGI_FORMAT_R32_FLOAT: case DXGI_FORMAT_R32_UINT: case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R24G8_TYPELESS: case DXGI_FORMAT_D24_UNORM_S8_UINT: case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8X8_UNORM: case DXGI_FORMAT_B8G8R8A8_TYPELESS: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP: case DXGI_FORMAT_R8G8_B8G8_UNORM: case DXGI_FORMAT_G8R8_G8B8_UNORM: return 4;
    case DXGI_FORMAT_R8G8_TYPELESS: case DXGI_FORMAT_R8G8_UNORM: case DXGI_FORMAT_R8G8_UINT: case DXGI_FORMAT_R8G8_SNORM: case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_TYPELESS: case DXGI_FORMAT_R16_FLOAT: case DXGI_FORMAT_D16_UNORM: case DXGI_FORMAT_R16_UNORM: case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM: case DXGI_FORMAT_R16_SINT: case DXGI_FORMAT_B5G6R5_UNORM: case DXGI_FORMAT_B5G5R5A1_UNORM: case DXGI_FORMAT_B4G4R4A4_UNORM: return 2;
    case DXGI_FORMAT_R8_TYPELESS: case DXGI_FORMAT_R8_UNORM: case DXGI_FORMAT_R8_UINT: case DXGI_FORMAT_R8_SNORM: case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM: return 1;
    default: return 0;
    }
}
void d3dh_image_free(d3dh_image *im) { if (im) { free(im->data); memset(im, 0, sizeof *im); } }
uint32_t d3dh_image_hash(const d3dh_image *im) {
    UINT bpp = d3dh_format_bytes(im->format), rb = im->width * bpp; uint32_t h = 2166136261u;
    for (UINT y = 0; y < im->height; y++) for (UINT x = 0; x < rb; x++) { h ^= im->data[(size_t)y * im->pitch + x]; h *= 16777619u; }
    return h;
}
uint32_t d3dh_image_pixel32(const d3dh_image *im, UINT x, UINT y) { uint32_t v; memcpy(&v, im->data + (size_t)y * im->pitch + (size_t)x * 4, 4); return v; }

/* copy rows out of a mapped region into a tight image */
static HRESULT image_from_map(d3dh_image *out, const void *src, UINT srcpitch, UINT w, UINT h, DXGI_FORMAT f) {
    UINT bpp = d3dh_format_bytes(f);
    memset(out, 0, sizeof *out);
    if (!bpp) return E_NOTIMPL;
    out->width = w; out->height = h; out->format = f; out->pitch = w * bpp; out->size = (size_t)out->pitch * h;
    out->data = malloc(out->size ? out->size : 1);
    if (!out->data) return E_OUTOFMEMORY;
    for (UINT y = 0; y < h; y++) memcpy(out->data + (size_t)y * out->pitch, (const char *)src + (size_t)y * srcpitch, out->pitch);
    return S_OK;
}

/* ---------------- device creation ---------------- */
static const D3D_DRIVER_TYPE k_drv11[] = { D3D_DRIVER_TYPE_WARP, D3D_DRIVER_TYPE_REFERENCE, D3D_DRIVER_TYPE_NULL, D3D_DRIVER_TYPE_HARDWARE };
HRESULT d3dh_make_device11_fl(ID3D11Device **dev, ID3D11DeviceContext **ctx, const D3D_FEATURE_LEVEL *fls, UINT nfl, D3D_FEATURE_LEVEL *fl_out, D3D_DRIVER_TYPE *driver_out, UINT flags) {
    __typeof__(&D3D11CreateDevice) f = d3dh_D3D11CreateDevice();
    if (dev) *dev = NULL;
    if (ctx) *ctx = NULL;
    if (!f) return D3DH_E_NOCOMPILER;
    HRESULT hr = E_FAIL;
    for (int i = 0; i < 4; i++) {
        D3D_FEATURE_LEVEL fl = 0;
        hr = f(NULL, k_drv11[i], NULL, flags, fls, nfl, D3D11_SDK_VERSION, dev, &fl, ctx);
        if (SUCCEEDED(hr)) { if (fl_out) *fl_out = fl; if (driver_out) *driver_out = k_drv11[i]; return hr; }
    }
    return hr;
}
HRESULT d3dh_make_device11(ID3D11Device **dev, ID3D11DeviceContext **ctx, D3D_FEATURE_LEVEL *fl_out, D3D_DRIVER_TYPE *driver_out, UINT flags) {
    return d3dh_make_device11_fl(dev, ctx, NULL, 0, fl_out, driver_out, flags);
}
static const D3D10_DRIVER_TYPE k_drv10[] = { D3D10_DRIVER_TYPE_WARP, D3D10_DRIVER_TYPE_REFERENCE, D3D10_DRIVER_TYPE_NULL, D3D10_DRIVER_TYPE_HARDWARE };
HRESULT d3dh_make_device10(ID3D10Device **dev, D3D10_DRIVER_TYPE *driver_out, UINT flags) {
    __typeof__(&D3D10CreateDevice) f = d3dh_D3D10CreateDevice();
    if (dev) *dev = NULL;
    if (!f) return D3DH_E_NOCOMPILER;
    HRESULT hr = E_FAIL;
    for (int i = 0; i < 4; i++) {
        hr = f(NULL, k_drv10[i], NULL, flags, D3D10_SDK_VERSION, dev);
        if (SUCCEEDED(hr)) { if (driver_out) *driver_out = k_drv10[i]; return hr; }
    }
    return hr;
}
HRESULT d3dh_make_device10_1(ID3D10Device1 **dev, D3D10_FEATURE_LEVEL1 *fl_out, D3D10_DRIVER_TYPE *driver_out, UINT flags) {
    __typeof__(&D3D10CreateDevice1) f = d3dh_D3D10CreateDevice1();
    static const D3D10_FEATURE_LEVEL1 lv[] = { D3D10_FEATURE_LEVEL_10_1, D3D10_FEATURE_LEVEL_10_0 };
    if (dev) *dev = NULL;
    if (!f) return D3DH_E_NOCOMPILER;
    HRESULT hr = E_FAIL;
    for (int i = 0; i < 4; i++) for (int j = 0; j < 2; j++) {
        hr = f(NULL, k_drv10[i], NULL, flags, lv[j], D3D10_1_SDK_VERSION, dev);
        if (SUCCEEDED(hr)) { if (fl_out) *fl_out = lv[j]; if (driver_out) *driver_out = k_drv10[i]; return hr; }
    }
    return hr;
}
HRESULT d3dh_make_factory1(IDXGIFactory1 **f) {
    __typeof__(&CreateDXGIFactory1) c = d3dh_CreateDXGIFactory1();
    *f = NULL;
    return c ? c(&IID_IDXGIFactory1, (void **)f) : D3DH_E_NOCOMPILER;
}
HRESULT d3dh_make_factory2(IDXGIFactory2 **f) {
    __typeof__(&CreateDXGIFactory2) c2 = d3dh_CreateDXGIFactory2();
    *f = NULL;
    if (c2) { HRESULT hr = c2(0, &IID_IDXGIFactory2, (void **)f); if (SUCCEEDED(hr)) return hr; }
    IDXGIFactory1 *f1 = NULL; HRESULT hr = d3dh_make_factory1(&f1);
    if (FAILED(hr)) return hr;
    hr = IDXGIFactory1_QueryInterface(f1, &IID_IDXGIFactory2, (void **)f);
    IDXGIFactory1_Release(f1);
    return hr;
}
HRESULT d3dh_make_device12(ID3D12Device **dev, D3D_FEATURE_LEVEL min_fl, int *is_warp_out) {
    __typeof__(&D3D12CreateDevice) cd = d3dh_D3D12CreateDevice();
    if (dev) *dev = NULL;
    if (is_warp_out) *is_warp_out = 0;
    if (!cd) return D3DH_E_NOCOMPILER;
    HRESULT hr = E_FAIL;
    __typeof__(&CreateDXGIFactory1) cf = d3dh_CreateDXGIFactory1();
    IDXGIFactory4 *fac = NULL;
    if (cf && SUCCEEDED(cf(&IID_IDXGIFactory4, (void **)&fac)) && fac) {
        IDXGIAdapter *ad = NULL;
        if (SUCCEEDED(IDXGIFactory4_EnumWarpAdapter(fac, &IID_IDXGIAdapter, (void **)&ad)) && ad) {
            hr = cd((IUnknown *)ad, min_fl, &IID_ID3D12Device, (void **)dev);
            IDXGIAdapter_Release(ad);
            if (SUCCEEDED(hr)) { if (is_warp_out) *is_warp_out = 1; IDXGIFactory4_Release(fac); return hr; }
        }
        IDXGIFactory4_Release(fac);
    }
    return cd(NULL, min_fl, &IID_ID3D12Device, (void **)dev);
}
static void fill_pp(D3DPRESENT_PARAMETERS *pp, HWND hw) {
    memset(pp, 0, sizeof *pp);
    pp->Windowed = TRUE; pp->SwapEffect = D3DSWAPEFFECT_DISCARD; pp->BackBufferFormat = D3DFMT_UNKNOWN;
    pp->BackBufferWidth = 64; pp->BackBufferHeight = 64; pp->hDeviceWindow = hw;
}
HRESULT d3dh_make_device9(IDirect3D9 **d3d, IDirect3DDevice9 **dev, HWND hwnd, HWND *hwnd_out, D3DDEVTYPE *type_out) {
    __typeof__(&Direct3DCreate9) c = d3dh_Direct3DCreate9();
    static const D3DDEVTYPE ty[] = { D3DDEVTYPE_NULLREF, D3DDEVTYPE_HAL, D3DDEVTYPE_REF };
    *d3d = NULL; *dev = NULL;
    if (!c) return D3DH_E_NOCOMPILER;
    if (!hwnd) hwnd = d3dh_create_hidden_window();
    if (hwnd_out) *hwnd_out = hwnd;
    *d3d = c(D3D_SDK_VERSION);
    if (!*d3d) return D3DERR_NOTAVAILABLE;
    HRESULT hr = E_FAIL;
    for (int i = 0; i < 3; i++) {
        D3DPRESENT_PARAMETERS pp; fill_pp(&pp, hwnd);
        hr = IDirect3D9_CreateDevice(*d3d, D3DADAPTER_DEFAULT, ty[i], hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, dev);
        if (SUCCEEDED(hr)) { if (type_out) *type_out = ty[i]; return hr; }
    }
    return hr;
}
HRESULT d3dh_make_device9ex(IDirect3D9Ex **d3d, IDirect3DDevice9Ex **dev, HWND hwnd, HWND *hwnd_out, D3DDEVTYPE *type_out) {
    __typeof__(&Direct3DCreate9Ex) c = d3dh_Direct3DCreate9Ex();
    static const D3DDEVTYPE ty[] = { D3DDEVTYPE_NULLREF, D3DDEVTYPE_HAL, D3DDEVTYPE_REF };
    *d3d = NULL; *dev = NULL;
    if (!c) return D3DH_E_NOCOMPILER;
    if (!hwnd) hwnd = d3dh_create_hidden_window();
    if (hwnd_out) *hwnd_out = hwnd;
    HRESULT hr = c(D3D_SDK_VERSION, d3d);
    if (FAILED(hr)) return hr;
    for (int i = 0; i < 3; i++) {
        D3DPRESENT_PARAMETERS pp; fill_pp(&pp, hwnd);
        hr = IDirect3D9Ex_CreateDeviceEx(*d3d, D3DADAPTER_DEFAULT, ty[i], hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, NULL, dev);
        if (SUCCEEDED(hr)) { if (type_out) *type_out = ty[i]; return hr; }
    }
    return hr;
}

/* ---------------- shader compile ---------------- */
static char g_cerr[4096];
const char *d3dh_compile_errors(void) { return g_cerr; }
const void *d3dh_blob_ptr(ID3DBlob *b) { return b ? ID3D10Blob_GetBufferPointer(b) : NULL; }
SIZE_T d3dh_blob_size(ID3DBlob *b) { return b ? ID3D10Blob_GetBufferSize(b) : 0; }
HRESULT d3dh_compile_ex(const char *src, const char *srcname, const D3D_SHADER_MACRO *defs, const char *entry, const char *target, UINT f1, UINT f2, ID3DBlob **out) {
    __typeof__(&D3DCompile) c = d3dh_D3DCompile();
    ID3DBlob *err = NULL;
    g_cerr[0] = 0;
    *out = NULL;
    if (!c) return D3DH_E_NOCOMPILER;
    HRESULT hr = c(src, strlen(src), srcname, defs, NULL, entry, target, f1, f2, out, &err);
    if (err) {
        SIZE_T n = ID3D10Blob_GetBufferSize(err);
        if (n >= sizeof g_cerr) n = sizeof g_cerr - 1;
        memcpy(g_cerr, ID3D10Blob_GetBufferPointer(err), n); g_cerr[n] = 0;
        ID3D10Blob_Release(err);
    }
    return hr;
}
HRESULT d3dh_compile(const char *src, const char *entry, const char *target, UINT flags, ID3DBlob **out) {
    return d3dh_compile_ex(src, "d3dh_src", NULL, entry, target, flags, 0, out);
}

/* ---------------- readback: D3D11 ---------------- */
HRESULT d3dh_readback_texture2d11(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11Texture2D *src, UINT sub, d3dh_image *out) {
    D3D11_TEXTURE2D_DESC d; ID3D11Texture2D *st = NULL; D3D11_MAPPED_SUBRESOURCE m;
    ID3D11Texture2D_GetDesc(src, &d);
    UINT mip = sub % d.MipLevels, w = d.Width >> mip, h = d.Height >> mip;
    if (!w) w = 1;
    if (!h) h = 1;
    d.Width = w; d.Height = h; d.MipLevels = 1; d.ArraySize = 1; d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ; d.MiscFlags = 0;
    HRESULT hr = ID3D11Device_CreateTexture2D(dev, &d, NULL, &st);
    if (FAILED(hr)) return hr;
    ID3D11DeviceContext_CopySubresourceRegion(ctx, (ID3D11Resource *)st, 0, 0, 0, 0, (ID3D11Resource *)src, sub, NULL);
    hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &m);
    if (SUCCEEDED(hr)) { hr = image_from_map(out, m.pData, m.RowPitch, w, h, d.Format); ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)st, 0); }
    ID3D11Texture2D_Release(st);
    return hr;
}
HRESULT d3dh_readback_buffer11(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11Buffer *src, void **data, size_t *size) {
    D3D11_BUFFER_DESC d; ID3D11Buffer *st = NULL; D3D11_MAPPED_SUBRESOURCE m;
    ID3D11Buffer_GetDesc(src, &d);
    d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ; d.MiscFlags = 0;
    HRESULT hr = ID3D11Device_CreateBuffer(dev, &d, NULL, &st);
    if (FAILED(hr)) return hr;
    ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)st, (ID3D11Resource *)src);
    hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &m);
    if (SUCCEEDED(hr)) {
        *data = malloc(d.ByteWidth ? d.ByteWidth : 1); *size = d.ByteWidth;
        if (*data) memcpy(*data, m.pData, d.ByteWidth); else hr = E_OUTOFMEMORY;
        ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)st, 0);
    }
    ID3D11Buffer_Release(st);
    return hr;
}

/* ---------------- readback: D3D10 ---------------- */
HRESULT d3dh_readback_texture2d10(ID3D10Device *dev, ID3D10Texture2D *src, d3dh_image *out) {
    D3D10_TEXTURE2D_DESC d; ID3D10Texture2D *st = NULL; D3D10_MAPPED_TEXTURE2D m;
    ID3D10Texture2D_GetDesc(src, &d);
    d.MipLevels = 1; d.ArraySize = 1; d.Usage = D3D10_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D10_CPU_ACCESS_READ; d.MiscFlags = 0;
    HRESULT hr = ID3D10Device_CreateTexture2D(dev, &d, NULL, &st);
    if (FAILED(hr)) return hr;
    ID3D10Device_CopySubresourceRegion(dev, (ID3D10Resource *)st, 0, 0, 0, 0, (ID3D10Resource *)src, 0, NULL);
    hr = ID3D10Texture2D_Map(st, 0, D3D10_MAP_READ, 0, &m);
    if (SUCCEEDED(hr)) { hr = image_from_map(out, m.pData, m.RowPitch, d.Width, d.Height, d.Format); ID3D10Texture2D_Unmap(st, 0); }
    ID3D10Texture2D_Release(st);
    return hr;
}
HRESULT d3dh_readback_buffer10(ID3D10Device *dev, ID3D10Buffer *src, void **data, size_t *size) {
    D3D10_BUFFER_DESC d; ID3D10Buffer *st = NULL; void *p = NULL;
    ID3D10Buffer_GetDesc(src, &d);
    d.Usage = D3D10_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D10_CPU_ACCESS_READ; d.MiscFlags = 0;
    HRESULT hr = ID3D10Device_CreateBuffer(dev, &d, NULL, &st);
    if (FAILED(hr)) return hr;
    ID3D10Device_CopyResource(dev, (ID3D10Resource *)st, (ID3D10Resource *)src);
    hr = ID3D10Buffer_Map(st, D3D10_MAP_READ, 0, &p);
    if (SUCCEEDED(hr)) {
        *data = malloc(d.ByteWidth ? d.ByteWidth : 1); *size = d.ByteWidth;
        if (*data) memcpy(*data, p, d.ByteWidth); else hr = E_OUTOFMEMORY;
        ID3D10Buffer_Unmap(st);
    }
    ID3D10Buffer_Release(st);
    return hr;
}

/* ---------------- D3D12 ---------------- */
D3D12_RESOURCE_DESC d3dh_d12_resource_desc(ID3D12Resource *r) {
    /* x64 MS ABI: struct return uses a hidden pointer in the register after 'this' */
    D3D12_RESOURCE_DESC d; memset(&d, 0, sizeof d);
    ((void *(WINAPI *)(ID3D12Resource *, D3D12_RESOURCE_DESC *))r->lpVtbl->GetDesc)(r, &d);
    return d;
}
D3D12_CPU_DESCRIPTOR_HANDLE d3dh_d12_heap_cpu_start(ID3D12DescriptorHeap *h) {
    D3D12_CPU_DESCRIPTOR_HANDLE d; memset(&d, 0, sizeof d);
    ((void *(WINAPI *)(ID3D12DescriptorHeap *, D3D12_CPU_DESCRIPTOR_HANDLE *))h->lpVtbl->GetCPUDescriptorHandleForHeapStart)(h, &d);
    return d;
}
D3D12_GPU_DESCRIPTOR_HANDLE d3dh_d12_heap_gpu_start(ID3D12DescriptorHeap *h) {
    D3D12_GPU_DESCRIPTOR_HANDLE d; memset(&d, 0, sizeof d);
    ((void *(WINAPI *)(ID3D12DescriptorHeap *, D3D12_GPU_DESCRIPTOR_HANDLE *))h->lpVtbl->GetGPUDescriptorHandleForHeapStart)(h, &d);
    return d;
}
HRESULT d3dh_d12_flush(ID3D12Device *dev, ID3D12CommandQueue *q) {
    ID3D12Fence *fence = NULL;
    HRESULT hr = ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&fence);
    if (FAILED(hr)) return hr;
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    hr = ID3D12CommandQueue_Signal(q, fence, 1);
    if (SUCCEEDED(hr)) hr = ID3D12Fence_SetEventOnCompletion(fence, 1, ev);
    if (SUCCEEDED(hr) && WaitForSingleObject(ev, 30000) != WAIT_OBJECT_0) hr = HRESULT_FROM_WIN32(WAIT_TIMEOUT);
    CloseHandle(ev);
    ID3D12Fence_Release(fence);
    return hr;
}
/* Core: copy sub 0 of src (texture if tex!=NULL else buffer) into a readback buffer and map it. *blob receives malloc'd bytes. */
static HRESULT readback12(ID3D12Device *dev, ID3D12Resource *src, D3D12_RESOURCE_STATES before, int is_tex, d3dh_image *img, void **data, size_t *size) {
    D3D12_RESOURCE_DESC d = d3dh_d12_resource_desc(src);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT lay; UINT rows = 1; UINT64 rowsize = 0, total = 0;
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 1, 0, &lay, &rows, &rowsize, &total);
    if (!is_tex) total = d.Width;
    HRESULT hr;
    ID3D12Resource *rb = NULL; ID3D12CommandQueue *q = NULL; ID3D12CommandAllocator *al = NULL; ID3D12GraphicsCommandList *cl = NULL;
    D3D12_HEAP_PROPERTIES hp; memset(&hp, 0, sizeof hp); hp.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC bd; memset(&bd, 0, sizeof bd);
    bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = total ? total : 1; bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1;
    bd.Format = DXGI_FORMAT_UNKNOWN; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    hr = ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void **)&rb);
    if (FAILED(hr)) return hr;
    D3D12_COMMAND_QUEUE_DESC qd; memset(&qd, 0, sizeof qd); qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = ID3D12Device_CreateCommandQueue(dev, &qd, &IID_ID3D12CommandQueue, (void **)&q);
    if (SUCCEEDED(hr)) hr = ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&al);
    if (SUCCEEDED(hr)) hr = ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, al, NULL, &IID_ID3D12GraphicsCommandList, (void **)&cl);
    if (SUCCEEDED(hr)) {
        D3D12_RESOURCE_BARRIER b; memset(&b, 0, sizeof b);
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = src; b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = before; b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        if (before != D3D12_RESOURCE_STATE_COPY_SOURCE) ID3D12GraphicsCommandList_ResourceBarrier(cl, 1, &b);
        if (is_tex) {
            D3D12_TEXTURE_COPY_LOCATION dl, sl; memset(&dl, 0, sizeof dl); memset(&sl, 0, sizeof sl);
            dl.pResource = rb; dl.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dl.PlacedFootprint = lay;
            sl.pResource = src; sl.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; sl.SubresourceIndex = 0;
            ID3D12GraphicsCommandList_CopyTextureRegion(cl, &dl, 0, 0, 0, &sl, NULL);
        } else ID3D12GraphicsCommandList_CopyBufferRegion(cl, rb, 0, src, 0, d.Width);
        if (before != D3D12_RESOURCE_STATE_COPY_SOURCE) {
            b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE; b.Transition.StateAfter = before;
            ID3D12GraphicsCommandList_ResourceBarrier(cl, 1, &b);
        }
        hr = ID3D12GraphicsCommandList_Close(cl);
    }
    if (SUCCEEDED(hr)) {
        ID3D12CommandList *lists[1] = { (ID3D12CommandList *)cl };
        ID3D12CommandQueue_ExecuteCommandLists(q, 1, lists);
        hr = d3dh_d12_flush(dev, q);
    }
    if (SUCCEEDED(hr)) {
        void *p = NULL; D3D12_RANGE rr = { 0, (SIZE_T)total };
        hr = ID3D12Resource_Map(rb, 0, &rr, &p);
        if (SUCCEEDED(hr)) {
            if (is_tex) hr = image_from_map(img, (char *)p + lay.Offset, lay.Footprint.RowPitch, (UINT)d.Width, d.Height, d.Format);
            else { *size = (size_t)d.Width; *data = malloc(*size ? *size : 1); if (*data) memcpy(*data, p, *size); else hr = E_OUTOFMEMORY; }
            D3D12_RANGE wr = { 0, 0 };
            ID3D12Resource_Unmap(rb, 0, &wr);
        }
    }
    if (cl) ID3D12GraphicsCommandList_Release(cl);
    if (al) ID3D12CommandAllocator_Release(al);
    if (q) ID3D12CommandQueue_Release(q);
    ID3D12Resource_Release(rb);
    return hr;
}
HRESULT d3dh_readback_texture2d12(ID3D12Device *dev, ID3D12Resource *src, D3D12_RESOURCE_STATES st, d3dh_image *out) { return readback12(dev, src, st, 1, out, NULL, NULL); }
HRESULT d3dh_readback_buffer12(ID3D12Device *dev, ID3D12Resource *src, D3D12_RESOURCE_STATES st, void **data, size_t *size) { return readback12(dev, src, st, 0, NULL, data, size); }
