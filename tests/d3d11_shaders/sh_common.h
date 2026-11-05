/* Shared fixtures for the d3d11_shaders area. */
#ifndef SH_COMMON_H
#define SH_COMMON_H
#include "d3dh.h"
#include "shtable.h"

enum { ST_VS, ST_PS, ST_GS, ST_HS, ST_DS, ST_CS, ST_COUNT };
static const char *const sh_stage_name[ST_COUNT] = { "vs", "ps", "gs", "hs", "ds", "cs" };

typedef struct { ID3D11Device *dev; ID3D11DeviceContext *ctx; D3D_FEATURE_LEVEL fl; D3D_DRIVER_TYPE drv; } SHENV;

/* Device with the default feature level list. Returns 0 (after t_skip) when unavailable or below minfl. */
static int sh_env_fl(SHENV *e, const D3D_FEATURE_LEVEL *fls, UINT n, UINT minfl)
{
    memset(e, 0, sizeof *e);
    HRESULT hr = d3dh_make_device11_fl(&e->dev, &e->ctx, fls, n, &e->fl, &e->drv, 0);
    if (FAILED(hr)) { t_skip("no D3D11 device (%s)", d3dh_hr_name(hr)); return 0; }
    if ((UINT)e->fl < minfl) { t_skip("feature level 0x%x below required 0x%x", (UINT)e->fl, minfl); SAFE_RELEASE(e->ctx); SAFE_RELEASE(e->dev); return 0; }
    return 1;
}
static int sh_env(SHENV *e, UINT minfl) { return sh_env_fl(e, NULL, 0, minfl); }
static void sh_env_free(SHENV *e) { if (e->ctx) { ID3D11DeviceContext_ClearState(e->ctx); } SAFE_RELEASE(e->ctx); SAFE_RELEASE(e->dev); }
/* Device restricted to exactly one feature level. */
static int sh_env_exact(SHENV *e, D3D_FEATURE_LEVEL fl)
{
    if (!sh_env_fl(e, &fl, 1, 0)) return 0;
    return 1;
}
#define SH_ENV(e, minfl) SHENV e; if (!sh_env(&e, minfl)) return
#define SH_ENV_EXACT(e, fl) SHENV e; if (!sh_env_exact(&e, fl)) return

static int sh_stage_of_target(const char *t)
{
    for (int i = 0; i < ST_COUNT; i++) if (t[0] == sh_stage_name[i][0] && t[1] == sh_stage_name[i][1]) return i;
    return -1;
}
static const sh_ent *sh_find(const char *name)
{
    for (int i = 0; i < SH_NTABLE; i++) if (!strcmp(sh_table[i].name, name)) return &sh_table[i];
    return NULL;
}

/* Compile (cached) a table entry. NULL on failure: a hard failure on Windows, a skip under Wine's weak compiler. */
static ID3DBlob *sh_blob_target(const char *name, const char *target)
{
    static struct { char key[96]; ID3DBlob *b; int tried; } cache[256];
    char key[96]; snprintf(key, sizeof key, "%s/%s", name, target);
    int free_slot = -1;
    for (int i = 0; i < 256; i++) {
        if (cache[i].tried && !strcmp(cache[i].key, key)) return cache[i].b;
        if (!cache[i].tried && free_slot < 0) free_slot = i;
    }
    const sh_ent *e = sh_find(name);
    if (!e) { CHECKF(0, "unknown table shader %s", name); return NULL; }
    ID3DBlob *b = NULL;
    HRESULT hr = d3dh_compile(e->src, "main", target, 0, &b);
    if (hr == D3DH_E_NOCOMPILER) { t_skip("d3dcompiler_47 unavailable"); return NULL; }
    if (FAILED(hr)) {
        if (t_is_wine()) t_skip("wine compiler cannot build %s/%s", name, target);
        else CHECKF(0, "D3DCompile %s as %s failed hr=%s: %s", name, target, d3dh_hr_name(hr), d3dh_compile_errors());
        b = NULL;
    }
    if (free_slot >= 0) { snprintf(cache[free_slot].key, sizeof cache[free_slot].key, "%s", key); cache[free_slot].b = b; cache[free_slot].tried = 1; }
    return b;
}
static ID3DBlob *sh_blob(const char *name)
{
    const sh_ent *e = sh_find(name);
    if (!e) { CHECKF(0, "unknown table shader %s", name); return NULL; }
    return sh_blob_target(name, e->target);
}
#define SH_BLOB(var, name) ID3DBlob *var = sh_blob(name); if (!var) return
#define SH_BP(b) d3dh_blob_ptr(b)
#define SH_BN(b) d3dh_blob_size(b)

/* Copy of a blob that can be damaged freely. */
static unsigned char *sh_dup(ID3DBlob *b, size_t extra)
{
    unsigned char *p = malloc(SH_BN(b) + extra + 1);
    memcpy(p, SH_BP(b), SH_BN(b));
    return p;
}

/* Stage-generic creation / binding. */
static HRESULT sh_create(ID3D11Device *d, int st, const void *p, SIZE_T n, ID3D11ClassLinkage *cl, void **out)
{
    switch (st) {
    case ST_VS: return ID3D11Device_CreateVertexShader(d, p, n, cl, (ID3D11VertexShader **)out);
    case ST_PS: return ID3D11Device_CreatePixelShader(d, p, n, cl, (ID3D11PixelShader **)out);
    case ST_GS: return ID3D11Device_CreateGeometryShader(d, p, n, cl, (ID3D11GeometryShader **)out);
    case ST_HS: return ID3D11Device_CreateHullShader(d, p, n, cl, (ID3D11HullShader **)out);
    case ST_DS: return ID3D11Device_CreateDomainShader(d, p, n, cl, (ID3D11DomainShader **)out);
    default:    return ID3D11Device_CreateComputeShader(d, p, n, cl, (ID3D11ComputeShader **)out);
    }
}
static void sh_set(ID3D11DeviceContext *c, int st, void *sh, ID3D11ClassInstance *const *ci, UINT n)
{
    switch (st) {
    case ST_VS: ID3D11DeviceContext_VSSetShader(c, sh, ci, n); break;
    case ST_PS: ID3D11DeviceContext_PSSetShader(c, sh, ci, n); break;
    case ST_GS: ID3D11DeviceContext_GSSetShader(c, sh, ci, n); break;
    case ST_HS: ID3D11DeviceContext_HSSetShader(c, sh, ci, n); break;
    case ST_DS: ID3D11DeviceContext_DSSetShader(c, sh, ci, n); break;
    default:    ID3D11DeviceContext_CSSetShader(c, sh, ci, n); break;
    }
}
static void sh_get(ID3D11DeviceContext *c, int st, void **sh, ID3D11ClassInstance **ci, UINT *n)
{
    switch (st) {
    case ST_VS: ID3D11DeviceContext_VSGetShader(c, (ID3D11VertexShader **)sh, ci, n); break;
    case ST_PS: ID3D11DeviceContext_PSGetShader(c, (ID3D11PixelShader **)sh, ci, n); break;
    case ST_GS: ID3D11DeviceContext_GSGetShader(c, (ID3D11GeometryShader **)sh, ci, n); break;
    case ST_HS: ID3D11DeviceContext_HSGetShader(c, (ID3D11HullShader **)sh, ci, n); break;
    case ST_DS: ID3D11DeviceContext_DSGetShader(c, (ID3D11DomainShader **)sh, ci, n); break;
    default:    ID3D11DeviceContext_CSGetShader(c, (ID3D11ComputeShader **)sh, ci, n); break;
    }
}
static const IID *sh_iid(int st)
{
    static const IID *const t[ST_COUNT] = { &IID_ID3D11VertexShader, &IID_ID3D11PixelShader, &IID_ID3D11GeometryShader,
                                            &IID_ID3D11HullShader, &IID_ID3D11DomainShader, &IID_ID3D11ComputeShader };
    return t[st];
}
#define SH_REL(p) do { if (p) { IUnknown_Release((IUnknown *)(p)); (p) = NULL; } } while (0)

/* Builds a minimal valid DXBC container (no chunks) for header-validation tests. */
static size_t sh_empty_dxbc(unsigned char *buf)
{
    memset(buf, 0, 32);
    memcpy(buf, "DXBC", 4);
    *(DWORD *)(buf + 20) = 1;
    *(DWORD *)(buf + 24) = 32;
    return 32;
}
#endif
