/* Signature-extraction exports: D3D10Get{Input,Output,InputAndOutput}SignatureBlob (d3d10 / d3d10_1)
 * and D3DGetInputAndOutputSignatureBlob (d3dcompiler_47). */
#include "gap0.h"

typedef HRESULT (WINAPI *sigfn_t)(const void *, SIZE_T, ID3DBlob **);

static const struct { const char *name; int in, out; } sig_fns[] = {
    { "D3D10GetInputSignatureBlob", 1, 0 },
    { "D3D10GetOutputSignatureBlob", 0, 1 },
    { "D3D10GetInputAndOutputSignatureBlob", 1, 1 },
};
static const char *const sig_mods[] = { "d3d10.dll", "d3d10_1.dll" };

static ID3DBlob *compile_sig(const char *src, const char *profile)
{
    ID3DBlob *b = NULL;
    HRESULT hr = d3dh_compile(src, "main", profile, 0, &b);
    if (FAILED(hr)) { t_skip("cannot compile %s (hr %s)", profile, d3dh_hr_name(hr)); return NULL; }
    return b;
}

/* Verify a container produced by the D3D10 builder: 'DXBC', version 1, total size == blob size, parts exact. */
static void check_container(ID3DBlob *out, DWORD nparts, const DWORD *fourccs)
{
    const unsigned char *p = d3dh_blob_ptr(out);
    SIZE_T n = d3dh_blob_size(out);
    DWORD i;
    EXPECT_EQ(n >= 32 + 4 * nparts, 1);
    if (n < 32) return;
    CHECK(!memcmp(p, "DXBC", 4));
    EXPECT_EQ(*(const DWORD *)(p + 20), 1);
    EXPECT_EQ(*(const DWORD *)(p + 24), n);
    EXPECT_EQ(gap0_dxbc_nparts(p, n), nparts);
    if (gap0_dxbc_nparts(p, n) != nparts) return;
    for (i = 0; i < nparts; i++) {
        DWORD off = *(const DWORD *)(p + 32 + 4 * i);
        CHECK(off + 8 <= n);
        if (off + 8 > n) return;
        EXPECT_EQ(*(const DWORD *)(p + off), fourccs[i]);
        CHECK(off + 8 + *(const DWORD *)(p + off + 4) <= n);
    }
    if (gap0_is_all_zero(p + 4, 16)) t_note("container hash is all zero");
}

static void run_valid(const char *src, const char *profile, int strict)
{
    ID3DBlob *sh = compile_sig(src, profile);
    size_t m, f;
    if (!sh) return;
    for (m = 0; m < 2; m++) for (f = 0; f < 3; f++) {
        sigfn_t fn = (sigfn_t)t_proc_in(sig_mods[m], sig_fns[f].name);
        ID3DBlob *out = NULL;
        DWORD fc[2], nf = 0, isz = 0, osz = 0;
        const void *ip, *op;
        HRESULT hr;
        if (!fn) { t_skip("%s!%s missing", sig_mods[m], sig_fns[f].name); return; }
        hr = fn(d3dh_blob_ptr(sh), d3dh_blob_size(sh), &out);
        if (!strict) { /* SM5 containers are not understood by the d3d10 signature helpers */
            CHECKF(hr == E_FAIL_HR && out == NULL, "%s!%s on %s: %s", sig_mods[m], sig_fns[f].name, profile, d3dh_hr_name(hr));
            SAFE_RELEASE(out);
            continue;
        }
        EXPECT_HR(hr, S_OK);
        if (FAILED(hr) || !out) continue;
        if (sig_fns[f].in) fc[nf++] = FOURCC('I','S','G','N');
        if (sig_fns[f].out) fc[nf++] = FOURCC('O','S','G','N');
        check_container(out, nf, fc);
        /* payloads are verbatim copies of the source parts */
        ip = gap0_dxbc_part(d3dh_blob_ptr(sh), d3dh_blob_size(sh), FOURCC('I','S','G','N'), &isz);
        op = gap0_dxbc_part(d3dh_blob_ptr(sh), d3dh_blob_size(sh), FOURCC('O','S','G','N'), &osz);
        if (sig_fns[f].in && ip) {
            DWORD s2 = 0; const void *q = gap0_dxbc_part(d3dh_blob_ptr(out), d3dh_blob_size(out), FOURCC('I','S','G','N'), &s2);
            CHECK(q && s2 == isz && !memcmp(q, ip, isz));
        }
        if (sig_fns[f].out && op) {
            DWORD s2 = 0; const void *q = gap0_dxbc_part(d3dh_blob_ptr(out), d3dh_blob_size(out), FOURCC('O','S','G','N'), &s2);
            CHECK(q && s2 == osz && !memcmp(q, op, osz));
        }
        if (!sig_fns[f].in) CHECK(gap0_dxbc_part(d3dh_blob_ptr(out), d3dh_blob_size(out), FOURCC('I','S','G','N'), NULL) == NULL);
        if (!sig_fns[f].out) CHECK(gap0_dxbc_part(d3dh_blob_ptr(out), d3dh_blob_size(out), FOURCC('O','S','G','N'), NULL) == NULL);
        EXPECT_EQ(d3dh_refcount(out), 1);
        { ULONG rc_ = D3DH_RELEASE_RC(out); EXPECT_EQ(rc_, 0); }
    }
    SAFE_RELEASE(sh);
}

TEST(gap0_sig_d3d10_valid_vs40) { run_valid(GAP0_VS_SRC, "vs_4_0", 1); }
TEST(gap0_sig_d3d10_valid_ps40) { run_valid(GAP0_PS_SRC, "ps_4_0", 1); }
TEST(gap0_sig_d3d10_valid_vs41) { run_valid(GAP0_VS_SRC, "vs_4_1", 1); }
TEST(gap0_sig_d3d10_valid_vs50) { run_valid(GAP0_VS_SRC, "vs_5_0", 0); }

TEST(gap0_sig_d3d10_null_data)
{
    size_t m, f;
    for (m = 0; m < 2; m++) for (f = 0; f < 3; f++) {
        sigfn_t fn = (sigfn_t)t_proc_in(sig_mods[m], sig_fns[f].name);
        ID3DBlob *out = (ID3DBlob *)(ULONG_PTR)0x10;   /* sentinel, never dereferenced */
        HRESULT hr;
        if (!fn) { t_skip("missing %s", sig_fns[f].name); return; }
        hr = fn(NULL, 0, &out);
        EXPECT_HR(hr, E_FAIL_HR);
        EXPECT_EQ(out, 0x10);                           /* output untouched on this path */
        out = (ID3DBlob *)(ULONG_PTR)0x10;
        hr = fn(NULL, 64, &out);
        EXPECT_HR(hr, E_FAIL_HR);
        EXPECT_EQ(out, 0x10);
    }
}

TEST(gap0_sig_d3d10_null_out_success)
{
    ID3DBlob *sh = compile_sig(GAP0_VS_SRC, "vs_4_0");
    size_t m, f;
    if (!sh) return;
    for (m = 0; m < 2; m++) for (f = 0; f < 3; f++) {
        sigfn_t fn = (sigfn_t)t_proc_in(sig_mods[m], sig_fns[f].name);
        if (!fn) continue;
        /* the output pointer is only dereferenced when the container is built; a NULL one is not an error here */
        HRESULT hr = fn(d3dh_blob_ptr(sh), d3dh_blob_size(sh), NULL);
        CHECKF(hr == S_OK, "%s!%s(valid, NULL out) = %s", sig_mods[m], sig_fns[f].name, d3dh_hr_name(hr));
    }
    SAFE_RELEASE(sh);
}

TEST(gap0_sig_d3d10_malformed)
{
    ID3DBlob *sh = compile_sig(GAP0_VS_SRC, "vs_4_0");
    size_t m, f;
    unsigned char *copy;
    SIZE_T n;
    if (!sh) return;
    n = d3dh_blob_size(sh);
    copy = malloc(n + 64);
    for (m = 0; m < 2; m++) for (f = 0; f < 3; f++) {
        sigfn_t fn = (sigfn_t)t_proc_in(sig_mods[m], sig_fns[f].name);
        ID3DBlob *out;
        HRESULT hr;
        if (!fn) continue;
        /* all-zero buffer */
        memset(copy, 0, n + 64); out = NULL;
        hr = fn(copy, 64, &out); EXPECT_HR_FAILED(hr); CHECK(out == NULL);
        /* wrong magic */
        memcpy(copy, d3dh_blob_ptr(sh), n); copy[3] = 'D' + 1; out = NULL;
        hr = fn(copy, n, &out); EXPECT_HR_FAILED(hr); CHECK(out == NULL);
        /* zero size, short header sizes */
        memcpy(copy, d3dh_blob_ptr(sh), n);
        { static const SIZE_T sizes[] = { 0, 4, 20, 31 }; int i;
          for (i = 0; i < 4; i++) { out = NULL; hr = fn(copy, sizes[i], &out); EXPECT_HR_FAILED(hr); CHECK(out == NULL); } }
        /* container cut one byte short (declared size > supplied size) */
        out = NULL; hr = fn(copy, n - 1, &out);
        CHECKF(FAILED(hr), "%s!%s accepted a container truncated by one byte", sig_mods[m], sig_fns[f].name);
        SAFE_RELEASE(out);
        /* corrupted hash */
        memcpy(copy, d3dh_blob_ptr(sh), n); copy[4] ^= 0xff; out = NULL;
        hr = fn(copy, n, &out);
        CHECKF(hr == E_FAIL_HR && out == NULL, "%s!%s with corrupted checksum = %s", sig_mods[m], sig_fns[f].name, d3dh_hr_name(hr));
        SAFE_RELEASE(out);
    }
    free(copy);
    SAFE_RELEASE(sh);
}

TEST(gap0_sig_d3d10_parity_both_equals_parts)
{
    /* InputAndOutput == Input part followed by Output part, identical across d3d10 and d3d10_1 */
    ID3DBlob *sh = compile_sig(GAP0_PS_SRC, "ps_4_0"), *a = NULL, *b = NULL;
    sigfn_t f0, f1;
    if (!sh) return;
    f0 = (sigfn_t)t_proc_in("d3d10.dll", "D3D10GetInputAndOutputSignatureBlob");
    f1 = (sigfn_t)t_proc_in("d3d10_1.dll", "D3D10GetInputAndOutputSignatureBlob");
    if (!f0 || !f1) { t_skip("exports missing"); SAFE_RELEASE(sh); return; }
    EXPECT_HR(f0(d3dh_blob_ptr(sh), d3dh_blob_size(sh), &a), S_OK);
    EXPECT_HR(f1(d3dh_blob_ptr(sh), d3dh_blob_size(sh), &b), S_OK);
    if (a && b) {
        EXPECT_EQ(d3dh_blob_size(a), d3dh_blob_size(b));
        if (d3dh_blob_size(a) == d3dh_blob_size(b)) CHECK(!memcmp(d3dh_blob_ptr(a), d3dh_blob_ptr(b), d3dh_blob_size(a)));
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(sh);
}

/* ---- D3DGetInputAndOutputSignatureBlob (d3dcompiler_47) ---- */

static void run_d3dc_valid(const char *src, const char *profile)
{
    typedef HRESULT (WINAPI *fn_t)(const void *, SIZE_T, ID3DBlob **);
    fn_t fn = (fn_t)t_proc_in("d3dcompiler_47.dll", "D3DGetInputAndOutputSignatureBlob");
    ID3DBlob *sh, *out = NULL;
    const unsigned char *p;
    SIZE_T n;
    DWORD isz = 0, osz = 0;
    const void *i1, *o1;
    REQUIRE(fn);
    sh = compile_sig(src, profile);
    if (!sh) return;
    EXPECT_HR(fn(d3dh_blob_ptr(sh), d3dh_blob_size(sh), &out), S_OK);
    if (out) {
        p = d3dh_blob_ptr(out); n = d3dh_blob_size(out);
        CHECK(n >= 32 && !memcmp(p, "DXBC", 4));
        EXPECT_EQ(*(const DWORD *)(p + 24), n);
        EXPECT_EQ(gap0_dxbc_nparts(p, n), 2);
        i1 = gap0_dxbc_part(p, n, FOURCC('I','S','G','N'), &isz);
        if (!i1) i1 = gap0_dxbc_part(p, n, FOURCC('I','S','G','1'), &isz);
        o1 = gap0_dxbc_part(p, n, FOURCC('O','S','G','N'), &osz);
        if (!o1) o1 = gap0_dxbc_part(p, n, FOURCC('O','S','G','1'), &osz);
        CHECK(i1 != NULL); CHECK(o1 != NULL);
        if (gap0_dxbc_nparts(p, n) == 2) {
            DWORD a = gap0_dxbc_part_fourcc(p, 0), b = gap0_dxbc_part_fourcc(p, 1);
            CHECK(a == FOURCC('I','S','G','N') || a == FOURCC('I','S','G','1'));
            CHECK(b == FOURCC('O','S','G','N') || b == FOURCC('O','S','G','1'));
        }
        EXPECT_EQ(d3dh_refcount(out), 1);
        SAFE_RELEASE(out);
    }
    SAFE_RELEASE(sh);
}
TEST(gap0_sig_d3dc_valid_vs40) { run_d3dc_valid(GAP0_VS_SRC, "vs_4_0"); }
TEST(gap0_sig_d3dc_valid_ps40) { run_d3dc_valid(GAP0_PS_SRC, "ps_4_0"); }
TEST(gap0_sig_d3dc_valid_vs50) { run_d3dc_valid(GAP0_VS_SRC, "vs_5_0"); }
TEST(gap0_sig_d3dc_valid_ps50) { run_d3dc_valid(GAP0_PS_SRC, "ps_5_0"); }

TEST(gap0_sig_d3dc_null_out)
{
    typedef HRESULT (WINAPI *fn_t)(const void *, SIZE_T, ID3DBlob **);
    fn_t fn = (fn_t)t_proc_in("d3dcompiler_47.dll", "D3DGetInputAndOutputSignatureBlob");
    ID3DBlob *sh;
    REQUIRE(fn);
    sh = compile_sig(GAP0_VS_SRC, "vs_4_0");
    if (!sh) return;
    EXPECT_HR(fn(d3dh_blob_ptr(sh), d3dh_blob_size(sh), NULL), INVALIDCALL_HR);
    EXPECT_HR(fn(NULL, 0, NULL), INVALIDCALL_HR);
    SAFE_RELEASE(sh);
}

TEST(gap0_sig_d3dc_malformed)
{
    typedef HRESULT (WINAPI *fn_t)(const void *, SIZE_T, ID3DBlob **);
    fn_t fn = (fn_t)t_proc_in("d3dcompiler_47.dll", "D3DGetInputAndOutputSignatureBlob");
    ID3DBlob *sh, *out;
    unsigned char zeros[64] = { 0 };
    unsigned char *copy;
    SIZE_T n;
    static const SIZE_T sizes[] = { 0, 4, 20, 31 };
    int i;
    REQUIRE(fn);
    sh = compile_sig(GAP0_VS_SRC, "vs_4_0");
    if (!sh) return;
    n = d3dh_blob_size(sh);
    copy = malloc(n);
    out = NULL; EXPECT_HR_FAILED(fn(NULL, 0, &out)); CHECK(out == NULL);
    out = NULL; EXPECT_HR_FAILED(fn(zeros, sizeof zeros, &out)); CHECK(out == NULL);
    memcpy(copy, d3dh_blob_ptr(sh), n);
    for (i = 0; i < 4; i++) { out = NULL; EXPECT_HR_FAILED(fn(copy, sizes[i], &out)); CHECK(out == NULL); }
    copy[0] = 'X'; out = NULL; EXPECT_HR_FAILED(fn(copy, n, &out)); CHECK(out == NULL);
    free(copy);
    SAFE_RELEASE(sh);
}

TEST(gap0_sig_d3dc_matches_d3d10_both)
{
    typedef HRESULT (WINAPI *fn_t)(const void *, SIZE_T, ID3DBlob **);
    fn_t fc = (fn_t)t_proc_in("d3dcompiler_47.dll", "D3DGetInputAndOutputSignatureBlob");
    fn_t f10 = (fn_t)t_proc_in("d3d10.dll", "D3D10GetInputAndOutputSignatureBlob");
    ID3DBlob *sh, *a = NULL, *b = NULL;
    REQUIRE(fc); REQUIRE(f10);
    sh = compile_sig(GAP0_VS_SRC, "vs_4_0");
    if (!sh) return;
    if (SUCCEEDED(fc(d3dh_blob_ptr(sh), d3dh_blob_size(sh), &a)) && SUCCEEDED(f10(d3dh_blob_ptr(sh), d3dh_blob_size(sh), &b))) {
        DWORD s1 = 0, s2 = 0;
        const void *x = gap0_dxbc_part(d3dh_blob_ptr(a), d3dh_blob_size(a), FOURCC('I','S','G','N'), &s1);
        const void *y = gap0_dxbc_part(d3dh_blob_ptr(b), d3dh_blob_size(b), FOURCC('I','S','G','N'), &s2);
        CHECK(x && y && s1 == s2 && !memcmp(x, y, s1));
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(sh);
}
