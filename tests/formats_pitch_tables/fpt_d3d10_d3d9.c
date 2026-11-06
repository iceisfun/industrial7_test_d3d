/* D3D10 staging pitches and D3D9 LockRect/LockBox pitches per format and width. */
#include "fpt.h"

/* ================================= D3D10 ================================== */
static UINT sup10(ID3D10Device *dev, UINT f)
{
    UINT s = 0;
    return SUCCEEDED(ID3D10Device_CheckFormatSupport(dev, (DXGI_FORMAT)f, &s)) ? s : 0;
}
static BYTE pat(UINT id, UINT z, UINT row, UINT i) { return (BYTE)(id * 29 + z * 13 + row * 7 + i * 3 + 5); }

static void run10_2d(UINT grp)
{
    ID3D10Device *dev = fpt_dev10(); UINT id, wi;
    if (!dev) { t_skip("no D3D10 device"); return; }
    for (id = 0; id < FPT_NFORMATS; id++) {
        const fpt_fmt *f = &fpt_formats[id]; char line[300]; int n, any = 0; UINT prev = 0, sup;
        if (f->group != grp) continue;
        sup = sup10(dev, id);
        n = snprintf(line, sizeof line, "d3d10 tex2d %s rowpitch by width:", f->name);
        for (wi = 0; wi < FPT_NW; wi++) {
            UINT w = fpt_widths[wi], h = fpt_is_bc(f) ? 8 : 5, rowb = fpt_row_bytes(f, w), rows = fpt_rows(f, h), r, i; D3D10_TEXTURE2D_DESC d = {0};
            ID3D10Texture2D *a = NULL, *b = NULL; D3D10_MAPPED_TEXTURE2D m; HRESULT hr;
            int must = (sup & D3D10_FORMAT_SUPPORT_TEXTURE2D) && !(fpt_is_bc(f) && (w % 4 || h % 4)) && !((f->group == G_422 || f->group == G_VPACK) && f->bw == 2 && (w & 1));
            d.Width = w; d.Height = h; d.MipLevels = 1; d.ArraySize = 1; d.Format = (DXGI_FORMAT)id; d.SampleDesc.Count = 1;
            d.Usage = D3D10_USAGE_STAGING; d.CPUAccessFlags = D3D10_CPU_ACCESS_READ | D3D10_CPU_ACCESS_WRITE;
            hr = ID3D10Device_CreateTexture2D(dev, &d, NULL, &a);
            if (SUCCEEDED(hr)) hr = ID3D10Device_CreateTexture2D(dev, &d, NULL, &b);
            if (FAILED(hr)) {
                if (must) CHECKF(0, "d3d10 %s w=%u staging creation %s", f->name, w, d3dh_hr_name(hr)); else CHECKF(hr == E_INVALIDARG, "d3d10 %s w=%u: staging creation %s want E_INVALIDARG (support 0x%x)", f->name, w, d3dh_hr_name(hr), sup);
                SAFE_RELEASE(a); SAFE_RELEASE(b); n += snprintf(line + n, sizeof line - n, " -"); continue;
            }
            any = 1;
            CHECKF(must, "d3d10 %s w=%u: staging creation succeeded where Win10 refuses it (support 0x%x)", f->name, w, sup);
            memset(&m, 0, sizeof m);
            hr = ID3D10Texture2D_Map(a, 0, D3D10_MAP_READ_WRITE, 0, &m);
            EXPECT_HR(hr, S_OK);
            if (SUCCEEDED(hr)) {
                CHECKF(m.RowPitch >= rowb, "d3d10 %s w=%u RowPitch %u < %u", f->name, w, m.RowPitch, rowb);
                CHECKF(!prev || m.RowPitch >= prev, "d3d10 %s pitch shrinks with width: %u after %u", f->name, m.RowPitch, prev);
                if (fpt_is_warp()) CHECKF(m.RowPitch == fpt_align(rowb, 4), "d3d10 %s w=%u RowPitch %u want %u (WARP: row bytes aligned to 4)", f->name, w, m.RowPitch, fpt_align(rowb, 4));
                prev = m.RowPitch;
                n += snprintf(line + n, sizeof line - n, " %u", m.RowPitch);
                for (r = 0; r < rows; r++) for (i = 0; i < rowb; i++) ((BYTE *)m.pData)[(size_t)r * m.RowPitch + i] = pat(id, 0, r, i);
                ID3D10Texture2D_Unmap(a, 0);
                ID3D10Device_CopyResource(dev, (ID3D10Resource *)b, (ID3D10Resource *)a);
                memset(&m, 0, sizeof m);
                hr = ID3D10Texture2D_Map(b, 0, D3D10_MAP_READ, 0, &m);
                EXPECT_HR(hr, S_OK);
                if (SUCCEEDED(hr)) {
                    int ok = 1; for (r = 0; r < rows; r++) for (i = 0; i < rowb; i++) if (((BYTE *)m.pData)[(size_t)r * m.RowPitch + i] != pat(id, 0, r, i)) ok = 0;
                    if (must) CHECKF(ok, "d3d10 %s w=%u CopyResource content mismatch", f->name, w); else if (!ok) t_note("d3d10 %s w=%u CopyResource content mismatch", f->name, w);
                    ID3D10Texture2D_Unmap(b, 0);
                }
            }
            SAFE_RELEASE(a); SAFE_RELEASE(b);
        }
        if (any) t_note("%s", line);
    }
}
static void run10_3d(UINT grp)
{
    ID3D10Device *dev = fpt_dev10(); UINT id, wi;
    if (!dev) { t_skip("no D3D10 device"); return; }
    for (id = 0; id < FPT_NFORMATS; id++) {
        const fpt_fmt *f = &fpt_formats[id]; UINT sup;
        if (f->group != grp) continue;
        sup = sup10(dev, id);
        for (wi = 0; wi < FPT_NW; wi++) {
            UINT w = fpt_widths[wi], h = fpt_is_bc(f) ? 8 : 5, dp = 3, rowb = fpt_row_bytes(f, w), rows = fpt_rows(f, h), z, r, i; D3D10_TEXTURE3D_DESC d = {0};
            ID3D10Texture3D *a = NULL, *b = NULL; D3D10_MAPPED_TEXTURE3D m; HRESULT hr;
            int must = ((f->group >= G_B8 && f->group <= G_B128) || fpt_is_bc(f)) && (sup & D3D10_FORMAT_SUPPORT_TEXTURE3D) && !(sup & D3D10_FORMAT_SUPPORT_DEPTH_STENCIL) && !(fpt_is_bc(f) && w % 4);
            d.Width = w; d.Height = h; d.Depth = dp; d.MipLevels = 1; d.Format = (DXGI_FORMAT)id;
            d.Usage = D3D10_USAGE_STAGING; d.CPUAccessFlags = D3D10_CPU_ACCESS_READ | D3D10_CPU_ACCESS_WRITE;
            hr = ID3D10Device_CreateTexture3D(dev, &d, NULL, &a);
            if (SUCCEEDED(hr)) hr = ID3D10Device_CreateTexture3D(dev, &d, NULL, &b);
            if (FAILED(hr)) { if (must) CHECKF(0, "d3d10 3D %s w=%u creation %s", f->name, w, d3dh_hr_name(hr)); SAFE_RELEASE(a); SAFE_RELEASE(b); continue; }
            memset(&m, 0, sizeof m);
            hr = ID3D10Texture3D_Map(a, 0, D3D10_MAP_READ_WRITE, 0, &m);
            EXPECT_HR(hr, S_OK);
            if (SUCCEEDED(hr)) {
                CHECKF(m.RowPitch >= rowb, "d3d10 3D %s w=%u RowPitch %u < %u", f->name, w, m.RowPitch, rowb);
                CHECKF(m.DepthPitch >= m.RowPitch * rows, "d3d10 3D %s w=%u DepthPitch %u < %u", f->name, w, m.DepthPitch, m.RowPitch * rows);
                for (z = 0; z < dp; z++) for (r = 0; r < rows; r++) for (i = 0; i < rowb; i++) ((BYTE *)m.pData)[(size_t)z * m.DepthPitch + (size_t)r * m.RowPitch + i] = pat(id, z, r, i);
                ID3D10Texture3D_Unmap(a, 0);
                ID3D10Device_CopyResource(dev, (ID3D10Resource *)b, (ID3D10Resource *)a);
                memset(&m, 0, sizeof m);
                hr = ID3D10Texture3D_Map(b, 0, D3D10_MAP_READ, 0, &m);
                EXPECT_HR(hr, S_OK);
                if (SUCCEEDED(hr)) {
                    int ok = 1; for (z = 0; z < dp; z++) for (r = 0; r < rows; r++) for (i = 0; i < rowb; i++) if (((BYTE *)m.pData)[(size_t)z * m.DepthPitch + (size_t)r * m.RowPitch + i] != pat(id, z, r, i)) ok = 0;
                    if (must) CHECKF(ok, "d3d10 3D %s w=%u content mismatch", f->name, w); else if (!ok) t_note("d3d10 3D %s w=%u content mismatch", f->name, w);
                    ID3D10Texture3D_Unmap(b, 0);
                }
            }
            SAFE_RELEASE(a); SAFE_RELEASE(b);
        }
    }
}
static void run10_mips(UINT grp)
{
    ID3D10Device *dev = fpt_dev10(); UINT id;
    if (!dev) { t_skip("no D3D10 device"); return; }
    for (id = 0; id < FPT_NFORMATS; id++) {
        const fpt_fmt *f = &fpt_formats[id]; enum { W = 64, H = 32, MIPS = 7, ARR = 3 };
        D3D10_TEXTURE2D_DESC d = {0}; ID3D10Texture2D *t = NULL; UINT a, m, first[MIPS] = {0}; HRESULT hr;
        if (f->group != grp) continue;
        d.Width = W; d.Height = H; d.MipLevels = MIPS; d.ArraySize = ARR; d.Format = (DXGI_FORMAT)id; d.SampleDesc.Count = 1;
        d.Usage = D3D10_USAGE_STAGING; d.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
        hr = ID3D10Device_CreateTexture2D(dev, &d, NULL, &t);
        if (FAILED(hr)) { if (((grp >= G_B8 && grp <= G_B128) || fpt_is_bc(f)) && (sup10(dev, id) & D3D10_FORMAT_SUPPORT_TEXTURE2D) && !(sup10(dev, id) & D3D10_FORMAT_SUPPORT_DEPTH_STENCIL)) CHECKF(0, "d3d10 %s mip/array creation %s", f->name, d3dh_hr_name(hr)); continue; }
        for (a = 0; a < ARR; a++) for (m = 0; m < MIPS; m++) {
            D3D10_MAPPED_TEXTURE2D ms; UINT sub = fpt_calc_sub(m, a, MIPS);
            memset(&ms, 0, sizeof ms);
            hr = ID3D10Texture2D_Map(t, sub, D3D10_MAP_READ, 0, &ms);
            EXPECT_HR(hr, S_OK);
            if (FAILED(hr)) continue;
            CHECKF(ms.RowPitch >= fpt_row_bytes(f, fpt_mip(W, m)), "d3d10 %s a=%u m=%u RowPitch %u", f->name, a, m, ms.RowPitch);
            if (a == 0) first[m] = ms.RowPitch; else CHECKF(ms.RowPitch == first[m], "d3d10 %s m=%u slice %u pitch %u vs %u", f->name, m, a, ms.RowPitch, first[m]);
            ID3D10Texture2D_Unmap(t, sub);
        }
        SAFE_RELEASE(t);
    }
}
#define T10(nm, grp) TEST(fpt_d3d10_tex2d_##nm) { run10_2d(grp); } TEST(fpt_d3d10_mipchain_##nm) { run10_mips(grp); }
T10(b8, G_B8) T10(b16, G_B16) T10(b32, G_B32) T10(b64, G_B64) T10(b96, G_B96) T10(b128, G_B128)
T10(bc1, G_BC1) T10(bc2, G_BC2) T10(bc3, G_BC3) T10(bc4, G_BC4) T10(bc5, G_BC5)
TEST(fpt_d3d10_tex2d_bc6h_bc7_observed) { run10_2d(G_BC6); run10_2d(G_BC7); }
TEST(fpt_d3d10_tex2d_video_observed) { run10_2d(G_VPACK); run10_2d(G_PAL); run10_2d(G_422); }
#define T10_3D(nm, grp) TEST(fpt_d3d10_tex3d_##nm) { run10_3d(grp); }
T10_3D(b8, G_B8) T10_3D(b16, G_B16) T10_3D(b32, G_B32) T10_3D(b64, G_B64) T10_3D(b128, G_B128) T10_3D(bc1, G_BC1) T10_3D(bc3, G_BC3) T10_3D(bc5, G_BC5)

TEST(fpt_d3d10_miplevels_zero_full_chain)
{
    ID3D10Device *dev = fpt_dev10(); UINT i; static const UINT sz[][2] = { {1,1}, {2,1}, {4,4}, {5,3}, {64,8}, {100,100}, {255,256} };
    if (!dev) { t_skip("no D3D10 device"); return; }
    for (i = 0; i < sizeof sz / sizeof *sz; i++) {
        D3D10_TEXTURE2D_DESC t = {0}, g = {0}; ID3D10Texture2D *tex = NULL; UINT mx = sz[i][0] > sz[i][1] ? sz[i][0] : sz[i][1], want = 1;
        while (mx > 1) { mx >>= 1; want++; }
        t.Width = sz[i][0]; t.Height = sz[i][1]; t.MipLevels = 0; t.ArraySize = 1; t.Format = DXGI_FORMAT_R8G8B8A8_UNORM; t.SampleDesc.Count = 1;
        t.Usage = D3D10_USAGE_DEFAULT; t.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET; t.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;
        EXPECT_HR(ID3D10Device_CreateTexture2D(dev, &t, NULL, &tex), S_OK);
        if (!tex) continue;
        ID3D10Texture2D_GetDesc(tex, &g);
        CHECKF(g.MipLevels == want, "d3d10 %ux%u MipLevels %u want %u", sz[i][0], sz[i][1], g.MipLevels, want);
        SAFE_RELEASE(tex);
    }
}
TEST(fpt_d3d10_limits)
{
    ID3D10Device *dev = fpt_dev10(); D3D10_TEXTURE2D_DESC t = {0}; ID3D10Texture2D *tex = NULL; HRESULT hr;
    if (!dev) { t_skip("no D3D10 device"); return; }
    CHECK(D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION == 8192 && D3D10_REQ_TEXTURE3D_U_V_OR_W_DIMENSION == 2048 && D3D10_REQ_TEXTURE1D_U_DIMENSION == 8192);
    CHECK(D3D10_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION == 512 && D3D10_REQ_MIP_LEVELS == 14 && D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT == 16 && D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT == 128);
    CHECK(D3D10_REQ_CONSTANT_BUFFER_ELEMENT_COUNT == 4096 && D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT == 8);
    t.MipLevels = 1; t.ArraySize = 1; t.Format = DXGI_FORMAT_R8_UNORM; t.SampleDesc.Count = 1; t.Usage = D3D10_USAGE_DEFAULT;
    t.Width = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION; t.Height = 1;
    hr = ID3D10Device_CreateTexture2D(dev, &t, NULL, &tex); EXPECT_HR(hr, S_OK); SAFE_RELEASE(tex);
    /* WARP does not enforce the 8192 limit of D3D10 hardware: widths up to 2^23 work, one more does not */
    if (fpt_is_warp()) {
        t.Width = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION + 1; hr = ID3D10Device_CreateTexture2D(dev, &t, NULL, &tex); EXPECT_HR(hr, S_OK); SAFE_RELEASE(tex);
        t.Width = 1u << 23; hr = ID3D10Device_CreateTexture2D(dev, &t, NULL, &tex); EXPECT_HR(hr, S_OK); SAFE_RELEASE(tex);
    }
    t.Width = (1u << 23) + 1;
    hr = ID3D10Device_CreateTexture2D(dev, &t, NULL, &tex); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(tex);
    t.Width = 1; t.ArraySize = D3D10_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
    hr = ID3D10Device_CreateTexture2D(dev, &t, NULL, &tex); EXPECT_HR(hr, S_OK); SAFE_RELEASE(tex);
    t.ArraySize = D3D10_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION + 1;
    hr = ID3D10Device_CreateTexture2D(dev, &t, NULL, &tex); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(tex);
}

/* ================================= D3D9 =================================== */
typedef struct { D3DFORMAT f; const char *name; UINT bpb, bw, bh; int block; } fmt9;
#define FCC(a, b, c, d) ((D3DFORMAT)MAKEFOURCC(a, b, c, d))
static const fmt9 t9[] = {
    { D3DFMT_R8G8B8, "R8G8B8", 3, 1, 1, 0 }, { D3DFMT_A8R8G8B8, "A8R8G8B8", 4, 1, 1, 0 }, { D3DFMT_X8R8G8B8, "X8R8G8B8", 4, 1, 1, 0 },
    { D3DFMT_R5G6B5, "R5G6B5", 2, 1, 1, 0 }, { D3DFMT_X1R5G5B5, "X1R5G5B5", 2, 1, 1, 0 }, { D3DFMT_A1R5G5B5, "A1R5G5B5", 2, 1, 1, 0 },
    { D3DFMT_A4R4G4B4, "A4R4G4B4", 2, 1, 1, 0 }, { D3DFMT_R3G3B2, "R3G3B2", 1, 1, 1, 0 }, { D3DFMT_A8, "A8", 1, 1, 1, 0 },
    { D3DFMT_A8R3G3B2, "A8R3G3B2", 2, 1, 1, 0 }, { D3DFMT_X4R4G4B4, "X4R4G4B4", 2, 1, 1, 0 }, { D3DFMT_A2B10G10R10, "A2B10G10R10", 4, 1, 1, 0 },
    { D3DFMT_A8B8G8R8, "A8B8G8R8", 4, 1, 1, 0 }, { D3DFMT_X8B8G8R8, "X8B8G8R8", 4, 1, 1, 0 }, { D3DFMT_G16R16, "G16R16", 4, 1, 1, 0 },
    { D3DFMT_A2R10G10B10, "A2R10G10B10", 4, 1, 1, 0 }, { D3DFMT_A16B16G16R16, "A16B16G16R16", 8, 1, 1, 0 },
    { D3DFMT_A8P8, "A8P8", 2, 1, 1, 0 }, { D3DFMT_P8, "P8", 1, 1, 1, 0 }, { D3DFMT_L8, "L8", 1, 1, 1, 0 }, { D3DFMT_A8L8, "A8L8", 2, 1, 1, 0 }, { D3DFMT_A4L4, "A4L4", 1, 1, 1, 0 },
    { D3DFMT_V8U8, "V8U8", 2, 1, 1, 0 }, { D3DFMT_L6V5U5, "L6V5U5", 2, 1, 1, 0 }, { D3DFMT_X8L8V8U8, "X8L8V8U8", 4, 1, 1, 0 }, { D3DFMT_Q8W8V8U8, "Q8W8V8U8", 4, 1, 1, 0 },
    { D3DFMT_V16U16, "V16U16", 4, 1, 1, 0 }, { D3DFMT_A2W10V10U10, "A2W10V10U10", 4, 1, 1, 0 },
    { D3DFMT_L16, "L16", 2, 1, 1, 0 }, { D3DFMT_Q16W16V16U16, "Q16W16V16U16", 8, 1, 1, 0 },
    { D3DFMT_R16F, "R16F", 2, 1, 1, 0 }, { D3DFMT_G16R16F, "G16R16F", 4, 1, 1, 0 }, { D3DFMT_A16B16G16R16F, "A16B16G16R16F", 8, 1, 1, 0 },
    { D3DFMT_R32F, "R32F", 4, 1, 1, 0 }, { D3DFMT_G32R32F, "G32R32F", 8, 1, 1, 0 }, { D3DFMT_A32B32G32R32F, "A32B32G32R32F", 16, 1, 1, 0 },
    { D3DFMT_A2B10G10R10_XR_BIAS, "A2B10G10R10_XR_BIAS", 4, 1, 1, 0 },
    { D3DFMT_DXT1, "DXT1", 8, 4, 4, 1 }, { D3DFMT_DXT2, "DXT2", 16, 4, 4, 1 }, { D3DFMT_DXT3, "DXT3", 16, 4, 4, 1 },
    { D3DFMT_DXT4, "DXT4", 16, 4, 4, 1 }, { D3DFMT_DXT5, "DXT5", 16, 4, 4, 1 },
    { D3DFMT_UYVY, "UYVY", 4, 2, 1, 2 }, { D3DFMT_YUY2, "YUY2", 4, 2, 1, 2 }, { D3DFMT_R8G8_B8G8, "R8G8_B8G8", 4, 2, 1, 2 }, { D3DFMT_G8R8_G8B8, "G8R8_G8B8", 4, 2, 1, 2 },
};
#define N9 (sizeof t9 / sizeof *t9)
static UINT row9(const fmt9 *f, UINT w) { return fpt_ceil_div(w, f->bw) * f->bpb; }
static UINT rows9(const fmt9 *f, UINT h) { return fpt_ceil_div(h, f->bh); }

static IDirect3DDevice9 *dev9(void)
{
    IDirect3DDevice9 *d = fpt_dev9(NULL);
    if (!d) t_skip("no D3D9 device");
    return d;
}
/* Whether the runtime exposes the format as a texture (query through the device's own creation parameters). */
static int fmt_ok9(IDirect3DDevice9 *dev, D3DFORMAT f, D3DRESOURCETYPE rt)
{
    D3DDEVICE_CREATION_PARAMETERS cp; IDirect3D9 *d3d = NULL; HRESULT hr; int ok;
    if (FAILED(IDirect3DDevice9_GetCreationParameters(dev, &cp))) return 0;
    if (FAILED(IDirect3DDevice9_GetDirect3D(dev, &d3d))) return 0;
    hr = IDirect3D9_CheckDeviceFormat(d3d, cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, 0, rt, f);
    ok = SUCCEEDED(hr);
    IDirect3D9_Release(d3d);
    return ok;
}

static void run9_tex(int blockclass)
{
    IDirect3DDevice9 *dev = dev9(); UINT i, wi;
    if (!dev) return;
    for (i = 0; i < N9; i++) {
        const fmt9 *f = &t9[i]; char line[400]; int n, any = 0, supported; UINT prev = 0;
        if (f->block != blockclass) continue;
        supported = fmt_ok9(dev, f->f, D3DRTYPE_TEXTURE);
        n = snprintf(line, sizeof line, "d3d9 sysmem %s (CheckDeviceFormat %s) pitch by width:", f->name, supported ? "ok" : "no");
        for (wi = 0; wi < FPT_NW; wi++) {
            UINT w = fpt_widths[wi], h = f->block == 1 ? 8 : 5, r, k; IDirect3DTexture9 *t = NULL; D3DLOCKED_RECT lr, lr2; D3DSURFACE_DESC sd; HRESULT hr;
            int must = supported && !(f->block == 1 && (w % 4 || h % 4));
            hr = IDirect3DDevice9_CreateTexture(dev, w, h, 1, 0, f->f, D3DPOOL_SYSTEMMEM, &t, NULL);
            if (FAILED(hr)) {
                if (must) CHECKF(0, "d3d9 %s %ux%u sysmem creation %s", f->name, w, h, d3dh_hr_name(hr));
                else CHECKF(hr == D3DERR_INVALIDCALL, "d3d9 %s %ux%u sysmem creation: %s want D3DERR_INVALIDCALL", f->name, w, h, d3dh_hr_name(hr));
                n += snprintf(line + n, sizeof line - n, " -"); continue;
            }
            any = 1;
            CHECKF(must, "d3d9 %s %ux%u: sysmem creation succeeded where Win10 refuses it", f->name, w, h);
            memset(&sd, 0, sizeof sd);
            EXPECT_HR(IDirect3DTexture9_GetLevelDesc(t, 0, &sd), D3D_OK);
            CHECKF(sd.Width == w && sd.Height == h && sd.Format == f->f && sd.Pool == D3DPOOL_SYSTEMMEM, "d3d9 %s level desc %ux%u fmt %u pool %u", f->name, sd.Width, sd.Height, sd.Format, sd.Pool);
            CHECK(IDirect3DTexture9_GetLevelCount(t) == 1);
            memset(&lr, 0, sizeof lr);
            hr = IDirect3DTexture9_LockRect(t, 0, &lr, NULL, 0);
            EXPECT_HR(hr, D3D_OK);
            if (SUCCEEDED(hr)) {
                CHECKF(lr.pBits != NULL, "d3d9 %s pBits NULL", f->name);
                CHECKF((UINT)lr.Pitch >= row9(f, w), "d3d9 %s w=%u Pitch %d < %u", f->name, w, lr.Pitch, row9(f, w));
                CHECKF(!prev || (UINT)lr.Pitch >= prev, "d3d9 %s pitch shrinks with width: %d after %u", f->name, lr.Pitch, prev);
                CHECKF((UINT)lr.Pitch == fpt_align(row9(f, w), 4), "d3d9 %s w=%u Pitch %d want %u (system-memory textures align rows to 4 bytes)", f->name, w, lr.Pitch, fpt_align(row9(f, w), 4));
                prev = lr.Pitch;
                n += snprintf(line + n, sizeof line - n, " %d", lr.Pitch);
                for (r = 0; r < rows9(f, h); r++) for (k = 0; k < row9(f, w); k++) ((BYTE *)lr.pBits)[(size_t)r * lr.Pitch + k] = (BYTE)(i * 11 + r * 5 + k + 1);
                EXPECT_HR(IDirect3DTexture9_UnlockRect(t, 0), D3D_OK);
                memset(&lr2, 0, sizeof lr2);
                hr = IDirect3DTexture9_LockRect(t, 0, &lr2, NULL, D3DLOCK_READONLY);
                EXPECT_HR(hr, D3D_OK);
                if (SUCCEEDED(hr)) {
                    int ok = 1;
                    CHECKF(lr2.Pitch == lr.Pitch, "d3d9 %s w=%u pitch changed between locks (%d vs %d)", f->name, w, lr.Pitch, lr2.Pitch);
                    for (r = 0; r < rows9(f, h); r++) for (k = 0; k < row9(f, w); k++) if (((BYTE *)lr2.pBits)[(size_t)r * lr2.Pitch + k] != (BYTE)(i * 11 + r * 5 + k + 1)) ok = 0;
                    CHECKF(ok, "d3d9 %s w=%u content lost between locks", f->name, w);
                    IDirect3DTexture9_UnlockRect(t, 0);
                }
            }
            IDirect3DTexture9_Release(t);
        }
        if (any) t_note("%s", line);
    }
}
TEST(fpt_d3d9_texture_pitch_uncompressed) { run9_tex(0); }
TEST(fpt_d3d9_texture_pitch_dxt) { run9_tex(1); }
TEST(fpt_d3d9_texture_pitch_packed_yuv) { run9_tex(2); }

/* DXT textures need dimensions that are multiples of 4 (system memory pool included). */
TEST(fpt_d3d9_dxt_non_multiple_of_four)
{
    IDirect3DDevice9 *dev = dev9(); static const UINT sz[][2] = { {1,1}, {2,2}, {3,3}, {4,3}, {3,4}, {5,5}, {7,9}, {15,17}, {4,4}, {8,4}, {4,8}, {12,16} }; UINT i, f;
    static const D3DFORMAT fm[] = { D3DFMT_DXT1, D3DFMT_DXT3, D3DFMT_DXT5 };
    if (!dev) return;
    for (f = 0; f < 3; f++) for (i = 0; i < sizeof sz / sizeof *sz; i++) {
        IDirect3DTexture9 *t = NULL; HRESULT hr = IDirect3DDevice9_CreateTexture(dev, sz[i][0], sz[i][1], 1, 0, fm[f], D3DPOOL_SYSTEMMEM, &t, NULL);
        D3DLOCKED_RECT lr = {0}; int good = !(sz[i][0] % 4) && !(sz[i][1] % 4);
        CHECKF(hr == (good ? D3D_OK : D3DERR_INVALIDCALL), "fmt %u %ux%u sysmem: %s", fm[f], sz[i][0], sz[i][1], d3dh_hr_name(hr));
        if (good && t && SUCCEEDED(IDirect3DTexture9_LockRect(t, 0, &lr, NULL, 0))) {
            UINT bb = fm[f] == D3DFMT_DXT1 ? 8 : 16;
            CHECKF((UINT)lr.Pitch == sz[i][0] / 4 * bb, "fmt %u %ux%u Pitch %d", fm[f], sz[i][0], sz[i][1], lr.Pitch);
            IDirect3DTexture9_UnlockRect(t, 0);
        }
        if (t) IDirect3DTexture9_Release(t);
    }
}
TEST(fpt_d3d9_texture_levels_zero_full_chain)
{
    IDirect3DDevice9 *dev = dev9(); static const UINT sz[][2] = { {1,1}, {2,1}, {4,4}, {5,3}, {16,16}, {64,8}, {100,100}, {255,256} }; UINT i;
    if (!dev) return;
    for (i = 0; i < sizeof sz / sizeof *sz; i++) {
        IDirect3DTexture9 *t = NULL; UINT mx = sz[i][0] > sz[i][1] ? sz[i][0] : sz[i][1], want = 1, m;
        while (mx > 1) { mx >>= 1; want++; }
        EXPECT_HR(IDirect3DDevice9_CreateTexture(dev, sz[i][0], sz[i][1], 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &t, NULL), D3D_OK);
        if (!t) continue;
        CHECKF(IDirect3DTexture9_GetLevelCount(t) == want, "d3d9 %ux%u levels %u want %u", sz[i][0], sz[i][1], IDirect3DTexture9_GetLevelCount(t), want);
        for (m = 0; m < want; m++) {
            D3DSURFACE_DESC sd; D3DLOCKED_RECT lr = {0};
            memset(&sd, 0, sizeof sd);
            EXPECT_HR(IDirect3DTexture9_GetLevelDesc(t, m, &sd), D3D_OK);
            CHECKF(sd.Width == fpt_mip(sz[i][0], m) && sd.Height == fpt_mip(sz[i][1], m), "d3d9 level %u desc %ux%u", m, sd.Width, sd.Height);
            if (SUCCEEDED(IDirect3DTexture9_LockRect(t, m, &lr, NULL, 0))) {
                CHECKF((UINT)lr.Pitch >= 4 * sd.Width, "d3d9 level %u pitch %d", m, lr.Pitch);
                IDirect3DTexture9_UnlockRect(t, m);
            }
        }
        IDirect3DTexture9_Release(t);
    }
}
TEST(fpt_d3d9_dxt_mip_chain_pitch)
{
    IDirect3DDevice9 *dev = dev9(); UINT i, m;
    static const D3DFORMAT fm[] = { D3DFMT_DXT1, D3DFMT_DXT3, D3DFMT_DXT5 };
    if (!dev) return;
    for (i = 0; i < 3; i++) {
        IDirect3DTexture9 *t = NULL; UINT bytes = fm[i] == D3DFMT_DXT1 ? 8 : 16; HRESULT hr;
        if (!fmt_ok9(dev, fm[i], D3DRTYPE_TEXTURE)) { t_note("DXT format %u not available", fm[i]); continue; }
        hr = IDirect3DDevice9_CreateTexture(dev, 64, 64, 0, 0, fm[i], D3DPOOL_SYSTEMMEM, &t, NULL);
        EXPECT_HR(hr, D3D_OK);
        if (!t) continue;
        for (m = 0; m < IDirect3DTexture9_GetLevelCount(t); m++) {
            D3DLOCKED_RECT lr = {0}; UINT w = fpt_mip(64, m);
            hr = IDirect3DTexture9_LockRect(t, m, &lr, NULL, D3DLOCK_READONLY);
            EXPECT_HR(hr, D3D_OK);
            if (SUCCEEDED(hr)) {
                CHECKF((UINT)lr.Pitch >= fpt_ceil_div(w, 4) * bytes, "DXT mip %u (width %u) pitch %d < %u", m, w, lr.Pitch, fpt_ceil_div(w, 4) * bytes);
                IDirect3DTexture9_UnlockRect(t, m);
            }
        }
        IDirect3DTexture9_Release(t);
    }
}
TEST(fpt_d3d9_volume_texture_pitch)
{
    IDirect3DDevice9 *dev = dev9(); UINT i, wi;
    static const D3DFORMAT fm[] = { D3DFMT_A8R8G8B8, D3DFMT_L8, D3DFMT_A16B16G16R16F, D3DFMT_A32B32G32R32F, D3DFMT_DXT1, D3DFMT_DXT5 };
    if (!dev) return;
    for (i = 0; i < sizeof fm / sizeof *fm; i++) {
        const fmt9 *f = NULL; UINT k;
        for (k = 0; k < N9; k++) if (t9[k].f == fm[i]) f = &t9[k];
        if (!fmt_ok9(dev, fm[i], D3DRTYPE_VOLUMETEXTURE)) { t_note("volume format %s not available", f->name); continue; }
        for (wi = 0; wi < FPT_NW; wi++) {
            UINT w = fpt_widths[wi], h = 8, d = 3; IDirect3DVolumeTexture9 *t = NULL; D3DLOCKED_BOX lb; HRESULT hr;
            if (f->block == 1 && w % 4) continue;
            hr = IDirect3DDevice9_CreateVolumeTexture(dev, w, h, d, 1, 0, fm[i], D3DPOOL_SYSTEMMEM, &t, NULL);
            if (FAILED(hr)) { t_note("d3d9 volume %s w=%u: %s", f->name, w, d3dh_hr_name(hr)); continue; }
            memset(&lb, 0, sizeof lb);
            hr = IDirect3DVolumeTexture9_LockBox(t, 0, &lb, NULL, 0);
            EXPECT_HR(hr, D3D_OK);
            if (SUCCEEDED(hr)) {
                CHECKF((UINT)lb.RowPitch >= row9(f, w), "d3d9 volume %s w=%u RowPitch %d < %u", f->name, w, lb.RowPitch, row9(f, w));
                CHECKF((UINT)lb.SlicePitch >= (UINT)lb.RowPitch * rows9(f, h), "d3d9 volume %s w=%u SlicePitch %d < %u", f->name, w, lb.SlicePitch, lb.RowPitch * rows9(f, h));
                IDirect3DVolumeTexture9_UnlockBox(t, 0);
            }
            IDirect3DVolumeTexture9_Release(t);
        }
    }
}
TEST(fpt_d3d9_cube_texture_pitch)
{
    IDirect3DDevice9 *dev = dev9(); UINT i, face;
    static const D3DFORMAT fm[] = { D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_DXT1, D3DFMT_DXT5, D3DFMT_A16B16G16R16F };
    if (!dev) return;
    for (i = 0; i < sizeof fm / sizeof *fm; i++) {
        IDirect3DCubeTexture9 *t = NULL; HRESULT hr; const fmt9 *f = NULL; UINT k, prev = 0;
        for (k = 0; k < N9; k++) if (t9[k].f == fm[i]) f = &t9[k];
        if (!fmt_ok9(dev, fm[i], D3DRTYPE_CUBETEXTURE)) { t_note("cube format %s not available", f->name); continue; }
        hr = IDirect3DDevice9_CreateCubeTexture(dev, 16, 1, 0, fm[i], D3DPOOL_SYSTEMMEM, &t, NULL);
        EXPECT_HR(hr, D3D_OK);
        if (!t) continue;
        for (face = 0; face < 6; face++) {
            D3DLOCKED_RECT lr = {0};
            hr = IDirect3DCubeTexture9_LockRect(t, (D3DCUBEMAP_FACES)face, 0, &lr, NULL, 0);
            EXPECT_HR(hr, D3D_OK);
            if (SUCCEEDED(hr)) {
                CHECKF((UINT)lr.Pitch >= row9(f, 16), "d3d9 cube %s face %u pitch %d", f->name, face, lr.Pitch);
                if (prev) CHECKF((UINT)lr.Pitch == prev, "d3d9 cube %s face %u pitch %d differs from face 0 (%u)", f->name, face, lr.Pitch, prev);
                prev = lr.Pitch;
                IDirect3DCubeTexture9_UnlockRect(t, (D3DCUBEMAP_FACES)face, 0);
            }
        }
        IDirect3DCubeTexture9_Release(t);
    }
}
static IDirect3DDevice9 *g9n;
static void ct9_null(void *c) { IDirect3DDevice9_CreateTexture(g9n, 16, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, NULL, NULL); }
TEST(fpt_d3d9_texture_size_validation)
{
    IDirect3DDevice9 *dev = dev9(); IDirect3DTexture9 *t = NULL; HRESULT hr;
    if (!dev) return;
    hr = IDirect3DDevice9_CreateTexture(dev, 0, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &t, NULL); EXPECT_HR(hr, D3DERR_INVALIDCALL); CHECK(t == NULL);
    hr = IDirect3DDevice9_CreateTexture(dev, 16, 0, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &t, NULL); EXPECT_HR(hr, D3DERR_INVALIDCALL); CHECK(t == NULL);
    hr = IDirect3DDevice9_CreateTexture(dev, 16, 16, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_SYSTEMMEM, &t, NULL); EXPECT_HR_FAILED(hr); CHECK(t == NULL);
    /* a NULL ppTexture is dereferenced without validation: access violation, not D3DERR_INVALIDCALL */
    g9n = dev; CHECK(t_try(ct9_null, NULL) == 0xC0000005u);
}
