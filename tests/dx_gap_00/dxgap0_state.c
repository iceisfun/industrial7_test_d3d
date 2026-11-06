/* D3D10CreateStateBlock, D3D10CreateEffectPoolFromMemory, D3D10DisassembleEffect (real devices). */
#include "dxgap0.h"
#include <d3d10effect.h>

typedef HRESULT (WINAPI *csb_t)(ID3D10Device *, D3D10_STATE_BLOCK_MASK *, ID3D10StateBlock **);
typedef HRESULT (WINAPI *maskfn_t)(D3D10_STATE_BLOCK_MASK *);
typedef HRESULT (WINAPI *maskcap_t)(D3D10_STATE_BLOCK_MASK *, D3D10_DEVICE_STATE_TYPES, UINT, UINT);

static csb_t get_csb(void) { return (csb_t)t_proc_in("d3d10.dll", "D3D10CreateStateBlock"); }

static int open10(ID3D10Device **dev)
{
    HRESULT hr = d3dh_make_device10(dev, NULL, 0);
    if (FAILED(hr)) { t_skip("no D3D10 device: %s", d3dh_hr_name(hr)); return 0; }
    return 1;
}

static void enable_all(D3D10_STATE_BLOCK_MASK *m)
{
    maskfn_t f = (maskfn_t)t_proc_in("d3d10.dll", "D3D10StateBlockMaskEnableAll");
    if (f) f(m); else memset(m, 0xff, sizeof *m);
}
static void disable_all(D3D10_STATE_BLOCK_MASK *m)
{
    maskfn_t f = (maskfn_t)t_proc_in("d3d10.dll", "D3D10StateBlockMaskDisableAll");
    if (f) f(m); else memset(m, 0, sizeof *m);
}

TEST(dxgap0_stateblock_create_release)
{
    csb_t fn = get_csb();
    ID3D10Device *dev = NULL, *d2 = NULL;
    ID3D10StateBlock *sb = NULL, *sb2 = NULL;
    D3D10_STATE_BLOCK_MASK mask;
    ULONG before, after, rc;
    REQUIRE(fn);
    if (!open10(&dev)) return;
    enable_all(&mask);
    before = d3dh_refcount(dev);
    EXPECT_HR(fn(dev, &mask, &sb), S_OK);
    CHECK(sb != NULL);
    if (sb) {
        EXPECT_EQ(d3dh_refcount(sb), 1);
        /* Win10: the state block object answers QueryInterface with E_NOINTERFACE even for IID_IUnknown and for its own IID */
        sb2 = (ID3D10StateBlock *)1; EXPECT_HR(sb->lpVtbl->QueryInterface(sb, &IID_IUnknown, (void **)&sb2), E_NOINTERFACE_HR); CHECK(sb2 == NULL);
        sb2 = (ID3D10StateBlock *)1; EXPECT_HR(sb->lpVtbl->QueryInterface(sb, &IID_ID3D10StateBlock, (void **)&sb2), E_NOINTERFACE_HR); CHECK(sb2 == NULL);
        EXPECT_HR(sb->lpVtbl->GetDevice(sb, &d2), S_OK);
        CHECK(d2 == dev);
        SAFE_RELEASE(d2);
        after = d3dh_refcount(dev);
        t_note("device refcount before/after state block creation: %lu/%lu", before, after);
        rc = sb->lpVtbl->Release(sb);
        EXPECT_EQ(rc, 0);
        EXPECT_EQ(d3dh_refcount(dev), before);
    }
    SAFE_RELEASE(dev);
}

TEST(dxgap0_stateblock_capture_apply_full)
{
    csb_t fn = get_csb();
    ID3D10Device *dev = NULL;
    ID3D10StateBlock *sb = NULL;
    ID3D10RasterizerState *rs = NULL, *rs_got = NULL;
    D3D10_RASTERIZER_DESC rd;
    D3D10_VIEWPORT vp1 = { 1, 2, 30, 40, 0.f, 1.f }, vp2 = { 5, 6, 70, 80, 0.25f, 0.75f }, got;
    UINT n;
    D3D10_PRIMITIVE_TOPOLOGY topo;
    D3D10_STATE_BLOCK_MASK mask;
    REQUIRE(fn);
    if (!open10(&dev)) return;
    memset(&rd, 0, sizeof rd);
    rd.FillMode = D3D10_FILL_SOLID; rd.CullMode = D3D10_CULL_NONE; rd.DepthClipEnable = TRUE;
    EXPECT_HR(dev->lpVtbl->CreateRasterizerState(dev, &rd, &rs), S_OK);
    enable_all(&mask);
    EXPECT_HR(fn(dev, &mask, &sb), S_OK);
    if (!sb || !rs) goto out;
    dev->lpVtbl->RSSetViewports(dev, 1, &vp1);
    dev->lpVtbl->RSSetState(dev, rs);
    dev->lpVtbl->IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
    EXPECT_HR(sb->lpVtbl->Capture(sb), S_OK);
    /* disturb everything that was captured */
    dev->lpVtbl->RSSetViewports(dev, 1, &vp2);
    dev->lpVtbl->RSSetState(dev, NULL);
    dev->lpVtbl->IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
    EXPECT_HR(sb->lpVtbl->Apply(sb), S_OK);
    n = 1; memset(&got, 0, sizeof got);
    dev->lpVtbl->RSGetViewports(dev, &n, &got);
    EXPECT_EQ(n, 1);
    EXPECT_EQ(got.TopLeftX, 1); EXPECT_EQ(got.TopLeftY, 2); EXPECT_EQ(got.Width, 30); EXPECT_EQ(got.Height, 40);
    dev->lpVtbl->RSGetState(dev, &rs_got);
    CHECK(rs_got == rs);
    SAFE_RELEASE(rs_got);
    dev->lpVtbl->IAGetPrimitiveTopology(dev, &topo);
    EXPECT_EQ(topo, D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
    /* a second Apply is idempotent */
    dev->lpVtbl->IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    EXPECT_HR(sb->lpVtbl->Apply(sb), S_OK);
    dev->lpVtbl->IAGetPrimitiveTopology(dev, &topo);
    EXPECT_EQ(topo, D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
out:
    SAFE_RELEASE(sb); SAFE_RELEASE(rs); SAFE_RELEASE(dev);
}

TEST(dxgap0_stateblock_partial_mask)
{
    csb_t fn = get_csb();
    maskcap_t en = (maskcap_t)t_proc_in("d3d10.dll", "D3D10StateBlockMaskEnableCapture");
    ID3D10Device *dev = NULL;
    ID3D10StateBlock *sb = NULL;
    D3D10_STATE_BLOCK_MASK mask;
    D3D10_VIEWPORT vp1 = { 3, 4, 50, 60, 0.f, 1.f }, vp2 = { 9, 9, 10, 10, 0.f, 1.f }, got;
    D3D10_PRIMITIVE_TOPOLOGY topo;
    UINT n = 1;
    REQUIRE(fn); REQUIRE(en);
    if (!open10(&dev)) return;
    disable_all(&mask);
    EXPECT_HR(en(&mask, D3D10_DST_RS_VIEWPORTS, 0, 1), S_OK);
    EXPECT_HR(fn(dev, &mask, &sb), S_OK);
    if (!sb) { SAFE_RELEASE(dev); return; }
    dev->lpVtbl->RSSetViewports(dev, 1, &vp1);
    dev->lpVtbl->IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
    EXPECT_HR(sb->lpVtbl->Capture(sb), S_OK);
    dev->lpVtbl->RSSetViewports(dev, 1, &vp2);
    dev->lpVtbl->IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
    EXPECT_HR(sb->lpVtbl->Apply(sb), S_OK);
    memset(&got, 0, sizeof got);
    dev->lpVtbl->RSGetViewports(dev, &n, &got);
    EXPECT_EQ(got.TopLeftX, 3); EXPECT_EQ(got.TopLeftY, 4); EXPECT_EQ(got.Width, 50); EXPECT_EQ(got.Height, 60);
    dev->lpVtbl->IAGetPrimitiveTopology(dev, &topo);
    EXPECT_EQ(topo, D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);        /* not part of the mask: left alone */
    SAFE_RELEASE(sb); SAFE_RELEASE(dev);
}

TEST(dxgap0_stateblock_empty_mask)
{
    csb_t fn = get_csb();
    ID3D10Device *dev = NULL;
    ID3D10StateBlock *sb = NULL;
    D3D10_STATE_BLOCK_MASK mask;
    D3D10_PRIMITIVE_TOPOLOGY topo;
    REQUIRE(fn);
    if (!open10(&dev)) return;
    disable_all(&mask);
    EXPECT_HR(fn(dev, &mask, &sb), S_OK);
    if (sb) {
        dev->lpVtbl->IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
        EXPECT_HR(sb->lpVtbl->Capture(sb), S_OK);
        dev->lpVtbl->IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
        EXPECT_HR(sb->lpVtbl->Apply(sb), S_OK);
        dev->lpVtbl->IAGetPrimitiveTopology(dev, &topo);
        EXPECT_EQ(topo, D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
        SAFE_RELEASE(sb);
    }
    SAFE_RELEASE(dev);
}

TEST(dxgap0_stateblock_null_mask_and_release_objects)
{
    csb_t fn = get_csb();
    ID3D10Device *dev = NULL;
    ID3D10StateBlock *sb = NULL;
    D3D10_STATE_BLOCK_MASK mask;
    HRESULT hr;
    REQUIRE(fn);
    if (!open10(&dev)) return;
    hr = GAP0_CALL(fn(dev, NULL, &sb));
    EXPECT_HR(hr, GAP0_AVHR);   /* Win10: the NULL mask is dereferenced */
    SAFE_RELEASE(sb);
    enable_all(&mask);
    EXPECT_HR(fn(dev, &mask, &sb), S_OK);
    if (sb) {
        ULONG before = d3dh_refcount(dev);
        EXPECT_HR(sb->lpVtbl->Capture(sb), S_OK);
        hr = sb->lpVtbl->ReleaseAllDeviceObjects(sb);
        EXPECT_HR(hr, S_OK);
        t_note("device refcount around Capture/ReleaseAllDeviceObjects: %lu -> %lu", before, d3dh_refcount(dev));
        hr = sb->lpVtbl->Apply(sb);
        t_note("Apply after ReleaseAllDeviceObjects = %s", d3dh_hr_name(hr));
        SAFE_RELEASE(sb);
    }
    SAFE_RELEASE(dev);
}

TEST(dxgap0_stateblock_two_blocks_independent)
{
    csb_t fn = get_csb();
    ID3D10Device *dev = NULL;
    ID3D10StateBlock *a = NULL, *b = NULL;
    D3D10_STATE_BLOCK_MASK mask;
    D3D10_PRIMITIVE_TOPOLOGY topo;
    REQUIRE(fn);
    if (!open10(&dev)) return;
    enable_all(&mask);
    EXPECT_HR(fn(dev, &mask, &a), S_OK);
    EXPECT_HR(fn(dev, &mask, &b), S_OK);
    CHECK(a && b && a != b);
    if (a && b) {
        dev->lpVtbl->IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
        a->lpVtbl->Capture(a);
        dev->lpVtbl->IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        b->lpVtbl->Capture(b);
        a->lpVtbl->Apply(a);
        dev->lpVtbl->IAGetPrimitiveTopology(dev, &topo);
        EXPECT_EQ(topo, D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
        b->lpVtbl->Apply(b);
        dev->lpVtbl->IAGetPrimitiveTopology(dev, &topo);
        EXPECT_EQ(topo, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(dev);
}

/* ---- effects ---- */
typedef HRESULT (WINAPI *cep_t)(void *, SIZE_T, UINT, ID3D10Device *, ID3D10EffectPool **);
typedef HRESULT (WINAPI *cef_t)(void *, SIZE_T, UINT, ID3D10Device *, ID3D10EffectPool *, ID3D10Effect **);
typedef HRESULT (WINAPI *dise_t)(ID3D10Effect *, BOOL, ID3D10Blob **);

static const char FX_SRC[] =
    "cbuffer Globals { float4 gColor; };\n"
    "struct VSOut { float4 p : SV_Position; };\n"
    "VSOut VS(float4 p : POSITION) { VSOut o; o.p = p; return o; }\n"
    "float4 PS(VSOut i) : SV_Target { return gColor; }\n"
    "technique10 Simple { pass P0 { SetVertexShader(CompileShader(vs_4_0, VS())); SetGeometryShader(NULL); SetPixelShader(CompileShader(ps_4_0, PS())); } }\n";
static const char POOL_SRC[] =
    "shared cbuffer Shared { float4 sharedValue; };\n";

static ID3DBlob *compile_fx(const char *src, const char *name)
{
    typedef HRESULT (WINAPI *dc_t)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob **, ID3DBlob **);
    dc_t dc = (dc_t)t_proc_in("d3dcompiler_47.dll", "D3DCompile");
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr;
    if (!dc) { t_skip("no D3DCompile"); return NULL; }
    hr = dc(src, strlen(src), name, NULL, NULL, NULL, "fx_4_0", 0, 0, &code, &err);
    if (FAILED(hr)) {
        t_skip("fx_4_0 compile failed (%s): %s", d3dh_hr_name(hr), err ? (const char *)d3dh_blob_ptr(err) : "");
        SAFE_RELEASE(err);
        return NULL;
    }
    SAFE_RELEASE(err);
    return code;
}

TEST(dxgap0_effectpool_create)
{
    cep_t fn = (cep_t)t_proc_in("d3d10.dll", "D3D10CreateEffectPoolFromMemory");
    ID3D10Device *dev = NULL, *d2 = NULL;
    ID3D10EffectPool *pool = NULL;
    ID3D10Effect *eff = NULL;
    ID3DBlob *code;
    REQUIRE(fn);
    if (!open10(&dev)) return;
    code = compile_fx(POOL_SRC, "pool.fx");
    if (!code) { SAFE_RELEASE(dev); return; }
    EXPECT_HR(fn(d3dh_blob_ptr(code), d3dh_blob_size(code), 0, dev, &pool), S_OK);
    if (pool) {
        EXPECT_EQ(d3dh_refcount(pool), 1);
        eff = pool->lpVtbl->AsEffect(pool);
        CHECK(eff != NULL);
        if (eff) {
            EXPECT_EQ(eff->lpVtbl->IsPool(eff), TRUE);
            EXPECT_HR(eff->lpVtbl->GetDevice(eff, &d2), S_OK);
            CHECK(d2 == dev);
            SAFE_RELEASE(d2);
        }
        { ULONG rc = pool->lpVtbl->Release(pool); EXPECT_EQ(rc, 0); }
    }
    /* a NULL device is rejected when binding */
    pool = (ID3D10EffectPool *)(ULONG_PTR)0x10;
    EXPECT_HR_FAILED(fn(d3dh_blob_ptr(code), d3dh_blob_size(code), 0, NULL, &pool));
    CHECK(pool == NULL);
    /* truncated blob */
    pool = (ID3D10EffectPool *)(ULONG_PTR)0x10;
    EXPECT_HR_FAILED(fn(d3dh_blob_ptr(code), d3dh_blob_size(code) / 2, 0, dev, &pool));
    CHECK(pool == NULL);
    SAFE_RELEASE(code); SAFE_RELEASE(dev);
}

TEST(dxgap0_disassemble_effect)
{
    cef_t mk = (cef_t)t_proc_in("d3d10.dll", "D3D10CreateEffectFromMemory");
    dise_t dis = (dise_t)t_proc_in("d3d10.dll", "D3D10DisassembleEffect");
    ID3D10Device *dev = NULL;
    ID3D10Effect *eff = NULL;
    ID3DBlob *code, *text = NULL;
    HRESULT hr;
    REQUIRE(mk); REQUIRE(dis);
    if (!open10(&dev)) return;
    code = compile_fx(FX_SRC, "disas.fx");
    if (!code) { SAFE_RELEASE(dev); return; }
    hr = mk(d3dh_blob_ptr(code), d3dh_blob_size(code), 0, dev, NULL, &eff);
    EXPECT_HR(hr, S_OK);
    if (eff) {
        /* argument validation */
        text = SENT_BLOB;
        EXPECT_HR(dis(NULL, FALSE, &text), INVALIDCALL_HR);
        CHECK(text == SENT_BLOB);                   /* untouched */
        EXPECT_HR(dis(eff, FALSE, NULL), INVALIDCALL_HR);
        EXPECT_HR(dis(NULL, TRUE, NULL), INVALIDCALL_HR);
        /* plain text */
        text = NULL;
        EXPECT_HR(dis(eff, FALSE, &text), S_OK);
        if (text) {
            char *s = malloc(d3dh_blob_size(text) + 1);
            memcpy(s, d3dh_blob_ptr(text), d3dh_blob_size(text)); s[d3dh_blob_size(text)] = 0;
            CHECK(d3dh_blob_size(text) > 16);
            CHECK(strstr(s, "<pre>") == NULL);
            if (!strstr(s, "Simple")) t_note("technique name not found in the listing");
            t_note("effect disassembly head: %.120s", s);
            free(s);
            SAFE_RELEASE(text);
        }
        /* colour-coded: wrapped in HTML */
        EXPECT_HR(dis(eff, TRUE, &text), S_OK);
        if (text) {
            char *s = malloc(d3dh_blob_size(text) + 1);
            memcpy(s, d3dh_blob_ptr(text), d3dh_blob_size(text)); s[d3dh_blob_size(text)] = 0;
            CHECK(strncmp(s, "<pre>", 5) == 0);
            CHECK(strstr(s, "<body bgcolor=\"#000000\">") != NULL);
            CHECK(strstr(s, "<font color") != NULL);
            free(s);
            SAFE_RELEASE(text);
        }
        { ULONG rc = eff->lpVtbl->Release(eff); EXPECT_EQ(rc, 0); }
    }
    SAFE_RELEASE(code); SAFE_RELEASE(dev);
}
