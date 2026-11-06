/* d3dcompiler_47: malformed / truncated / tampered DXBC containers fed to every container-consuming entry point.
 * Structural damage is reported as D3DERR_INVALIDCALL, a checksum mismatch as E_FAIL, an unusable shader as E_INVALIDARG. */
#include "rb.h"

#define SRC "struct VI { float4 p : POSITION; float2 uv : TEXCOORD0; };\ncbuffer CB : register(b0) { float4x4 m; };\n" \
            "float4 main(VI i, out float2 uvo : TEXCOORD0) : SV_Position { uvo = i.uv; return mul(i.p, m); }\n"

typedef HRESULT (WINAPI *Part_t)(const void *, SIZE_T, D3D_BLOB_PART, UINT, ID3DBlob **);
typedef HRESULT (WINAPI *Strip_t)(const void *, SIZE_T, UINT, ID3DBlob **);
typedef HRESULT (WINAPI *Sig_t)(const void *, SIZE_T, ID3DBlob **);
typedef HRESULT (WINAPI *Set_t)(const void *, SIZE_T, D3D_BLOB_PART, UINT, const void *, SIZE_T, ID3DBlob **);

static void rb_probe_all(const void *p, SIZE_T n, HRESULT want, const char *label)
{
    ID3DBlob *o; void *r; HRESULT hr;
    Part_t part = (Part_t)t_proc_in("d3dcompiler_47.dll", "D3DGetBlobPart"); Strip_t strip = (Strip_t)t_proc_in("d3dcompiler_47.dll", "D3DStripShader");
    Sig_t in = (Sig_t)t_proc_in("d3dcompiler_47.dll", "D3DGetInputSignatureBlob"), out = (Sig_t)t_proc_in("d3dcompiler_47.dll", "D3DGetOutputSignatureBlob");
    Sig_t io = (Sig_t)t_proc_in("d3dcompiler_47.dll", "D3DGetInputAndOutputSignatureBlob"), dbg = (Sig_t)t_proc_in("d3dcompiler_47.dll", "D3DGetDebugInfo");
    Set_t set = (Set_t)t_proc_in("d3dcompiler_47.dll", "D3DSetBlobPart");
#define PROBE(name, call) do { o = NULL; r = NULL; hr = (call); t_check(hr == want, __FILE__, __LINE__, name " result", "%s: got %s want %s", label, d3dh_hr_name(hr), d3dh_hr_name(want)); if (SUCCEEDED(hr)) { SAFE_RELEASE(o); if (r) ((IUnknown *)r)->lpVtbl->Release((IUnknown *)r); } } while (0)
    PROBE("D3DReflect", d3dh_D3DReflect()(p, n, &IID_ID3D11ShaderReflection, &r));
    PROBE("D3DGetBlobPart", part(p, n, D3D_BLOB_INPUT_SIGNATURE_BLOB, 0, &o));
    PROBE("D3DGetInputSignatureBlob", in(p, n, &o));
    PROBE("D3DGetOutputSignatureBlob", out(p, n, &o));
    PROBE("D3DGetInputAndOutputSignatureBlob", io(p, n, &o));
    PROBE("D3DGetDebugInfo", dbg(p, n, &o));
    PROBE("D3DStripShader", strip(p, n, 1, &o));
    if (!t_is_wine()) PROBE("D3DSetBlobPart", set(p, n, D3D_BLOB_PRIVATE_DATA, 0, "abcd", 4, &o));
    if (!t_is_wine() && RB_D3DReflectLibrary()) PROBE("D3DReflectLibrary", RB_D3DReflectLibrary()(p, n, &RB_IID_LibraryReflection, &r));
#undef PROBE
}
/* Every entry point must accept a well-formed container (debug info is legitimately absent). */
static void rb_probe_valid(const void *p, SIZE_T n, const char *label)
{
    ID3DBlob *o = NULL; void *r = NULL; HRESULT hr;
    Part_t part = (Part_t)t_proc_in("d3dcompiler_47.dll", "D3DGetBlobPart"); Strip_t strip = (Strip_t)t_proc_in("d3dcompiler_47.dll", "D3DStripShader");
    Sig_t io = (Sig_t)t_proc_in("d3dcompiler_47.dll", "D3DGetInputAndOutputSignatureBlob");
#define VPROBE(name, call) do { o = NULL; r = NULL; hr = (call); t_check(hr == S_OK, __FILE__, __LINE__, name " accepts container", "%s: got %s", label, d3dh_hr_name(hr)); if (SUCCEEDED(hr)) { SAFE_RELEASE(o); if (r) ((IUnknown *)r)->lpVtbl->Release((IUnknown *)r); } } while (0)
    VPROBE("D3DReflect", d3dh_D3DReflect()(p, n, &IID_ID3D11ShaderReflection, &r));
    VPROBE("D3DGetBlobPart", part(p, n, D3D_BLOB_INPUT_SIGNATURE_BLOB, 0, &o));
    VPROBE("D3DGetInputAndOutputSignatureBlob", io(p, n, &o));
    VPROBE("D3DStripShader", strip(p, n, 1, &o));
#undef VPROBE
}
static BYTE *rb_copy(ID3DBlob *b) { BYTE *c = malloc(RB_BS(b)); memcpy(c, RB_BP(b), RB_BS(b)); return c; }
#define PUT32(buf, off, v) (*(DWORD *)((BYTE *)(buf) + (off)) = (DWORD)(v))
#define GET32(buf, off) (*(const DWORD *)((const BYTE *)(buf) + (off)))

/* A test with a fresh private copy `c` of a compiled shader, `n` its size; body edits c / n then probes. */
#define MCASE(nm, ...) \
TEST(rb_invalid_##nm) { RB_COMPILE(b, SRC, "vs_5_0"); BYTE *c = rb_copy(b); SIZE_T n = RB_BS(b); (void)c; (void)n; { __VA_ARGS__ } free(c); ID3D10Blob_Release(b); }

MCASE(valid_baseline_probes_ok, { rb_probe_valid(c, n, "compiler output"); })
MCASE(null_data_pointer, { rb_probe_all(NULL, 0x100, RB_INVALIDCALL, "NULL data"); rb_probe_all(NULL, 0, RB_INVALIDCALL, "NULL data size 0"); })
MCASE(size_zero, { rb_probe_all(c, 0, RB_INVALIDCALL, "size 0"); })
MCASE(size_tiny, {
    static const SIZE_T sz[] = { 1, 2, 3, 4, 7, 8, 16, 20, 24, 28, 31 };
    for (size_t i = 0; i < sizeof sz / sizeof *sz; i++) { char l[32]; sprintf(l, "size %zu", sz[i]); rb_probe_all(c, sz[i], RB_INVALIDCALL, l); }
})
MCASE(size_one_less_than_container, { rb_probe_all(c, n - 1, RB_INVALIDCALL, "size-1"); rb_probe_all(c, n - 4, RB_INVALIDCALL, "size-4"); })
MCASE(size_larger_than_container_field, { BYTE *big = calloc(1, n + 16); memcpy(big, c, n); rb_probe_all(big, n + 4, RB_INVALIDCALL, "size+4"); rb_probe_all(big, n + 1, RB_INVALIDCALL, "size+1"); free(big); })
MCASE(size_exactly_header_only, { rb_probe_all(c, 0x20, RB_INVALIDCALL, "header only with stale size field"); })
MCASE(truncated_at_every_chunk_boundary, {
    rb_chunk ch[RB_MAXCHUNKS]; int cnt = rb_dxbc_parse(c, n, ch, RB_MAXCHUNKS);
    for (int i = 0; i < cnt; i++) { SIZE_T cut = (SIZE_T)((const BYTE *)ch[i].data - c) - 8; char l[48]; sprintf(l, "cut before chunk %d", i); rb_probe_all(c, cut, RB_INVALIDCALL, l); }
})
MCASE(bad_magic_each_byte, {
    for (int i = 0; i < 4; i++) { char l[32]; c[i] ^= 0x20; sprintf(l, "magic byte %d", i); rb_probe_all(c, n, RB_INVALIDCALL, l); c[i] ^= 0x20; }
    memcpy(c, "dxbc", 4); rb_probe_all(c, n, RB_INVALIDCALL, "lowercase magic");
    memset(c, 0, 4); rb_probe_all(c, n, RB_INVALIDCALL, "zero magic");
})
MCASE(bad_version_fields, {
    static const struct { int off; BYTE v; } t[] = { { 0x14, 2 }, { 0x14, 0 }, { 0x15, 1 }, { 0x16, 1 }, { 0x17, 1 } };
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) { BYTE keep = c[t[i].off]; char l[48]; c[t[i].off] = t[i].v; rb_dxbc_rehash(c, n); sprintf(l, "version byte %d = %d", t[i].off, t[i].v); rb_probe_all(c, n, RB_INVALIDCALL, l); c[t[i].off] = keep; }
})
MCASE(total_size_field_mismatch, {
    DWORD good = GET32(c, 0x18); static const int delta[] = { 4, -4, 1, -1, 0x100, -0x100 };
    for (size_t i = 0; i < sizeof delta / sizeof *delta; i++) { char l[40]; PUT32(c, 0x18, good + delta[i]); sprintf(l, "size field %+d", delta[i]); rb_probe_all(c, n, RB_INVALIDCALL, l); }
    PUT32(c, 0x18, 0); rb_probe_all(c, n, RB_INVALIDCALL, "size field 0");
    PUT32(c, 0x18, 0xffffffffu); rb_probe_all(c, n, RB_INVALIDCALL, "size field -1");
    PUT32(c, 0x18, 0x80000000u); rb_probe_all(c, n, RB_INVALIDCALL, "size field 2GB");
})
MCASE(chunk_count_inconsistent, {
    DWORD good = GET32(c, 0x1c);
    PUT32(c, 0x1c, good + 1); rb_probe_all(c, n, RB_INVALIDCALL, "count+1");
    PUT32(c, 0x1c, good - 1); rb_probe_all(c, n, RB_INVALIDCALL, "count-1");
    PUT32(c, 0x1c, 0xffffffffu); rb_probe_all(c, n, RB_INVALIDCALL, "count -1");
    PUT32(c, 0x1c, 0x40000000u); rb_probe_all(c, n, RB_INVALIDCALL, "count 0x40000000");
    PUT32(c, 0x1c, 1000); rb_probe_all(c, n, RB_INVALIDCALL, "count 1000");
})
MCASE(first_chunk_offset_wrong, {
    DWORD good = GET32(c, 0x20); static const int delta[] = { 4, -4, 1, 0x10000 };
    for (size_t i = 0; i < sizeof delta / sizeof *delta; i++) { char l[40]; PUT32(c, 0x20, good + delta[i]); sprintf(l, "first offset %+d", delta[i]); rb_probe_all(c, n, RB_INVALIDCALL, l); }
    PUT32(c, 0x20, 0); rb_probe_all(c, n, RB_INVALIDCALL, "first offset 0");
    PUT32(c, 0x20, 0xfffffff0u); rb_probe_all(c, n, RB_INVALIDCALL, "first offset near 4GB");
})
MCASE(second_chunk_offset_wrong, {
    DWORD good = GET32(c, 0x24); static const int delta[] = { 4, -4, 8, 0x100000 };
    for (size_t i = 0; i < sizeof delta / sizeof *delta; i++) { char l[40]; PUT32(c, 0x24, good + delta[i]); sprintf(l, "second offset %+d", delta[i]); rb_probe_all(c, n, RB_INVALIDCALL, l); }
    PUT32(c, 0x24, 0xffffffffu); rb_probe_all(c, n, RB_INVALIDCALL, "second offset -1");
})
MCASE(chunk_offsets_swapped, { DWORD a = GET32(c, 0x20), bb = GET32(c, 0x24); PUT32(c, 0x20, bb); PUT32(c, 0x24, a); rb_probe_all(c, n, RB_INVALIDCALL, "offsets swapped"); })
MCASE(chunk_size_field_wrong, {
    rb_chunk ch[RB_MAXCHUNKS]; int cnt = rb_dxbc_parse(c, n, ch, RB_MAXCHUNKS);
    for (int i = 0; i < cnt; i++) {
        DWORD off = GET32(c, 0x20 + i * 4), good = GET32(c, off + 4); char l[48];
        PUT32(c, off + 4, good + 4); sprintf(l, "chunk %d size +4", i); rb_probe_all(c, n, RB_INVALIDCALL, l);
        /* the last chunk is not followed by another offset, so a shorter size is only caught by the checksum */
        if (good >= 4) { PUT32(c, off + 4, good - 4); sprintf(l, "chunk %d size -4", i); rb_probe_all(c, n, i == cnt - 1 ? RB_E_FAIL : RB_INVALIDCALL, l); }
        /* the end-of-chunk test is a 32-bit sum (offset + size + 7): a huge size can wrap into range, and then only the last chunk
         * (no following offset to contradict it) survives structural validation and is rejected by the checksum instead */
        {
            static const DWORD huge[] = { 0xffffff00u, 0xfffffffcu };
            for (int k = 0; k < 2; k++) {
                DWORD wrapped = huge[k] + 7 + off; HRESULT want = (wrapped > (DWORD)n || i != cnt - 1) ? RB_INVALIDCALL : RB_E_FAIL;
                PUT32(c, off + 4, huge[k]); sprintf(l, "chunk %d size 0x%08x", i, huge[k]); rb_probe_all(c, n, want, l);
            }
        }
        PUT32(c, off + 4, good);
    }
})
MCASE(slack_between_chunks_is_invalid, {
    rb_chunk ch[RB_MAXCHUNKS]; int cnt = rb_dxbc_parse(c, n, ch, RB_MAXCHUNKS); size_t sz = 0; BYTE *g;
    CHECK(cnt >= 3);
    g = rb_dxbc_build_gap(ch, cnt, 1, 4, &sz);
    if (g) { rb_probe_all(g, sz, RB_INVALIDCALL, "4 bytes of slack before chunk 1"); free(g); }
    g = rb_dxbc_build_gap(ch, cnt, 0, 8, &sz);
    if (g) { rb_probe_all(g, sz, RB_INVALIDCALL, "8 bytes of slack before chunk 0"); free(g); }
})
MCASE(rebuilt_container_without_slack_is_valid, {
    rb_chunk ch[RB_MAXCHUNKS]; int cnt = rb_dxbc_parse(c, n, ch, RB_MAXCHUNKS); size_t sz = 0; BYTE *g = rb_dxbc_build(ch, cnt, &sz);
    CHECK(g != NULL);
    if (g) { CHECK(sz == n); CHECK(!memcmp(g, c, n)); rb_probe_valid(g, sz, "rebuilt"); free(g); }
})
MCASE(trailing_slack_after_last_chunk_is_accepted, {
    BYTE *g = calloc(1, n + 8); memcpy(g, c, n); PUT32(g, 0x18, n + 8); rb_dxbc_rehash(g, n + 8);
    ID3DBlob *o = NULL;
    EXPECT_HR(((Part_t)t_proc_in("d3dcompiler_47.dll", "D3DGetBlobPart"))(g, n + 8, D3D_BLOB_INPUT_SIGNATURE_BLOB, 0, &o), S_OK); SAFE_RELEASE(o);
    free(g);
})
MCASE(checksum_each_byte_flipped, {
    for (int i = 4; i < 20; i++) { char l[32]; c[i] ^= 0x01; sprintf(l, "hash byte %d", i - 4); rb_probe_all(c, n, RB_E_FAIL, l); c[i] ^= 0x01; }
})
MCASE(checksum_zeroed, { memset(c + 4, 0, 16); rb_probe_all(c, n, RB_E_FAIL, "zero hash"); memset(c + 4, 0xff, 16); rb_probe_all(c, n, RB_E_FAIL, "all-ones hash"); })
MCASE(payload_tampered_without_rehash, {
    c[n - 1] ^= 0x80; rb_probe_all(c, n, RB_E_FAIL, "last byte flipped");
    c[n - 1] ^= 0x80; c[0x40] ^= 1; rb_probe_all(c, n, RB_E_FAIL, "byte at 0x40 flipped");
})
MCASE(chunk_fourcc_tampered_without_rehash, { DWORD off = GET32(c, 0x20); c[off] ^= 0x01; rb_probe_all(c, n, RB_E_FAIL, "fourcc flipped"); })
MCASE(payload_tampered_with_rehash_still_container, {
    ID3DBlob *o = NULL; Part_t part = (Part_t)t_proc_in("d3dcompiler_47.dll", "D3DGetBlobPart");
    c[n - 1] ^= 0x80; rb_dxbc_rehash(c, n);
    EXPECT_HR(part(c, n, D3D_BLOB_INPUT_SIGNATURE_BLOB, 0, &o), S_OK); SAFE_RELEASE(o);
})
MCASE(zero_chunk_container, {
    ID3DBlob *o = NULL; void *r = NULL; size_t sz = 0; BYTE *z = rb_dxbc_build(NULL, 0, &sz);
    CHECK(z && sz == 0x20);
    if (z) {
        EXPECT_HR(((Part_t)t_proc_in("d3dcompiler_47.dll", "D3DGetBlobPart"))(z, sz, D3D_BLOB_INPUT_SIGNATURE_BLOB, 0, &o), RB_E_FAIL);
        EXPECT_HR(d3dh_D3DReflect()(z, sz, &IID_ID3D11ShaderReflection, &r), RB_E_INVALIDARG);
        free(z);
    }
})

/* ---- D3DReflect: a structurally valid container that is not a usable shader ---------------------- */
#define RCASE(nm, drop, want) \
TEST(rb_invalid_reflect_##nm) { RB_COMPILE(b, SRC, "vs_5_0"); size_t sz = 0; BYTE *x = rb_dxbc_edit(RB_BP(b), RB_BS(b), (drop), 0, NULL, 0, &sz); void *r = (void *)1; HRESULT hr; \
    CHECK(x != NULL); if (x) { hr = d3dh_D3DReflect()(x, sz, &IID_ID3D11ShaderReflection, &r); t_check(hr == (want), __FILE__, __LINE__, "D3DReflect", "got %s want %s", d3dh_hr_name(hr), d3dh_hr_name(want)); if (SUCCEEDED(hr) && r) ((IUnknown *)r)->lpVtbl->Release((IUnknown *)r); free(x); } ID3D10Blob_Release(b); }
RCASE(without_shader_code, FCC_SHEX, RB_E_INVALIDARG)
RCASE(without_input_signature, FCC_ISGN, RB_E_INVALIDARG)
RCASE(without_output_signature, FCC_OSGN, RB_E_INVALIDARG)
RCASE(without_resource_definition, FCC_RDEF, S_OK)
RCASE(without_statistics, FCC_STAT, S_OK)
RCASE(without_patch_constant_signature, FCC_PCSG, S_OK)
RCASE(without_unknown_chunk, 0x12345678, S_OK)

TEST(rb_invalid_reflect_unknown_iid)
{
    RB_COMPILE(b, SRC, "vs_5_0"); void *r = (void *)1; HRESULT hr;
    hr = d3dh_D3DReflect()(RB_BP(b), RB_BS(b), &IID_IUnknown, &r);
    t_check(hr == RB_E_INVALIDARG, __FILE__, __LINE__, "IUnknown", "got %s", d3dh_hr_name(hr)); if (SUCCEEDED(hr) && r) ((IUnknown *)r)->lpVtbl->Release((IUnknown *)r);
    r = (void *)1; hr = d3dh_D3DReflect()(RB_BP(b), RB_BS(b), &IID_ID3D11Device, &r);
    EXPECT_HR(hr, RB_E_INVALIDARG);
    r = (void *)1; hr = d3dh_D3DReflect()(RB_BP(b), RB_BS(b), &IID_ID3D11ShaderReflectionType, &r);
    EXPECT_HR(hr, RB_E_INVALIDARG);
    ID3D10Blob_Release(b);
}
TEST(rb_invalid_reflect_garbage_shader_code)
{
    /* container checksum fine, but the shader token stream is nonsense */
    RB_COMPILE(b, SRC, "vs_5_0"); rb_chunk ch[RB_MAXCHUNKS]; int cnt = rb_dxbc_parse(RB_BP(b), RB_BS(b), ch, RB_MAXCHUNKS), k = 0; rb_chunk o[RB_MAXCHUNKS]; BYTE zero[64] = { 0 }; size_t sz; BYTE *x; void *r = NULL; HRESULT hr;
    for (int i = 0; i < cnt; i++) { o[k] = ch[i]; if (ch[i].fourcc == FCC_SHEX || ch[i].fourcc == FCC_SHDR) { o[k].data = zero; o[k].size = sizeof zero; } k++; }
    x = rb_dxbc_build(o, k, &sz);
    hr = d3dh_D3DReflect()(x, sz, &IID_ID3D11ShaderReflection, &r);
    t_note("reflect of zeroed shader tokens: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && r) ((IUnknown *)r)->lpVtbl->Release((IUnknown *)r); else CHECK(hr == RB_E_INVALIDARG || hr == RB_E_FAIL);
    free(x); ID3D10Blob_Release(b);
}
/* D3DReflect on a container whose SHDR/SHEX chunk is cut to a few bytes (valid checksum) is not validated by Win10 22H2
 * d3dcompiler_47 (build 19041.868): depending on the size it returns S_OK, spins forever at 100% CPU (sizes 1, 3, 4, 5, 24),
 * or dies with a C++ exception (sizes 8, 12, 28, 40; bad_alloc under a memory limit). Nothing here can be pinned, and an
 * unbounded call would hang the whole run, so the call runs in a child process under a job object (300 MB, 10 s). */
CHILD(rb_reflect_cut_shader)
{
    ID3DBlob *b = NULL; rb_chunk ch[RB_MAXCHUNKS], o[RB_MAXCHUNKS]; size_t sz; BYTE *x; void *r = NULL; HRESULT hr; int cnt, k = 0;
    DWORD cut = argc > 1 ? (DWORD)atoi(argv[1]) : 4;
    if (FAILED(d3dh_compile(SRC, "main", "vs_5_0", 0, &b))) return 90;
    cnt = rb_dxbc_parse(RB_BP(b), RB_BS(b), ch, RB_MAXCHUNKS);
    for (int i = 0; i < cnt; i++) { o[k] = ch[i]; if (ch[i].fourcc == FCC_SHEX || ch[i].fourcc == FCC_SHDR) o[k].size = cut; k++; }
    x = rb_dxbc_build(o, k, &sz);
    hr = d3dh_D3DReflect()(x, sz, &IID_ID3D11ShaderReflection, &r);
    return SUCCEEDED(hr) ? 0 : 1;
}
/* 0 completed S_OK, 1 completed with failure, 2 timed out (killed), 3 killed/exception, 4 could not run */
static int rb_run_cut_child(DWORD cut)
{
    char a[16]; PROCESS_INFORMATION pi; HANDLE job = CreateJobObjectA(NULL, NULL), h; JOBOBJECT_EXTENDED_LIMIT_INFORMATION li; DWORD w, ec = 0; int res;
    memset(&li, 0, sizeof li); li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE; li.ProcessMemoryLimit = 300u << 20;
    if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof li)) { if (job) CloseHandle(job); return 4; }
    snprintf(a, sizeof a, "%lu", (unsigned long)cut);
    h = t_spawn_child("rb_reflect_cut_shader", a, CREATE_SUSPENDED, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) { CloseHandle(job); return 4; }
    AssignProcessToJobObject(job, h); ResumeThread(pi.hThread);
    w = WaitForSingleObject(h, 10000);
    if (w != WAIT_OBJECT_0) { TerminateProcess(h, 9); WaitForSingleObject(h, 5000); res = 2; }
    else { GetExitCodeProcess(h, &ec); res = ec == 0 ? 0 : ec == 1 ? 1 : 3; }
    CloseHandle(pi.hThread); CloseHandle(h); CloseHandle(job);
    return res;
}
TEST(rb_invalid_reflect_truncated_shader_chunk)
{
    static const DWORD cuts[] = { 0, 4, 8, 16, 32 }; static const char *const nm[] = { "S_OK", "failed", "hang (killed after 10 s)", "exception", "not run" };
    if (t_is_wine()) { t_skip("emulator"); return; }
    for (int i = 0; i < 5; i++) {
        int r = rb_run_cut_child(cuts[i]);
        CHECKF(r != 4, "child for shader chunk size %lu did not start", cuts[i]);
        t_note("reflect of a %lu-byte shader chunk: %s", cuts[i], nm[r]);
    }
}
TEST(rb_invalid_reflect_truncated_resource_definition)
{
    RB_COMPILE(b, SRC, "vs_5_0"); rb_chunk ch[RB_MAXCHUNKS]; int cnt = rb_dxbc_parse(RB_BP(b), RB_BS(b), ch, RB_MAXCHUNKS), k = 0; rb_chunk o[RB_MAXCHUNKS]; size_t sz; BYTE *x; void *r = NULL; HRESULT hr;
    for (int i = 0; i < cnt; i++) { o[k] = ch[i]; if (ch[i].fourcc == FCC_RDEF) o[k].size = 16; k++; }
    x = rb_dxbc_build(o, k, &sz);
    if (t_is_wine()) { t_skip("emulator reads past the chunk"); free(x); ID3D10Blob_Release(b); return; }
    hr = d3dh_D3DReflect()(x, sz, &IID_ID3D11ShaderReflection, &r);
    EXPECT_HR(hr, RB_E_INVALIDARG); /* resource definitions shorter than 0x1c bytes are rejected */
    if (SUCCEEDED(hr) && r) ((IUnknown *)r)->lpVtbl->Release((IUnknown *)r);
    free(x); ID3D10Blob_Release(b);
}
TEST(rb_invalid_reflect_truncated_signature)
{
    RB_COMPILE(b, SRC, "vs_5_0"); rb_chunk ch[RB_MAXCHUNKS]; int cnt = rb_dxbc_parse(RB_BP(b), RB_BS(b), ch, RB_MAXCHUNKS), k = 0; rb_chunk o[RB_MAXCHUNKS]; size_t sz; BYTE *x; void *r = NULL; HRESULT hr;
    for (int i = 0; i < cnt; i++) { o[k] = ch[i]; if (ch[i].fourcc == FCC_ISGN) o[k].size = 12; k++; }
    x = rb_dxbc_build(o, k, &sz);
    if (t_is_wine()) { t_skip("emulator reads past the chunk"); free(x); ID3D10Blob_Release(b); return; }
    hr = d3dh_D3DReflect()(x, sz, &IID_ID3D11ShaderReflection, &r);
    t_note("reflect with 12-byte ISGN: %s", d3dh_hr_name(hr)); /* only the container end bounds the element table, not the chunk */
    if (SUCCEEDED(hr) && r) ((IUnknown *)r)->lpVtbl->Release((IUnknown *)r);
    free(x); ID3D10Blob_Release(b);
}
TEST(rb_invalid_reflect_non_dxbc_inputs)
{
    static const char txt[] = "float4 main() : SV_Target { return 0; }";
    BYTE zeros[256] = { 0 }; void *r = (void *)1; REQUIRE(d3dh_D3DReflect());
    EXPECT_HR(d3dh_D3DReflect()(txt, sizeof txt, &IID_ID3D11ShaderReflection, &r), RB_INVALIDCALL);
    EXPECT_HR(d3dh_D3DReflect()(zeros, sizeof zeros, &IID_ID3D11ShaderReflection, &r), RB_INVALIDCALL);
    EXPECT_HR(d3dh_D3DReflect()("DXBC", 4, &IID_ID3D11ShaderReflection, &r), RB_INVALIDCALL);
}
TEST(rb_invalid_ps_and_cs_follow_same_rules)
{
    ID3DBlob *p, *cs; BYTE *a, *z; void *r;
    if (!rb_compile_t("float4 main() : SV_Target { return 1; }", "ps_4_0", 0, &p)) return;
    if (!rb_compile_t("RWBuffer<uint> u : register(u0); [numthreads(4,1,1)] void main(uint3 t : SV_DispatchThreadID) { u[t.x] = t.x; }", "cs_5_0", 0, &cs)) { ID3D10Blob_Release(p); return; }
    a = rb_copy(p); z = rb_copy(cs);
    a[5] ^= 1; z[9] ^= 1;
    r = NULL; EXPECT_HR(d3dh_D3DReflect()(a, RB_BS(p), &IID_ID3D11ShaderReflection, &r), RB_E_FAIL);
    r = NULL; EXPECT_HR(d3dh_D3DReflect()(z, RB_BS(cs), &IID_ID3D11ShaderReflection, &r), RB_E_FAIL);
    free(a); free(z); ID3D10Blob_Release(p); ID3D10Blob_Release(cs);
}
