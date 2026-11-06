/* Additional contract tests: usage-bit table, identifier flag table, validator state machine, Ex display mode details. */
#include "d9c.h"

#define N(a) (sizeof(a) / sizeof(*(a)))

/* ---- GetAdapterIdentifier: flags accepted are exactly the subset of {0x2, 0x4} ---- */
TEST(d3d9_core_x_identifier_flag_bit_table) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (unsigned f = 0; f < 16; f++) {
        D3DADAPTER_IDENTIFIER9 id; memset(&id, 0, sizeof id);
        HRESULT hr = IDirect3D9_GetAdapterIdentifier(d, 0, f, &id);
        int ok = (f & ~6u) == 0;
        CHECKF(ok ? hr == S_OK : hr == D9_INVALIDCALL, "flags 0x%x -> %s", f, d3dh_hr_name(hr));
    }
    for (int b = 3; b < 32; b++) {
        D3DADAPTER_IDENTIFIER9 id;
        HRESULT hr = IDirect3D9_GetAdapterIdentifier(d, 0, 1u << b, &id);
        CHECKF(hr == D9_INVALIDCALL, "bit %d -> %s", b, d3dh_hr_name(hr));
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_x_identifier_flag_whql_constant) { EXPECT_EQ(D3DENUM_WHQL_LEVEL, 2); }
TEST(d3d9_core_x_identifier_struct_size) {
    EXPECT_EQ(sizeof(D3DADAPTER_IDENTIFIER9), 0x450);
    EXPECT_EQ(offsetof(D3DADAPTER_IDENTIFIER9, DeviceName), 0x400);
}
TEST(d3d9_core_x_identifier_ex_bad_flags) {
    D9_GET_EX(d);
    D3DADAPTER_IDENTIFIER9 id;
    EXPECT_HR(IDirect3D9Ex_GetAdapterIdentifier(d, 0, 1, &id), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9Ex_GetAdapterIdentifier(d, 0, 8, &id), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9Ex_GetAdapterIdentifier(d, IDirect3D9Ex_GetAdapterCount(d), 0, &id), D9_INVALIDCALL);
    IDirect3D9Ex_Release(d);
}

/* ---- CheckDeviceFormat: usage-bit validation happens before any driver query ---- */
TEST(d3d9_core_x_cdf_invalid_usage_bit_each) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    /* Bits 2, 22, 24-27 and 29-31 are never valid usage bits. */
    static const int bits[] = { 2, 22, 24, 25, 26, 27, 29, 30, 31 };
    D3DRESOURCETYPE rts[] = { D3DRTYPE_TEXTURE, D3DRTYPE_CUBETEXTURE };
    for (unsigned r = 0; r < 2; r++)
        for (unsigned i = 0; i < N(bits); i++) {
            HRESULT hr = IDirect3D9_CheckDeviceFormat(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, 1u << bits[i], rts[r], D3DFMT_A8R8G8B8);
            CHECKF(hr == D9_INVALIDCALL, "rtype %u usage bit %d -> %s", rts[r], bits[i], d3dh_hr_name(hr));
            hr = IDirect3D9_CheckDeviceFormat(d, 0, D3DDEVTYPE_REF, D3DFMT_X8R8G8B8, 1u << bits[i], rts[r], D3DFMT_A8R8G8B8);
            CHECKF(hr == D9_INVALIDCALL, "REF rtype %u usage bit %d -> %s", rts[r], bits[i], d3dh_hr_name(hr));
        }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_x_cdf_depthstencil_nonz_on_surface_and_volume) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DFORMAT nz[] = { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_DXT1 };
    D3DRESOURCETYPE rts[] = { D3DRTYPE_SURFACE, D3DRTYPE_TEXTURE, D3DRTYPE_CUBETEXTURE };
    for (unsigned r = 0; r < N(rts); r++)
        for (unsigned i = 0; i < N(nz); i++) {
            HRESULT hr = IDirect3D9_CheckDeviceFormat(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, rts[r], nz[i]);
            CHECKF(hr == D9_INVALIDCALL, "rtype %u fmt 0x%x -> %s", rts[r], nz[i], d3dh_hr_name(hr));
        }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_x_cdf_binarybuffer_only_plain_surface) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DRESOURCETYPE rts[] = { D3DRTYPE_TEXTURE, D3DRTYPE_CUBETEXTURE };
    for (unsigned r = 0; r < 2; r++) {
        HRESULT hr = IDirect3D9_CheckDeviceFormat(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, 0, rts[r], D3DFMT_BINARYBUFFER);
        CHECKF(hr == D9_INVALIDCALL, "rtype %u -> %s", rts[r], d3dh_hr_name(hr));
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_x_cdf_multielement_autogen_invalid) {
    D9_GET(d);
    if (!d9_have_devtype(d, D3DDEVTYPE_HAL)) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    EXPECT_HR(IDirect3D9_CheckDeviceFormat(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_AUTOGENMIPMAP, D3DRTYPE_TEXTURE, D3DFMT_MULTI2_ARGB8), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_x_cdf_adapter_format_checked_by_devtype) {
    /* A non-display adapter format is NOTAVAILABLE for every real device type; only NULLREF skips that test. */
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DDEVTYPE tys[] = { D3DDEVTYPE_HAL, D3DDEVTYPE_REF, D3DDEVTYPE_SW };
    for (unsigned t = 0; t < 3; t++)
        CHECKF(IDirect3D9_CheckDeviceFormat(d, 0, tys[t], D3DFMT_D24S8, 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8) == D9_NOTAVAILABLE, "devtype %u", tys[t]);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_x_cdf_ex_same_validation) {
    D9_GET_EX(d);
    if (!IDirect3D9Ex_GetAdapterCount(d)) { IDirect3D9Ex_Release(d); t_skip("no adapters"); return; }
    UINT n = IDirect3D9Ex_GetAdapterCount(d);
    EXPECT_HR(IDirect3D9Ex_CheckDeviceFormat(d, n, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9Ex_CheckDeviceFormat(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9Ex_CheckDeviceFormat(d, 0, (D3DDEVTYPE)9, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8), D9_INVALIDCALL);
    IDirect3D9Ex_Release(d);
}

/* ---- CheckDeviceType / MultiSample on the Ex object ---- */
TEST(d3d9_core_x_cdt_ex_validation) {
    D9_GET_EX(d);
    UINT n = IDirect3D9Ex_GetAdapterCount(d);
    EXPECT_HR(IDirect3D9Ex_CheckDeviceType(d, n, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE), D9_INVALIDCALL);
    if (n) {
        EXPECT_HR(IDirect3D9Ex_CheckDeviceType(d, 0, D3DDEVTYPE_NULLREF, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE), D9_INVALIDCALL);
        EXPECT_HR(IDirect3D9Ex_CheckDeviceType(d, 0, D3DDEVTYPE_HAL, D3DFMT_UNKNOWN, D3DFMT_X8R8G8B8, TRUE), D9_INVALIDCALL);
        EXPECT_HR(IDirect3D9Ex_CheckDeviceType(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_UNKNOWN, FALSE), D9_INVALIDCALL);
    }
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_x_cdms_ex_validation) {
    D9_GET_EX(d);
    UINT n = IDirect3D9Ex_GetAdapterCount(d);
    DWORD q = 77;
    EXPECT_HR(IDirect3D9Ex_CheckDeviceMultiSampleType(d, n, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_NONE, &q), D9_INVALIDCALL);
    if (n) {
        EXPECT_HR(IDirect3D9Ex_CheckDeviceMultiSampleType(d, 0, (D3DDEVTYPE)7, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_NONE, &q), D9_INVALIDCALL);
        EXPECT_HR(IDirect3D9Ex_CheckDeviceMultiSampleType(d, 0, D3DDEVTYPE_HAL, D3DFMT_UNKNOWN, TRUE, D3DMULTISAMPLE_NONE, &q), D9_INVALIDCALL);
    }
    EXPECT_EQ(q, 77);
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_x_cdms_windowed_flag_monotonic_note) {
    D9_GET(d);
    if (!d9_have_devtype(d, D3DDEVTYPE_HAL)) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    for (UINT s = 2; s <= 8; s++) {
        DWORD qw = 0, qf = 0;
        HRESULT hw = IDirect3D9_CheckDeviceMultiSampleType(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, (D3DMULTISAMPLE_TYPE)s, &qw);
        HRESULT hf = IDirect3D9_CheckDeviceMultiSampleType(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, FALSE, (D3DMULTISAMPLE_TYPE)s, &qf);
        t_note("X8R8G8B8 %ux: windowed %s/%lu fullscreen %s/%lu", s, d3dh_hr_name(hw), qw, d3dh_hr_name(hf), qf);
    }
    IDirect3D9_Release(d);
}

/* ---- CheckDepthStencilMatch / CheckDeviceFormatConversion: Ex ---- */
TEST(d3d9_core_x_cdsm_ex_validation) {
    D9_GET_EX(d);
    UINT n = IDirect3D9Ex_GetAdapterCount(d);
    EXPECT_HR(IDirect3D9Ex_CheckDepthStencilMatch(d, n, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_D24S8), D9_INVALIDCALL);
    if (n) {
        EXPECT_HR(IDirect3D9Ex_CheckDepthStencilMatch(d, 0, D3DDEVTYPE_NULLREF, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_D24S8), D9_INVALIDCALL);
        EXPECT_HR(IDirect3D9Ex_CheckDepthStencilMatch(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_UNKNOWN, D3DFMT_D24S8), D9_INVALIDCALL);
    }
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_x_cdsm_adapter_checked_before_devtype_and_formats) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    /* Every error here is INVALIDCALL, but the order proves nothing is dereferenced for a bad adapter. */
    EXPECT_HR(IDirect3D9_CheckDepthStencilMatch(d, n, (D3DDEVTYPE)0, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_x_cdfc_ex_validation) {
    D9_GET_EX(d);
    UINT n = IDirect3D9Ex_GetAdapterCount(d);
    EXPECT_HR(IDirect3D9Ex_CheckDeviceFormatConversion(d, n, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8), D9_INVALIDCALL);
    if (n) {
        EXPECT_HR(IDirect3D9Ex_CheckDeviceFormatConversion(d, 0, D3DDEVTYPE_HAL, D3DFMT_UNKNOWN, D3DFMT_X8R8G8B8), D9_INVALIDCALL);
        EXPECT_HR(IDirect3D9Ex_CheckDeviceFormatConversion(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_UNKNOWN), D9_INVALIDCALL);
    }
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_x_cdfc_same_format_ok_when_caps_available) {
    D9_GET(d);
    if (!d9_have_devtype(d, D3DDEVTYPE_HAL)) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    /* Formats that differ only by alpha channel are always convertible once caps are available. */
    EXPECT_HR(IDirect3D9_CheckDeviceFormatConversion(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8), S_OK);
    EXPECT_HR(IDirect3D9_CheckDeviceFormatConversion(d, 0, D3DDEVTYPE_HAL, D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8), S_OK);
    EXPECT_HR(IDirect3D9_CheckDeviceFormatConversion(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8), S_OK);
    IDirect3D9_Release(d);
}

/* ---- IDirect3D9Ex display mode details ---- */
TEST(d3d9_core_x_ex_displaymode_null_rotation_allowed) {
    D9_GET_EX(d);
    if (!IDirect3D9Ex_GetAdapterCount(d)) { IDirect3D9Ex_Release(d); t_skip("no adapters"); return; }
    D3DDISPLAYMODEEX m; memset(&m, 0, sizeof m); m.Size = sizeof m;
    HRESULT hr = IDirect3D9Ex_GetAdapterDisplayModeEx(d, 0, &m, NULL);
    if (hr == D9_NOTAVAILABLE) { IDirect3D9Ex_Release(d); t_skip("no monitor"); return; }
    EXPECT_HR(hr, S_OK);
    CHECK(m.Width > 0 && m.Height > 0);
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_x_ex_displaymode_size_field_exact) {
    D9_GET_EX(d);
    if (!IDirect3D9Ex_GetAdapterCount(d)) { IDirect3D9Ex_Release(d); t_skip("no adapters"); return; }
    UINT sizes[] = { 0, 4, 20, 23, 25, 28, 48, 0xFFFFFFFF };
    for (unsigned i = 0; i < N(sizes); i++) {
        D3DDISPLAYMODEEX m; memset(&m, 0, sizeof m); m.Size = sizes[i];
        D3DDISPLAYROTATION rot;
        HRESULT hr = IDirect3D9Ex_GetAdapterDisplayModeEx(d, 0, &m, &rot);
        CHECKF(hr == D9_INVALIDCALL, "Size %u -> %s", sizes[i], d3dh_hr_name(hr));
    }
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_x_ex_enum_modeex_size_field_exact) {
    D9_GET_EX(d);
    if (!IDirect3D9Ex_GetAdapterCount(d)) { IDirect3D9Ex_Release(d); t_skip("no adapters"); return; }
    D3DDISPLAYMODEFILTER f = { sizeof(D3DDISPLAYMODEFILTER), D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_UNKNOWN };
    UINT sizes[] = { 0, 12, 23, 25, 0xFFFFFFFF };
    for (unsigned i = 0; i < N(sizes); i++) {
        D3DDISPLAYMODEEX m; memset(&m, 0, sizeof m); m.Size = sizes[i];
        HRESULT hr = IDirect3D9Ex_EnumAdapterModesEx(d, 0, &f, 0, &m);
        CHECKF(hr == D9_INVALIDCALL, "Size %u -> %s", sizes[i], d3dh_hr_name(hr));
    }
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_x_ex_filter_scanline_values) {
    D9_GET_EX(d);
    if (!IDirect3D9Ex_GetAdapterCount(d)) { IDirect3D9Ex_Release(d); t_skip("no adapters"); return; }
    /* Scanline ordering 0..2 is accepted, 3 and above is rejected as an invalid filter. */
    for (UINT so = 0; so < 5; so++) {
        D3DDISPLAYMODEFILTER f = { sizeof f, D3DFMT_X8R8G8B8, (D3DSCANLINEORDERING)so };
        UINT c = IDirect3D9Ex_GetAdapterModeCountEx(d, 0, &f);
        D3DDISPLAYMODEEX m; memset(&m, 0, sizeof m); m.Size = sizeof m;
        HRESULT hr = IDirect3D9Ex_EnumAdapterModesEx(d, 0, &f, 0, &m);
        if (so >= 3) { CHECKF(c == 0 && hr == D9_INVALIDCALL, "scanline %u count %u hr %s", so, c, d3dh_hr_name(hr)); }
        else if (c) CHECKF(hr == S_OK, "scanline %u count %u hr %s", so, c, d3dh_hr_name(hr));
        else CHECKF(hr == D9_INVALIDCALL, "scanline %u empty -> %s", so, d3dh_hr_name(hr));
    }
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_x_ex_modecount_matches_plain_enum_count_note) {
    D9_GET_EX(d);
    if (!IDirect3D9Ex_GetAdapterCount(d)) { IDirect3D9Ex_Release(d); t_skip("no adapters"); return; }
    D3DDISPLAYMODEFILTER f = { sizeof f, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_UNKNOWN };
    UINT ex = IDirect3D9Ex_GetAdapterModeCountEx(d, 0, &f), pl = IDirect3D9Ex_GetAdapterModeCount(d, 0, D3DFMT_X8R8G8B8);
    CHECKF(ex >= pl, "Ex filter count %u, GetAdapterModeCount %u", ex, pl);
    IDirect3D9Ex_Release(d);
}

/* ---- Shader validator state machine ---- */
typedef struct v9x v9x;
typedef struct v9xvt {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(v9x *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(v9x *);
    ULONG (STDMETHODCALLTYPE *Release)(v9x *);
    HRESULT (STDMETHODCALLTYPE *Begin)(v9x *, void *cb, void *ctx, DWORD reserved);
    HRESULT (STDMETHODCALLTYPE *Instruction)(v9x *, const char *comp, UINT line, const DWORD *tok, UINT cdw);
    HRESULT (STDMETHODCALLTYPE *End)(v9x *);
} v9xvt;
struct v9x { const v9xvt *vt; };
UDECL(v9x *, Direct3DShaderValidatorCreate9, (void));
static HRESULT CALLBACK vx_cb(const char *file, UINT line, DWORD flags, UINT id, const char *msg, void *ctx) { if (ctx) (*(int *)ctx)++; return S_OK; }
static v9x *mkv(void) {
    Direct3DShaderValidatorCreate9_pfn c = (Direct3DShaderValidatorCreate9_pfn)t_proc_in("d3d9.dll", "Direct3DShaderValidatorCreate9");
    if (!c) { t_skip("missing export"); return NULL; }
    if (t_is_wine()) { t_skip("emulator does not implement the validator"); return NULL; }
    v9x *v = c();
    if (!v) t_skip("validator not created");
    return v;
}
TEST(d3d9_core_x_validator_instruction_before_begin) {
    v9x *v = mkv(); if (!v) return;
    static const DWORD ver[] = { 0xFFFE0200 };
    EXPECT_HR(v->vt->Instruction(v, "t", 1, ver, 1), E_FAIL);
    /* The validator is now in its error state: Begin is refused too. */
    EXPECT_HR(v->vt->Begin(v, (void *)vx_cb, NULL, 0), E_FAIL);
    v->vt->Release(v);
}
TEST(d3d9_core_x_validator_null_tokens_poison) {
    v9x *v = mkv(); if (!v) return;
    int calls = 0;
    EXPECT_HR(v->vt->Begin(v, (void *)vx_cb, &calls, 0), S_OK);
    EXPECT_HR(v->vt->Instruction(v, "t", 1, NULL, 1), E_FAIL);
    static const DWORD ver[] = { 0xFFFE0200 };
    EXPECT_HR(v->vt->Instruction(v, "t", 2, ver, 1), E_FAIL);   /* state 4: everything fails */
    EXPECT_HR(v->vt->End(v), E_FAIL);
    EXPECT_EQ(calls, 1);                       /* Win10: the error callback fires exactly once, for the NULL token pointer */
    v->vt->Release(v);
}
TEST(d3d9_core_x_validator_zero_count_poison) {
    v9x *v = mkv(); if (!v) return;
    int calls = 0;
    static const DWORD ver[] = { 0xFFFE0200 };
    EXPECT_HR(v->vt->Begin(v, (void *)vx_cb, &calls, 0), S_OK);
    EXPECT_HR(v->vt->Instruction(v, "t", 1, ver, 0), E_FAIL);
    EXPECT_HR(v->vt->End(v), E_FAIL);
    v->vt->Release(v);
}
TEST(d3d9_core_x_validator_bad_version_token) {
    v9x *v = mkv(); if (!v) return;
    int calls = 0;
    static const DWORD bad[] = { 0x00000000, 0x12345678, 0x0000FFFF };
    for (unsigned i = 0; i < N(bad); i++) {
        v9x *w = mkv(); if (!w) break;
        EXPECT_HR(w->vt->Begin(w, (void *)vx_cb, &calls, 0), S_OK);
        HRESULT hr = w->vt->Instruction(w, "t", 1, &bad[i], 1);
        CHECKF(hr == E_FAIL, "version token 0x%08lx -> %s", bad[i], d3dh_hr_name(hr));
        w->vt->Release(w);
    }
    v->vt->Release(v);
}
TEST(d3d9_core_x_validator_end_without_end_token) {
    v9x *v = mkv(); if (!v) return;
    int calls = 0;
    static const DWORD ver[] = { 0xFFFF0300 };
    EXPECT_HR(v->vt->Begin(v, (void *)vx_cb, &calls, 0), S_OK);
    HRESULT h = v->vt->Instruction(v, "t", 1, ver, 1);
    EXPECT_HR(h, S_OK);
    EXPECT_HR(v->vt->End(v), E_FAIL);   /* the 0xFFFF end token was never supplied */
    v->vt->Release(v);
}
TEST(d3d9_core_x_validator_instruction_after_end_token_fails) {
    v9x *v = mkv(); if (!v) return;
    int calls = 0;
    static const DWORD ver[] = { 0xFFFE0300 }, end[] = { 0x0000FFFF }, mov[] = { 0x02000001, 0x800F0000, 0x90E40000 };
    EXPECT_HR(v->vt->Begin(v, (void *)vx_cb, &calls, 0), S_OK);
    HRESULT h1 = v->vt->Instruction(v, "t", 1, ver, 1);
    HRESULT h2 = v->vt->Instruction(v, "t", 2, end, 1);
    EXPECT_HR(h1, S_OK); EXPECT_HR(h2, S_OK);
    EXPECT_HR(v->vt->Instruction(v, "t", 3, mov, 3), E_FAIL);
    v->vt->Release(v);
}
TEST(d3d9_core_x_validator_reuse_after_complete_cycle_note) {
    v9x *v = mkv(); if (!v) return;
    int calls = 0;
    static const DWORD ver[] = { 0xFFFE0100 }, end[] = { 0x0000FFFF };
    EXPECT_HR(v->vt->Begin(v, (void *)vx_cb, &calls, 0), S_OK);
    v->vt->Instruction(v, "t", 1, ver, 1);
    v->vt->Instruction(v, "t", 2, end, 1);
    HRESULT he = v->vt->End(v);
    HRESULT hb = v->vt->Begin(v, (void *)vx_cb, &calls, 0);
    EXPECT_HR(he, E_FAIL);   /* Win10: a program with no instructions is invalid, and the object then stays in its error state */
    EXPECT_HR(hb, E_FAIL);
    v->vt->Release(v);
}
TEST(d3d9_core_x_validator_qi_null_out_cleared) {
    v9x *v = mkv(); if (!v) return;
    void *p = (void *)(ULONG_PTR)0x40;
    EXPECT_HR(v->vt->QueryInterface(v, &IID_IDirect3DTexture9, &p), D9_E_NOINTERFACE);
    CHECK(p == NULL);
    EXPECT_HR(v->vt->QueryInterface(v, &GUID_NULL, &p), D9_E_NOINTERFACE);
    CHECK(p == NULL);
    v->vt->Release(v);
}

/* ---- Creation: extra edge cases ---- */
TEST(d3d9_core_x_create9_version_sweep_around_boundaries) {
    __typeof__(&Direct3DCreate9) c = D9_CREATE9(); REQUIRE(c);
    IDirect3D9 *probe = c(D3D_SDK_VERSION);
    if (!probe) { t_skip("Direct3DCreate9 returned NULL"); return; }
    IDirect3D9_Release(probe);
    for (UINT v = 0; v <= 140; v++) {
        int ok = (v == 31 || v == 32 || v >= 132);
        IDirect3D9 *d = c(v);
        CHECKF((d != NULL) == ok, "version %u -> %p", v, (void *)d);
        if (d) IDirect3D9_Release(d);
    }
}
TEST(d3d9_core_x_create9ex_version_sweep_around_boundaries) {
    __typeof__(&Direct3DCreate9Ex) c = D9_CREATE9EX(); REQUIRE(c);
    D9_GET_EX(probe);
    IDirect3D9Ex_Release(probe);
    for (UINT v = 0; v <= 140; v++) {
        int ok = (v == 31 || v == 32 || v >= 132);
        IDirect3D9Ex *d = NULL;
        HRESULT hr = c(v, &d);
        CHECKF(ok ? (hr == S_OK && d) : (hr == D9_NOTAVAILABLE && !d), "version %u -> %s %p", v, d3dh_hr_name(hr), (void *)d);
        if (d) IDirect3D9Ex_Release(d);
    }
}
TEST(d3d9_core_x_create9_and_ex_exports_distinct) {
    FARPROC a = t_proc_in("d3d9.dll", "Direct3DCreate9"), b = t_proc_in("d3d9.dll", "Direct3DCreate9Ex");
    REQUIRE(a); REQUIRE(b);
    CHECK(a != b);
}
TEST(d3d9_core_x_ex_object_plain_methods_work) {
    D9_GET_EX(d);
    /* An Ex object is usable through the IDirect3D9 vtable prefix. */
    IDirect3D9 *p = (IDirect3D9 *)d;
    EXPECT_EQ(IDirect3D9_GetAdapterCount(p), IDirect3D9Ex_GetAdapterCount(d));
    EXPECT_EQ(IDirect3D9_AddRef(p), 2);
    EXPECT_EQ(IDirect3D9_Release(p), 1);
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_x_release_one_of_two_qi_pointers) {
    D9_GET_EX(d);
    IDirect3D9 *a = NULL; IDirect3D9Ex *b = NULL;
    EXPECT_HR(IDirect3D9Ex_QueryInterface(d, &IID_IDirect3D9, (void **)&a), S_OK);
    EXPECT_HR(IDirect3D9Ex_QueryInterface(d, &IID_IDirect3D9Ex, (void **)&b), S_OK);
    EXPECT_EQ(IDirect3D9Ex_Release(d), 2);
    EXPECT_EQ(IDirect3D9_Release(a), 1);
    UINT n = IDirect3D9Ex_GetAdapterCount(b);
    EXPECT_EQ(n, IDirect3D9Ex_GetAdapterCount(b));
    EXPECT_EQ(IDirect3D9Ex_Release(b), 0);
}

/* ---- Registered software device: exact ordering ---- */
TEST(d3d9_core_x_register_software_device_null_is_stateless) {
    D9_GET(d);
    /* A NULL initialisation function never registers anything, so repeated calls keep failing identically. */
    for (int i = 0; i < 3; i++) EXPECT_HR(IDirect3D9_RegisterSoftwareDevice(d, NULL), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}

/* ---- GetDeviceCaps: zeroing contract ---- */
TEST(d3d9_core_x_caps_devtype_range_zeroes_on_failure) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DDEVTYPE bad[] = { (D3DDEVTYPE)0, (D3DDEVTYPE)5, (D3DDEVTYPE)100 };
    for (unsigned i = 0; i < N(bad); i++) {
        D3DCAPS9 c; memset(&c, 0x5A, sizeof c);
        HRESULT hr = IDirect3D9_GetDeviceCaps(d, 0, bad[i], &c);
        if (!FAILED(hr)) t_note("devtype %u unexpectedly accepted: %s", bad[i], d3dh_hr_name(hr));
        if (FAILED(hr)) { const BYTE *b = (const BYTE *)&c; int z = 1; for (size_t k = 0; k < sizeof c; k++) if (b[k]) z = 0; CHECKF(z, "devtype %u caps not zeroed", bad[i]); }
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_x_caps_bytes_past_struct_untouched) {
    /* Exactly sizeof(D3DCAPS9) == 0x130 bytes are written on both success and failure. */
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    struct { D3DCAPS9 c; BYTE guard[64]; } b; memset(&b, 0xA5, sizeof b);
    IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_HAL, &b.c);
    int ok = 1; for (int i = 0; i < 64; i++) if (b.guard[i] != 0xA5) ok = 0;
    CHECK(ok);
    memset(&b, 0xA5, sizeof b);
    IDirect3D9_GetDeviceCaps(d, 0, (D3DDEVTYPE)0, &b.c);
    ok = 1; for (int i = 0; i < 64; i++) if (b.guard[i] != 0xA5) ok = 0;
    CHECK(ok);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_x_caps_hal_flag_dynamic_textures_vs_cdf) {
    /* DYNAMIC usage on a texture requires the driver's dynamic-texture cap (D3DCAPS2_DYNAMICTEXTURES, 0x20000000). */
    D9_GET(d);
    D3DCAPS9 c; memset(&c, 0, sizeof c);
    if (!IDirect3D9_GetAdapterCount(d) || FAILED(IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_HAL, &c))) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    HRESULT hr = IDirect3D9_CheckDeviceFormat(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_DYNAMIC, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8);
    if (!(c.Caps2 & 0x20000000)) CHECKF(hr == D9_INVALIDCALL, "no dynamic textures cap, CDF -> %s", d3dh_hr_name(hr));
    else t_note("dynamic textures supported, CDF -> %s", d3dh_hr_name(hr));
    IDirect3D9_Release(d);
}
