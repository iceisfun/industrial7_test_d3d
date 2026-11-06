/* d3dcompiler_47: D3DCompressShaders / D3DDecompressShaders / D3DCreateLinker / D3DLoadModule / D3DReflectLibrary. */
#include "gap1.h"

typedef struct { const void *p; SIZE_T n; } g1_sd;
UDECL(HRESULT, D3DCompressShaders, (UINT, g1_sd *, UINT, ID3DBlob **));
UDECL(HRESULT, D3DDecompressShaders, (const void *, SIZE_T, UINT, UINT, UINT *, UINT, ID3DBlob **, UINT *));
UDECL(HRESULT, D3DReflectLibrary, (const void *, SIZE_T, REFIID, void **));

#define BREL(b) do { if (b) { ID3D10Blob_Release(b); (b) = NULL; } } while (0)
#define BP(b)  ((const unsigned char *)ID3D10Blob_GetBufferPointer(b))
#define BS(b)  ((SIZE_T)ID3D10Blob_GetBufferSize(b))

#define FOURCC_RDEF G1_FOURCC('R', 'D', 'E', 'F')
#define FOURCC_STAT G1_FOURCC('S', 'T', 'A', 'T')
#define FOURCC_SDBG G1_FOURCC('S', 'D', 'B', 'G')
#define FOURCC_ISGN G1_FOURCC('I', 'S', 'G', 'N')
#define FOURCC_OSGN G1_FOURCC('O', 'S', 'G', 'N')
#define FOURCC_SHEX G1_FOURCC('S', 'H', 'E', 'X')
#define FOURCC_SHDR G1_FOURCC('S', 'H', 'D', 'R')

static ID3DBlob *fx_vs(UINT flags) { return g1_compile(SH_VS_TRANSFORM, "vs_5_0", flags); }
static ID3DBlob *fx_ps(UINT flags) { return g1_compile(SH_FLAT_PS, "ps_5_0", flags); }

/* Compress, skipping the test when the compiler cannot (emulated stub). Returns the compressed blob or NULL. */
static ID3DBlob *compress_n(UINT n, g1_sd *sd, UINT flags) {
    ID3DBlob *out = NULL;
    HRESULT hr = UN(D3DCompressShaders)(n, sd, flags, &out);
    if (FAILED(hr) && t_is_wine()) { t_skip("compress not implemented here (%s)", d3dh_hr_name(hr)); return NULL; }
    EXPECT_HR(hr, S_OK);
    if (FAILED(hr)) { BREL(out); return NULL; }
    CHECK(out != NULL);
    return out;
}

TEST(gap1_compress_null_out) {
    SKIP_ON_WINE("compiler API not implemented");
    REQUIRE(UN(D3DCompressShaders));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    g1_sd sd = { BP(vs), BS(vs) };
    EXPECT_HR(UN(D3DCompressShaders)(1, &sd, 0, NULL), G1_E_FAIL);
    EXPECT_HR(UN(D3DCompressShaders)(1, &sd, 1, NULL), G1_E_FAIL);
    BREL(vs);
}

TEST(gap1_compress_bad_flags_table) {
    SKIP_ON_WINE("compiler API not implemented");
    REQUIRE(UN(D3DCompressShaders));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    g1_sd sd = { BP(vs), BS(vs) };
    /* only bit 0 (D3D_COMPRESS_SHADER_KEEP_ALL_PARTS) is defined: any other bit is rejected and the output slot is cleared */
    static const UINT bad[] = { 2, 3, 4, 8, 0x10, 0x100, 0x10000, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        ID3DBlob *out = (ID3DBlob *)(ULONG_PTR)0x1000;
        HRESULT hr = UN(D3DCompressShaders)(1, &sd, bad[i], &out);
        CHECKF(hr == G1_E_FAIL, "flags 0x%x -> %s", bad[i], d3dh_hr_name(hr));
        CHECKF(out == NULL, "flags 0x%x out not cleared", bad[i]);
    }
    BREL(vs);
}

TEST(gap1_compress_header_layout) {
    SKIP_ON_WINE("compiler API not implemented");
    REQUIRE(UN(D3DCompressShaders));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    g1_sd sd = { BP(vs), BS(vs) };
    for (UINT fl = 0; fl <= 1; fl++) {
        ID3DBlob *c = compress_n(1, &sd, fl); if (!c) break;
        const unsigned char *p = BP(c);
        CHECK(BS(c) > 0x20);
        if (BS(c) >= 0x20) {
            EXPECT_EQ(g1_rd32(p), 0x44435342u);              /* container magic that D3DDecompressShaders requires */
            EXPECT_EQ(*(unsigned short *)(p + 4), 1);
            EXPECT_EQ(*(unsigned short *)(p + 6), 1);
            EXPECT_EQ(g1_rd32(p + 8), 0);
            for (int i = 0x10; i < 0x20; i += 4) EXPECT_EQ(g1_rd32(p + i), 0);
        }
        BREL(c);
    }
    BREL(vs);
}

static void expect_same_chunks(ID3DBlob *a, ID3DBlob *b, const char *what) {
    CHECKF(g1_is_dxbc(BP(b), BS(b)), "%s: not a DXBC container", what);
    if (!g1_is_dxbc(BP(b), BS(b))) return;
    EXPECT_EQ(g1_rd32(BP(b) + 0x18), BS(b));                /* total-size field matches the blob */
    EXPECT_EQ(g1_chunk_count(BP(a)), g1_chunk_count(BP(b)));
    if (g1_chunk_count(BP(a)) != g1_chunk_count(BP(b))) return;
    DWORD n = g1_chunk_count(BP(a));
    /* Win10: D3DDecompressShaders emits the parts in REVERSE order of the source container (source RDEF ISGN OSGN SHEX STAT -> STAT SHEX OSGN ISGN RDEF) */
    for (DWORD i = 0; i < n; i++) {
        DWORD j = n - 1 - i;
        CHECKF(g1_chunk_fourcc(BP(a), i) == g1_chunk_fourcc(BP(b), j), "%s: chunk %u fourcc", what, i);
        CHECKF(g1_chunk_size_at(BP(a), i) == g1_chunk_size_at(BP(b), j), "%s: chunk %u size", what, i);
        if (g1_chunk_size_at(BP(a), i) == g1_chunk_size_at(BP(b), j))
            CHECKF(!memcmp(g1_chunk_data_at(BP(a), i), g1_chunk_data_at(BP(b), j), g1_chunk_size_at(BP(a), i)), "%s: chunk %u payload", what, i);
    }
}

TEST(gap1_compress_roundtrip_keep_all_parts) {
    SKIP_ON_WINE("compiler API not implemented");
    REQUIRE(UN(D3DCompressShaders)); REQUIRE(UN(D3DDecompressShaders));
    ID3DBlob *vs = fx_vs(D3DCOMPILE_DEBUG); if (!vs) { t_skip("no compiler"); return; }
    g1_sd sd = { BP(vs), BS(vs) };
    ID3DBlob *c = compress_n(1, &sd, 1);
    if (c) {
        ID3DBlob *out[1] = { NULL }; UINT total = 0xdead;
        HRESULT hr = UN(D3DDecompressShaders)(BP(c), BS(c), 1, 0, NULL, 0, out, &total);
        EXPECT_HR(hr, S_OK);
        EXPECT_EQ(total, 1);
        CHECK(out[0] != NULL);
        if (out[0]) {
            expect_same_chunks(vs, out[0], "keep-all");
            EXPECT_EQ(BS(out[0]), BS(vs));   /* same size, parts reversed: not byte identical */
            CHECK(memcmp(BP(out[0]), BP(vs), BS(vs)) != 0);
            BREL(out[0]);
        }
    }
    BREL(c); BREL(vs);
}

TEST(gap1_compress_roundtrip_strips_reflection_and_debug) {
    SKIP_ON_WINE("compiler API not implemented");
    REQUIRE(UN(D3DCompressShaders)); REQUIRE(UN(D3DDecompressShaders));
    ID3DBlob *vs = fx_vs(D3DCOMPILE_DEBUG); if (!vs) { t_skip("no compiler"); return; }
    g1_sd sd = { BP(vs), BS(vs) };
    ID3DBlob *c = compress_n(1, &sd, 0);
    if (c) {
        ID3DBlob *out[1] = { NULL }; UINT total = 0;
        EXPECT_HR(UN(D3DDecompressShaders)(BP(c), BS(c), 1, 0, NULL, 0, out, &total), S_OK);
        if (out[0]) {
            const void *p = BP(out[0]); SIZE_T n = BS(out[0]);
            CHECK(g1_is_dxbc(p, n));
            /* parts that only serve reflection/statistics/debugging are dropped without KEEP_ALL_PARTS */
            CHECK(!g1_has_chunk(p, n, FOURCC_RDEF));
            CHECK(!g1_has_chunk(p, n, FOURCC_STAT));
            CHECK(!g1_has_chunk(p, n, FOURCC_SDBG));
            CHECK(g1_has_chunk(p, n, G1_FOURCC('S', 'P', 'D', 'B')));   /* the compiler's debug part (SPDB) is kept, not stripped */
            EXPECT_EQ(g1_chunk_count(p), 4);                           /* SPDB SHEX OSGN ISGN, in that order */
            if (g1_chunk_count(p) == 4) { EXPECT_EQ(g1_chunk_fourcc(p, 0), G1_FOURCC('S', 'P', 'D', 'B')); EXPECT_EQ(g1_chunk_fourcc(p, 1), FOURCC_SHEX); EXPECT_EQ(g1_chunk_fourcc(p, 2), FOURCC_OSGN); EXPECT_EQ(g1_chunk_fourcc(p, 3), FOURCC_ISGN); }
            /* the executable parts stay */
            CHECK(g1_has_chunk(p, n, FOURCC_SHEX) || g1_has_chunk(p, n, FOURCC_SHDR));
            CHECK(g1_has_chunk(p, n, FOURCC_ISGN));
            CHECK(g1_has_chunk(p, n, FOURCC_OSGN));
            CHECK(n < BS(vs));
            /* executable code is unchanged */
            DWORD s1 = 0, s2 = 0;
            const unsigned char *a = g1_find_chunk(BP(vs), BS(vs), FOURCC_SHEX, &s1), *b = g1_find_chunk(p, n, FOURCC_SHEX, &s2);
            if (a && b) { EXPECT_EQ(s1, s2); CHECK(s1 == s2 && !memcmp(a, b, s1)); }
            BREL(out[0]);
        }
    }
    BREL(c); BREL(vs);
}

TEST(gap1_decompress_two_shaders_all_orders) {
    SKIP_ON_WINE("compiler API not implemented");
    REQUIRE(UN(D3DCompressShaders)); REQUIRE(UN(D3DDecompressShaders));
    ID3DBlob *vs = fx_vs(0), *ps = fx_ps(0);
    if (!vs || !ps) { BREL(vs); BREL(ps); t_skip("no compiler"); return; }
    g1_sd sd[2] = { { BP(vs), BS(vs) }, { BP(ps), BS(ps) } };
    ID3DBlob *c = compress_n(2, sd, 1);
    if (c) {
        ID3DBlob *o[2] = { NULL, NULL }; UINT total = 0;
        EXPECT_HR(UN(D3DDecompressShaders)(BP(c), BS(c), 2, 0, NULL, 0, o, &total), S_OK);
        EXPECT_EQ(total, 2);
        if (o[0] && o[1]) { expect_same_chunks(vs, o[0], "all[0]"); expect_same_chunks(ps, o[1], "all[1]"); }
        BREL(o[0]); BREL(o[1]);

        /* explicit index list (uStartIndex is ignored when pIndices is given) */
        UINT idx[2] = { 1, 0 };
        total = 0;
        EXPECT_HR(UN(D3DDecompressShaders)(BP(c), BS(c), 2, 7, idx, 0, o, &total), S_OK);
        EXPECT_EQ(total, 2);
        if (o[0] && o[1]) { expect_same_chunks(ps, o[0], "idx[0]"); expect_same_chunks(vs, o[1], "idx[1]"); }
        BREL(o[0]); BREL(o[1]);

        /* start index */
        ID3DBlob *one[1] = { NULL }; total = 0;
        EXPECT_HR(UN(D3DDecompressShaders)(BP(c), BS(c), 1, 1, NULL, 0, one, &total), S_OK);
        EXPECT_EQ(total, 2);   /* pTotalShaders is the number of shaders in the container, not the number returned */
        if (one[0]) expect_same_chunks(ps, one[0], "start1");
        BREL(one[0]);

        /* pTotalShaders is optional */
        EXPECT_HR(UN(D3DDecompressShaders)(BP(c), BS(c), 1, 0, NULL, 0, one, NULL), S_OK);
        if (one[0]) expect_same_chunks(vs, one[0], "start0");
        BREL(one[0]);
    }
    BREL(c); BREL(vs); BREL(ps);
}

TEST(gap1_decompress_bad_arguments) {
    SKIP_ON_WINE("compiler API not implemented");
    REQUIRE(UN(D3DCompressShaders)); REQUIRE(UN(D3DDecompressShaders));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    g1_sd sd = { BP(vs), BS(vs) };
    ID3DBlob *c = compress_n(1, &sd, 0);
    if (c) {
        SIZE_T n = BS(c);
        unsigned char *buf = malloc(n); memcpy(buf, BP(c), n);
        ID3DBlob *o[1];
        UINT total;
#define DEC(src, size, fl, outp) UN(D3DDecompressShaders)((src), (size), 1, 0, NULL, (fl), (outp), &total)
        o[0] = NULL; EXPECT_HR(DEC(NULL, n, 0, o), G1_E_FAIL);
        o[0] = NULL; EXPECT_HR(DEC(buf, 0, 0, o), G1_E_FAIL);
        o[0] = NULL; EXPECT_HR(DEC(buf, 0x1f, 0, o), G1_E_FAIL);
        EXPECT_HR(DEC(buf, n, 0, NULL), G1_E_FAIL);
        o[0] = NULL; EXPECT_HR(DEC(buf, n, 1, o), G1_E_FAIL);           /* no flags are defined */
        o[0] = NULL; EXPECT_HR(DEC(buf, n, 0x80000000u, o), G1_E_FAIL);
        o[0] = NULL; EXPECT_HR(DEC(buf, (SIZE_T)0x80000000u, 0, o), G1_E_FAIL);   /* size above INT_MAX is rejected before any read */
        /* each header field that must hold a fixed value */
        static const struct { int off; unsigned char val; const char *what; } hdr[] = {
            { 0, 'X', "magic" }, { 3, 0, "magic last byte" }, { 4, 2, "major" }, { 6, 2, "minor" }, { 8, 1, "field 8" },
            { 0x10, 1, "reserved 0x10" }, { 0x14, 1, "reserved 0x14" }, { 0x18, 1, "reserved 0x18" }, { 0x1c, 1, "reserved 0x1c" },
        };
        for (unsigned i = 0; i < sizeof hdr / sizeof hdr[0]; i++) {
            unsigned char save = buf[hdr[i].off];
            buf[hdr[i].off] = (buf[hdr[i].off] == hdr[i].val) ? (unsigned char)(hdr[i].val ^ 0x55) : hdr[i].val;
            o[0] = NULL;
            HRESULT hr = DEC(buf, n, 0, o);
            CHECKF(hr == G1_E_FAIL, "header %s corrupt -> %s", hdr[i].what, d3dh_hr_name(hr));
            BREL(o[0]);
            buf[hdr[i].off] = save;
        }
        o[0] = NULL; total = 0;
        EXPECT_HR(DEC(buf, n, 0, o), S_OK);                                   /* control: the unmodified data still works */
        BREL(o[0]);
        free(buf);
    }
    BREL(c); BREL(vs);
}

TEST(gap1_compress_raw_blob_roundtrip) {
    SKIP_ON_WINE("compiler API not implemented");
    REQUIRE(UN(D3DCompressShaders)); REQUIRE(UN(D3DDecompressShaders));
    /* non-container input (HLSL text, a compiled effect...) is carried as an opaque part */
    static const char text[] = "float4 main() : SV_Target { return float4(1,2,3,4); } // gap1 raw payload";
    g1_sd sd = { text, sizeof text };
    ID3DBlob *out = NULL;
    HRESULT hr = UN(D3DCompressShaders)(1, &sd, 0, &out);
    if (FAILED(hr)) { t_note("compress of raw data -> %s", d3dh_hr_name(hr)); BREL(out); return; }
    ID3DBlob *o[1] = { NULL }; UINT total = 0;
    EXPECT_HR(UN(D3DDecompressShaders)(BP(out), BS(out), 1, 0, NULL, 0, o, &total), S_OK);
    EXPECT_EQ(total, 1);
    if (o[0]) {
        EXPECT_EQ(BS(o[0]), sizeof text);
        CHECK(BS(o[0]) == sizeof text && !memcmp(BP(o[0]), text, sizeof text));
    }
    BREL(o[0]); BREL(out);
}

TEST(gap1_compress_size_for_repetitive_input) {
    SKIP_ON_WINE("compiler API not implemented");
    REQUIRE(UN(D3DCompressShaders));
    static char big[8192]; memset(big, 'A', sizeof big);
    g1_sd sd = { big, sizeof big };
    ID3DBlob *out = NULL;
    HRESULT hr = UN(D3DCompressShaders)(1, &sd, 0, &out);
    if (FAILED(hr)) { t_note("compress of raw data -> %s", d3dh_hr_name(hr)); BREL(out); return; }
    CHECK(BS(out) < sizeof big);                                 /* the point of the API */
    BREL(out);
}

/* ---------------- D3DCreateLinker ---------------- */
TEST(gap1_linker_null_out) {
    SKIP_ON_WINE("compiler API not implemented");
    REQUIRE(U(D3DCreateLinker));
    EXPECT_HR(U(D3DCreateLinker)(NULL), G1_E_INVALIDARG);
}
TEST(gap1_linker_create_and_refcount) {
    SKIP_ON_WINE("compiler API not implemented");
    REQUIRE(U(D3DCreateLinker));
    ID3D11Linker *l = (ID3D11Linker *)(ULONG_PTR)1;
    HRESULT hr = U(D3DCreateLinker)(&l);
    if (FAILED(hr)) { t_note("D3DCreateLinker -> %s", d3dh_hr_name(hr)); return; }
    EXPECT_HR(hr, S_OK);
    CHECK(l != NULL && l != (ID3D11Linker *)(ULONG_PTR)1);
    if (l) {
        G1_EQ(IUnknown_AddRef((IUnknown *)l), 2);
        G1_EQ(IUnknown_Release((IUnknown *)l), 1);
        IUnknown *u = NULL;
        /* Win10: the linker answers only its own IID; even IID_IUnknown is E_NOINTERFACE */
        EXPECT_HR(IUnknown_QueryInterface((IUnknown *)l, &IID_IUnknown, (void **)&u), G1_E_NOINTERFACE);
        CHECK(u == NULL);
        void *again = NULL;
        EXPECT_HR(IUnknown_QueryInterface((IUnknown *)l, &IID_ID3D11Linker, &again), S_OK);
        CHECK(again == (void *)l);
        if (again) G1_EQ(IUnknown_Release((IUnknown *)again), 1);   /* the QI reference */
        void *bad = (void *)1;
        EXPECT_HR(IUnknown_QueryInterface((IUnknown *)l, &IID_ID3D11Module, &bad), G1_E_NOINTERFACE);
        CHECK(bad == NULL);
        G1_EQ(IUnknown_Release((IUnknown *)l), 0);
    }
}
TEST(gap1_linker_two_instances_distinct) {
    SKIP_ON_WINE("compiler API not implemented");
    REQUIRE(U(D3DCreateLinker));
    ID3D11Linker *a = NULL, *b = NULL;
    if (FAILED(U(D3DCreateLinker)(&a)) || FAILED(U(D3DCreateLinker)(&b))) { t_note("D3DCreateLinker failed"); if (a) IUnknown_Release((IUnknown *)a); return; }
    CHECK(a != b);
    IUnknown_Release((IUnknown *)a); IUnknown_Release((IUnknown *)b);
}

/* ---------------- D3DLoadModule ---------------- */
TEST(gap1_load_module_invalid_arguments) {
    SKIP_ON_WINE("compiler API not implemented");
    REQUIRE(U(D3DLoadModule));
    char buf[64]; memset(buf, 0x41, sizeof buf);
    ID3D11Module *m = (ID3D11Module *)(ULONG_PTR)0x2000;
    /* argument validation happens before the output pointer is cleared */
    EXPECT_HR(U(D3DLoadModule)(NULL, sizeof buf, &m), G1_E_INVALIDARG);
    CHECK(m == (ID3D11Module *)(ULONG_PTR)0x2000);
    EXPECT_HR(U(D3DLoadModule)(buf, 0, &m), G1_E_INVALIDARG);
    CHECK(m == (ID3D11Module *)(ULONG_PTR)0x2000);
    EXPECT_HR(U(D3DLoadModule)(buf, 0x7fffffff, &m), G1_E_INVALIDARG);          /* size must be below 0x7fffffff */
    EXPECT_HR(U(D3DLoadModule)(buf, (SIZE_T)0x80000000u, &m), G1_E_INVALIDARG);
    CHECK(m == (ID3D11Module *)(ULONG_PTR)0x2000);
    EXPECT_HR(U(D3DLoadModule)(buf, sizeof buf, NULL), G1_E_INVALIDARG);
}
TEST(gap1_load_module_garbage_clears_output) {
    SKIP_ON_WINE("compiler API not implemented");
    REQUIRE(U(D3DLoadModule));
    char buf[64]; memset(buf, 0x41, sizeof buf);
    ID3D11Module *m = (ID3D11Module *)(ULONG_PTR)0x2000;
    HRESULT hr = U(D3DLoadModule)(buf, sizeof buf, &m);
    EXPECT_HR_FAILED(hr);
    CHECK(m == NULL);
    EXPECT_HR(hr, G1_E_FAIL);   /* D3DLoadModule(garbage): measured on Win10 */
}
TEST(gap1_load_module_rejects_fxc_container) {
    SKIP_ON_WINE("compiler API not implemented");
    REQUIRE(U(D3DLoadModule));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    ID3D11Module *m = (ID3D11Module *)(ULONG_PTR)0x2000;
    HRESULT hr = U(D3DLoadModule)(BP(vs), BS(vs), &m);
    /* a DXBC shader is not a DXIL library */
    EXPECT_HR_FAILED(hr);
    CHECK(m == NULL);
    EXPECT_HR(hr, G1_E_INVALIDARG);   /* D3DLoadModule(vs_5_0 container): measured on Win10 */
    BREL(vs);
}

/* ---------------- D3DReflectLibrary ---------------- */
TEST(gap1_reflect_library_rejects_non_library) {
    SKIP_ON_WINE("D3DReflectLibrary unimplemented");
    REQUIRE(UN(D3DReflectLibrary));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    void *r = NULL;
    HRESULT hr = UN(D3DReflectLibrary)(BP(vs), BS(vs), &IID_IUnknown, &r);
    EXPECT_HR_FAILED(hr);
    CHECK(r == NULL);
    EXPECT_HR(hr, G1_E_INVALIDARG);   /* D3DReflectLibrary(vs_5_0 container): measured on Win10 */
    BREL(vs);
}
TEST(gap1_reflect_library_garbage) {
    SKIP_ON_WINE("D3DReflectLibrary unimplemented");
    REQUIRE(UN(D3DReflectLibrary));
    char buf[96]; memset(buf, 0x5a, sizeof buf);
    void *r = NULL;
    HRESULT hr = UN(D3DReflectLibrary)(buf, sizeof buf, &IID_IUnknown, &r);
    EXPECT_HR_FAILED(hr);
    CHECK(r == NULL);
    EXPECT_HR(hr, G1_D3DERR_INVALIDCALL);   /* D3DReflectLibrary(garbage): measured on Win10 */
}
TEST(gap1_reflect_library_truncated_header) {
    SKIP_ON_WINE("D3DReflectLibrary unimplemented");
    REQUIRE(UN(D3DReflectLibrary));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    void *r = NULL;
    HRESULT hr = UN(D3DReflectLibrary)(BP(vs), 0x18, &IID_IUnknown, &r);
    EXPECT_HR_FAILED(hr);
    CHECK(r == NULL);
    hr = UN(D3DReflectLibrary)(BP(vs), 4, &IID_IUnknown, &r);
    EXPECT_HR_FAILED(hr);
    BREL(vs);
}
