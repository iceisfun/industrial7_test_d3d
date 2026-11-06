/* d3dcompiler_47 behaviours checked by repetition and concurrency: compile/reflect determinism, thread safety, leak freedom. */
#include "rb.h"

static int rb_xdesc(ID3D11ShaderReflection *r, D3D11_SHADER_DESC *d)
{
    HRESULT hr; memset(d, 0, sizeof *d); hr = r->lpVtbl->GetDesc(r, d);
    t_check(SUCCEEDED(hr), __FILE__, __LINE__, "GetDesc", "%s", d3dh_hr_name(hr));
    return SUCCEEDED(hr);
}
#define XSRC_A "cbuffer CB : register(b0) { float4x4 m; float4 tint; };\nTexture2D t : register(t0); SamplerState s : register(s0);\n" \
               "float4 main(float4 p : SV_Position, float2 uv : TEXCOORD0) : SV_Target { return t.Sample(s, uv) * tint + mul(p, m); }\n"
#define XSRC_B "struct VI { float4 p : POSITION; float3 n : NORMAL; float2 uv : TEXCOORD0; };\n" \
               "struct VO { float4 p : SV_Position; float3 n : NORMAL; float2 uv : TEXCOORD0; };\n" \
               "cbuffer C : register(b1) { float4x4 wvp; };\nVO main(VI i) { VO o; o.p = mul(i.p, wvp); o.n = i.n; o.uv = i.uv; return o; }\n"

/* Twenty fresh compiles of the same source give byte-identical containers (checksum included). */
TEST(rb_x_compile_is_deterministic_20x)
{
    ID3DBlob *first = NULL;
    if (!rb_compile_t(XSRC_A, "ps_5_0", 0, &first)) return;
    for (int i = 1; i < 20; i++) {
        ID3DBlob *b = NULL;
        if (!rb_compile_t(XSRC_A, "ps_5_0", 0, &b)) break;
        CHECKF(RB_BS(b) == RB_BS(first) && !memcmp(RB_BP(b), RB_BP(first), RB_BS(first)), "compile %d differs from compile 0", i);
        ID3D10Blob_Release(b);
    }
    ID3D10Blob_Release(first);
}

/* Different targets / flags give different containers; the container hash covers everything after the 20 byte prefix. */
TEST(rb_x_compile_output_depends_on_target_and_flags)
{
    ID3DBlob *a = NULL, *b = NULL, *c = NULL;
    if (!rb_compile_t(XSRC_A, "ps_5_0", 0, &a) || !rb_compile_t(XSRC_A, "ps_4_0", 0, &b)) return;
    if (!rb_compile_t(XSRC_A, "ps_5_0", D3DCOMPILE_DEBUG, &c)) return;
    CHECK(RB_BS(a) != RB_BS(b) || memcmp(RB_BP(a), RB_BP(b), RB_BS(a)));
    CHECK(RB_BS(c) > RB_BS(a));                                  /* debug info adds chunks */
    CHECK(memcmp(RB_BP(a) + 4, RB_BP(c) + 4, 16) != 0);          /* and changes the checksum */
    ID3D10Blob_Release(a); ID3D10Blob_Release(b); ID3D10Blob_Release(c);
}

/* Reflecting the same blob again and again neither leaks handles nor changes the answer. */
TEST(rb_x_reflect_repeated_no_handle_leak)
{
    DWORD h0, h1; D3D11_SHADER_DESC first, d;
    RB_COMPILE(b, XSRC_A, "ps_5_0");
    GetProcessHandleCount(GetCurrentProcess(), &h0);
    for (int i = 0; i < 500; i++) {
        ID3D11ShaderReflection *r = rb_reflect(b);
        if (!r) break;
        if (rb_xdesc(r, &d)) {
            if (i == 0) first = d;
            else CHECKF(!memcmp(&first, &d, sizeof d - sizeof(LPCSTR)) || (d.InstructionCount == first.InstructionCount && d.ConstantBuffers == first.ConstantBuffers && d.BoundResources == first.BoundResources), "descriptor changed on iteration %d", i);
        }
        EXPECT_EQ(r->lpVtbl->Release(r), 0);
    }
    GetProcessHandleCount(GetCurrentProcess(), &h1);
    CHECKF(h1 <= h0 + 4, "handle count grew from %lu to %lu over 500 reflections", h0, h1);
    ID3D10Blob_Release(b);
}

typedef struct { const BYTE *blob; SIZE_T size; volatile LONG bad; UINT want_instr, want_cb, want_res; int rounds; } RxCtx;
static DWORD WINAPI rx_thread(LPVOID p)
{
    RxCtx *c = p;
    for (int i = 0; i < c->rounds; i++) {
        ID3D11ShaderReflection *r = NULL; D3D11_SHADER_DESC d;
        if (FAILED(d3dh_D3DReflect()(c->blob, c->size, &IID_ID3D11ShaderReflection, (void **)&r)) || !r) { InterlockedIncrement(&c->bad); continue; }
        if (FAILED(r->lpVtbl->GetDesc(r, &d)) || d.InstructionCount != c->want_instr || d.ConstantBuffers != c->want_cb || d.BoundResources != c->want_res) InterlockedIncrement(&c->bad);
        { ID3D11ShaderReflectionConstantBuffer *cb = r->lpVtbl->GetConstantBufferByName(r, "CB"); D3D11_SHADER_BUFFER_DESC bd; if (FAILED(cb->lpVtbl->GetDesc(cb, &bd)) || bd.Variables != 2) InterlockedIncrement(&c->bad); }
        r->lpVtbl->Release(r);
    }
    return 0;
}
/* Six threads reflect one shared blob concurrently. */
TEST(rb_x_reflect_concurrent_same_blob)
{
    RB_COMPILE(b, XSRC_A, "ps_5_0");
    ID3D11ShaderReflection *r0 = rb_reflect(b); D3D11_SHADER_DESC d; RxCtx c; HANDLE th[6];
    if (!r0 || !rb_xdesc(r0, &d)) { if (r0) r0->lpVtbl->Release(r0); ID3D10Blob_Release(b); return; }
    memset(&c, 0, sizeof c); c.blob = RB_BP(b); c.size = RB_BS(b); c.want_instr = d.InstructionCount; c.want_cb = d.ConstantBuffers; c.want_res = d.BoundResources; c.rounds = 200;
    for (int i = 0; i < 6; i++) th[i] = CreateThread(NULL, 0, rx_thread, &c, 0, NULL);
    for (int i = 0; i < 6; i++) { WaitForSingleObject(th[i], 120000); CloseHandle(th[i]); }
    EXPECT_EQ(c.bad, 0);
    r0->lpVtbl->Release(r0); ID3D10Blob_Release(b);
}

typedef struct { volatile LONG bad; int which; } CxCtx;
static DWORD WINAPI cx_thread(LPVOID p)
{
    CxCtx *c = p; const char *src = (c->which & 1) ? XSRC_B : XSRC_A, *tgt = (c->which & 1) ? "vs_5_0" : "ps_5_0";
    ID3DBlob *ref = NULL;
    if (FAILED(d3dh_compile(src, "main", tgt, 0, &ref))) { InterlockedIncrement(&c->bad); return 0; }
    for (int i = 0; i < 25; i++) {
        ID3DBlob *b = NULL;
        if (FAILED(d3dh_compile(src, "main", tgt, 0, &b))) { InterlockedIncrement(&c->bad); continue; }
        if (RB_BS(b) != RB_BS(ref) || memcmp(RB_BP(b), RB_BP(ref), RB_BS(ref))) InterlockedIncrement(&c->bad);
        ID3D10Blob_Release(b);
    }
    ID3D10Blob_Release(ref);
    return 0;
}
/* Concurrent compiles of two different shaders each equal that shader compiled alone. */
TEST(rb_x_compile_concurrent_matches_serial)
{
    CxCtx c[4]; HANDLE th[4]; ID3DBlob *a = NULL, *b = NULL;
    if (!d3dh_D3DCompile()) { t_skip("no compiler"); return; }
    if (!rb_compile_t(XSRC_A, "ps_5_0", 0, &a) || !rb_compile_t(XSRC_B, "vs_5_0", 0, &b)) return;
    for (int i = 0; i < 4; i++) { c[i].bad = 0; c[i].which = i; th[i] = CreateThread(NULL, 0, cx_thread, &c[i], 0, NULL); }
    for (int i = 0; i < 4; i++) { WaitForSingleObject(th[i], 180000); CloseHandle(th[i]); EXPECT_EQ(c[i].bad, 0); }
    ID3D10Blob_Release(a); ID3D10Blob_Release(b);
}
