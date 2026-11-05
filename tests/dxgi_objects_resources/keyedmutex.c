/* IDXGIKeyedMutex semantics over shared textures, including a cross-process check. */
#include "objs.h"

static HRESULT km_tex(objs_ctx *c, UINT extra_misc, ID3D11Texture2D **t, IDXGIKeyedMutex **km)
{
    HRESULT hr;
    *km = NULL;
    hr = objs_tex2d(c->dev, 64, 64, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0,
                    D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | extra_misc, t);
    if (FAILED(hr)) return hr;
    hr = ID3D11Texture2D_QueryInterface(*t, &IID_IDXGIKeyedMutex, (void **)km);
    if (FAILED(hr)) { ID3D11Texture2D_Release(*t); *t = NULL; }
    return hr;
}
#define NEED_KM(c, t, km) NEED_DEV(c); ID3D11Texture2D *t = NULL; IDXGIKeyedMutex *km = NULL; \
    if (FAILED(km_tex(&c, 0, &t, &km))) { t_skip("keyed mutex textures unsupported"); objs_close(&c); return; }
#define DONE_KM(c, t, km) do { IDXGIKeyedMutex_Release(km); ID3D11Texture2D_Release(t); objs_close(&c); } while (0)

TEST(objs_km_identity)
{
    NEED_KM(c, t, km);
    IDXGIObject *o1 = NULL, *o2 = NULL; IDXGIResource *r = NULL;
    EXPECT_HR_OK(IDXGIKeyedMutex_QueryInterface(km, &IID_IDXGIObject, (void **)&o1));
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r));
    EXPECT_HR_OK(IDXGIResource_QueryInterface(r, &IID_IDXGIObject, (void **)&o2));
    EXPECT_EQ(o1, o2);
    EXPECT_EQ(km, (IDXGIKeyedMutex *)r);
    SAFE_RELEASE(o1); SAFE_RELEASE(o2); SAFE_RELEASE(r);
    DONE_KM(c, t, km);
}
TEST(objs_km_parent_and_device)
{
    NEED_KM(c, t, km);
    IUnknown *p = NULL;
    EXPECT_HR_OK(IDXGIKeyedMutex_GetParent(km, &IID_IDXGIDevice, (void **)&p));
    EXPECT_EQ(p, (IUnknown *)c.dx);
    SAFE_RELEASE(p);
    EXPECT_HR_OK(IDXGIKeyedMutex_GetDevice(km, &IID_IDXGIDevice, (void **)&p));
    EXPECT_EQ(p, (IUnknown *)c.dx);
    SAFE_RELEASE(p);
    DONE_KM(c, t, km);
}
TEST(objs_km_private_data_shared_with_resource)
{
    NEED_KM(c, t, km);
    DWORD v = 5, out = 0; UINT size = 4; IDXGIResource *r = NULL;
    EXPECT_HR_OK(IDXGIKeyedMutex_SetPrivateData(km, &OBJS_G1, 4, &v));
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r));
    EXPECT_HR_OK(IDXGIResource_GetPrivateData(r, &OBJS_G1, &size, &out));
    EXPECT_EQ(out, 5);
    IDXGIResource_Release(r);
    DONE_KM(c, t, km);
}
TEST(objs_km_release_unacquired)
{
    NEED_KM(c, t, km);
    EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 1), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0xffffffffffffffffull), DXGI_ERROR_INVALID_CALL);
    DONE_KM(c, t, km);
}
TEST(objs_km_initial_key_is_zero)
{
    NEED_KM(c, t, km);
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 1, 0), WAIT_TIMEOUT);
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0xffffffffffffffffull, 0), WAIT_TIMEOUT);
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
    EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), S_OK);
    DONE_KM(c, t, km);
}
TEST(objs_km_reacquire_same_device)
{
    NEED_KM(c, t, km);
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 1, 0), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), S_OK);
    DONE_KM(c, t, km);
}
TEST(objs_km_release_to_any_key)
{
    NEED_KM(c, t, km);
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
    EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), S_OK);
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
    EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 1), S_OK);
    DONE_KM(c, t, km);
}
TEST(objs_km_acquire_requires_released_key)
{
    NEED_KM(c, t, km);
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
    EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 1), S_OK);
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), WAIT_TIMEOUT);
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 2, 0), WAIT_TIMEOUT);
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 1, 0), S_OK);
    EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), S_OK);
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
    DONE_KM(c, t, km);
}
TEST(objs_km_key_sequence)
{
    NEED_KM(c, t, km);
    UINT64 k;
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
    for (k = 1; k <= 20; k++) {
        HRESULT hr = IDXGIKeyedMutex_ReleaseSync(km, k);
        CHECKF(hr == S_OK, "release %llu: %s", (unsigned long long)k, d3dh_hr_name(hr));
        hr = IDXGIKeyedMutex_AcquireSync(km, k, 0);
        CHECKF(hr == S_OK, "acquire %llu: %s", (unsigned long long)k, d3dh_hr_name(hr));
    }
    EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), S_OK);
    DONE_KM(c, t, km);
}
TEST(objs_km_64bit_keys)
{
    NEED_KM(c, t, km);
    static const UINT64 keys[] = { 0x100000000ull, 0x8000000000000000ull, 0xffffffffffffffffull, 0x00000001ffffffffull };
    int i;
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
    for (i = 0; i < OBJS_ARRAY(keys); i++) {
        HRESULT hr = IDXGIKeyedMutex_ReleaseSync(km, keys[i]);
        CHECKF(hr == S_OK, "release %d: %s", i, d3dh_hr_name(hr));
        /* only the full 64-bit key matches */
        hr = IDXGIKeyedMutex_AcquireSync(km, keys[i] & 0xffffffffull, 0);
        if ((keys[i] & 0xffffffffull) != keys[i]) {
            t_note("key %d: acquire with the low 32 bits only -> %s", i, d3dh_hr_name(hr));
            if (hr == S_OK) IDXGIKeyedMutex_ReleaseSync(km, keys[i]);
        }
        hr = IDXGIKeyedMutex_AcquireSync(km, keys[i], 0);
        CHECKF(hr == S_OK, "acquire %d: %s", i, d3dh_hr_name(hr));
    }
    EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), S_OK);
    DONE_KM(c, t, km);
}
TEST(objs_km_timeout_returns_wait_timeout)
{
    NEED_KM(c, t, km);
    DWORD t0, el; HRESULT hr;
    t0 = GetTickCount();
    hr = IDXGIKeyedMutex_AcquireSync(km, 5, 100);
    el = GetTickCount() - t0;
    EXPECT_HR(hr, WAIT_TIMEOUT);
    CHECKF(el < 5000, "100 ms timeout took %lu ms", el);
    t_note("AcquireSync(100 ms timeout) returned after %lu ms", el);
    DONE_KM(c, t, km);
}
TEST(objs_km_wait_timeout_value)
{
    EXPECT_EQ((HRESULT)WAIT_TIMEOUT, 0x102);
    EXPECT_EQ((HRESULT)WAIT_ABANDONED, 0x80);
    CHECK(SUCCEEDED((HRESULT)WAIT_TIMEOUT));
}
TEST(objs_km_two_devices_basic)
{
    NEED_KM(c, t, km);
    objs_ctx c2; IDXGIResource *r = NULL; HANDLE h = NULL; ID3D11Texture2D *t2 = NULL; IDXGIKeyedMutex *km2 = NULL;
    if (!objs_open(&c2)) { t_skip("second device"); DONE_KM(c, t, km); return; }
    ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r);
    EXPECT_HR_OK(IDXGIResource_GetSharedHandle(r, &h));
    IDXGIResource_Release(r);
    EXPECT_HR_OK(ID3D11Device_OpenSharedResource(c2.dev, h, &IID_ID3D11Texture2D, (void **)&t2));
    if (t2) {
        EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t2, &IID_IDXGIKeyedMutex, (void **)&km2));
        if (km2) {
            EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
            /* the other device sees the mutex as held */
            EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km2, 0, 0), WAIT_TIMEOUT);
            EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km2, 1, 0), WAIT_TIMEOUT);
            /* and may not release what it does not own */
            EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km2, 0), DXGI_ERROR_INVALID_CALL);
            EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km2, 1), DXGI_ERROR_INVALID_CALL);
            /* the owning device cannot acquire a second time */
            EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), DXGI_ERROR_INVALID_CALL);
            EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), S_OK);
            EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km2, 0, 0), S_OK);
            EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), WAIT_TIMEOUT);
            EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km2, 0), S_OK);
            EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
            EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), S_OK);
            IDXGIKeyedMutex_Release(km2);
        }
        ID3D11Texture2D_Release(t2);
    }
    objs_close(&c2);
    DONE_KM(c, t, km);
}
TEST(objs_km_key_handoff_two_devices)
{
    NEED_KM(c, t, km);
    objs_ctx c2; IDXGIResource *r = NULL; HANDLE h = NULL; ID3D11Texture2D *t2 = NULL; IDXGIKeyedMutex *km2 = NULL;
    if (!objs_open(&c2)) { t_skip("second device"); DONE_KM(c, t, km); return; }
    ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r);
    IDXGIResource_GetSharedHandle(r, &h);
    IDXGIResource_Release(r);
    if (SUCCEEDED(ID3D11Device_OpenSharedResource(c2.dev, h, &IID_ID3D11Texture2D, (void **)&t2)) &&
        SUCCEEDED(ID3D11Texture2D_QueryInterface(t2, &IID_IDXGIKeyedMutex, (void **)&km2))) {
        int i;
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
        for (i = 0; i < 6; i++) {
            HRESULT hr = IDXGIKeyedMutex_ReleaseSync(km, i + 1);
            CHECKF(hr == S_OK, "step %d: release by A: %s", i, d3dh_hr_name(hr));
            hr = IDXGIKeyedMutex_AcquireSync(km2, i + 1, 0);
            CHECKF(hr == S_OK, "step %d: acquire by B: %s", i, d3dh_hr_name(hr));
            if (hr != S_OK) break;
            hr = IDXGIKeyedMutex_ReleaseSync(km2, 100 + i);
            CHECKF(hr == S_OK, "step %d: release by B: %s", i, d3dh_hr_name(hr));
            hr = IDXGIKeyedMutex_AcquireSync(km, 100 + i, 0);
            CHECKF(hr == S_OK, "step %d: acquire by A: %s", i, d3dh_hr_name(hr));
            if (hr != S_OK) break;
            /* A now owns it again; B sees a mismatching key */
            CHECKF(IDXGIKeyedMutex_AcquireSync(km2, 100 + i, 0) == WAIT_TIMEOUT, "step %d: B acquire while A holds", i);
        }
        IDXGIKeyedMutex_ReleaseSync(km, 0);
        IDXGIKeyedMutex_Release(km2);
    }
    SAFE_RELEASE(t2);
    objs_close(&c2);
    DONE_KM(c, t, km);
}
TEST(objs_km_device_closed_while_holding_abandons)
{
    objs_ctx c, c2; ID3D11Texture2D *t = NULL, *t2 = NULL; IDXGIKeyedMutex *km = NULL, *km2 = NULL; IDXGIResource *r = NULL; HANDLE h = NULL;
    if (!objs_open(&c)) { t_skip("no D3D11 device"); return; }
    if (FAILED(km_tex(&c, 0, &t, &km))) { t_skip("keyed mutex textures unsupported"); objs_close(&c); return; }
    if (!objs_open(&c2)) { t_skip("second device"); IDXGIKeyedMutex_Release(km); ID3D11Texture2D_Release(t); objs_close(&c); return; }
    ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r);
    IDXGIResource_GetSharedHandle(r, &h);
    IDXGIResource_Release(r);
    if (SUCCEEDED(ID3D11Device_OpenSharedResource(c2.dev, h, &IID_ID3D11Texture2D, (void **)&t2)) &&
        SUCCEEDED(ID3D11Texture2D_QueryInterface(t2, &IID_IDXGIKeyedMutex, (void **)&km2))) {
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
        IDXGIKeyedMutex_Release(km); ID3D11Texture2D_Release(t);
        objs_close(&c);   /* original device destroyed while holding the mutex */
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km2, 0, 0), WAIT_ABANDONED);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km2, 0, 0), WAIT_ABANDONED);
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km2, 0), DXGI_ERROR_INVALID_CALL);
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km2, 1), DXGI_ERROR_INVALID_CALL);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km2, 0, 0), WAIT_ABANDONED);
        IDXGIKeyedMutex_Release(km2); ID3D11Texture2D_Release(t2);
    } else {
        t_skip("open shared resource failed");
        SAFE_RELEASE(t2);
        IDXGIKeyedMutex_Release(km); ID3D11Texture2D_Release(t);
        objs_close(&c);
    }
    objs_close(&c2);
}
TEST(objs_km_nthandle_keyed_texture)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIKeyedMutex *km = NULL; IDXGIResource1 *r1 = NULL; HANDLE h = NULL; ID3D11Device1 *d1 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(c.dev, &IID_ID3D11Device1, (void **)&d1))) { t_skip("no ID3D11Device1"); objs_close(&c); return; }
    if (FAILED(km_tex(&c, D3D11_RESOURCE_MISC_SHARED_NTHANDLE, &t, &km))) { t_skip("NT-handle keyed textures unsupported"); ID3D11Device1_Release(d1); objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource1, (void **)&r1));
    if (r1 && SUCCEEDED(IDXGIResource1_CreateSharedHandle(r1, NULL, GENERIC_ALL | DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, NULL, &h))) {
        ID3D11Texture2D *o = NULL;
        EXPECT_HR_OK(ID3D11Device1_OpenSharedResource1(d1, h, &IID_ID3D11Texture2D, (void **)&o));
        if (o) {
            IDXGIKeyedMutex *km2 = NULL;
            EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(o, &IID_IDXGIKeyedMutex, (void **)&km2));
            if (km2) {
                /* same device opening its own shared texture: the opened view shares the mutex state */
                EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
                HRESULT hr = IDXGIKeyedMutex_AcquireSync(km2, 0, 0);
                t_note("acquire of the same-device alias while held -> %s", d3dh_hr_name(hr));
                if (hr == S_OK) IDXGIKeyedMutex_ReleaseSync(km2, 0);
                IDXGIKeyedMutex_ReleaseSync(km, 0);
                IDXGIKeyedMutex_Release(km2);
            }
            ID3D11Texture2D_Release(o);
        }
        CloseHandle(h);
    }
    SAFE_RELEASE(r1);
    IDXGIKeyedMutex_Release(km); ID3D11Texture2D_Release(t); ID3D11Device1_Release(d1);
    objs_close(&c);
}

/* ---- cross-process --------------------------------------------------------------------------------- */
CHILD(objs_km_child)
{
    objs_ctx c; ID3D11Device1 *d1 = NULL; ID3D11Texture2D *t = NULL; IDXGIKeyedMutex *km = NULL; WCHAR name[96]; HRESULT hr; int rc = 0;
    if (argc < 2) return 2;
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, name, 96);
    if (!objs_open(&c)) return 10;
    if (FAILED(ID3D11Device_QueryInterface(c.dev, &IID_ID3D11Device1, (void **)&d1))) { objs_close(&c); return 11; }
    hr = ID3D11Device1_OpenSharedResourceByName(d1, name, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, &IID_ID3D11Texture2D, (void **)&t);
    if (FAILED(hr)) { printf("  child: OpenSharedResourceByName -> %s\n", d3dh_hr_name(hr)); rc = 12; goto out; }
    if (FAILED(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIKeyedMutex, (void **)&km))) { rc = 13; goto out; }
    /* parent released with key 7: key 0 does not match, 7 does */
    hr = IDXGIKeyedMutex_AcquireSync(km, 0, 50);
    if (hr != WAIT_TIMEOUT) { printf("  child: acquire(0) -> %s\n", d3dh_hr_name(hr)); rc = 20; goto out; }
    hr = IDXGIKeyedMutex_AcquireSync(km, 7, 2000);
    if (hr != S_OK) { printf("  child: acquire(7) -> %s\n", d3dh_hr_name(hr)); rc = 21; goto out; }
    hr = IDXGIKeyedMutex_AcquireSync(km, 7, 0);
    if (hr != DXGI_ERROR_INVALID_CALL) { printf("  child: reacquire -> %s\n", d3dh_hr_name(hr)); rc = 22; goto out; }
    hr = IDXGIKeyedMutex_ReleaseSync(km, 8);
    if (hr != S_OK) { printf("  child: release(8) -> %s\n", d3dh_hr_name(hr)); rc = 23; goto out; }
out:
    SAFE_RELEASE(km); SAFE_RELEASE(t); SAFE_RELEASE(d1); objs_close(&c);
    return rc;
}
TEST(objs_km_cross_process_named)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIKeyedMutex *km = NULL; IDXGIResource1 *r1 = NULL; HANDLE h = NULL; WCHAR wname[96]; char name[96]; DWORD rc;
    if (FAILED(km_tex(&c, D3D11_RESOURCE_MISC_SHARED_NTHANDLE, &t, &km))) { t_skip("NT-handle keyed textures unsupported"); objs_close(&c); return; }
    snprintf(name, sizeof(name), "Local\\objs_km_%lu", GetCurrentProcessId());
    MultiByteToWideChar(CP_ACP, 0, name, -1, wname, 96);
    if (FAILED(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource1, (void **)&r1)) ||
        FAILED(IDXGIResource1_CreateSharedHandle(r1, NULL, GENERIC_ALL | DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, wname, &h))) {
        t_skip("named shared handle unsupported"); SAFE_RELEASE(r1); DONE_KM(c, t, km); return;
    }
    EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
    EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 7), S_OK);
    rc = t_run_child("objs_km_child", name, 30000);
    if (rc == 10 || rc == 11 || rc == 12) t_note("child could not set up (code %lu)", rc);
    else EXPECT_EQ(rc, 0);
    if (rc == 0) {
        /* the child released with key 8 */
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 7, 0), WAIT_TIMEOUT);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 8, 0), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), S_OK);
    }
    CloseHandle(h);
    IDXGIResource1_Release(r1);
    DONE_KM(c, t, km);
}
TEST(objs_km_named_collision)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL, *t2 = NULL; IDXGIKeyedMutex *km = NULL; IDXGIResource1 *r1 = NULL, *r2 = NULL; HANDLE h = NULL, h2 = NULL; WCHAR wname[96];
    if (FAILED(km_tex(&c, D3D11_RESOURCE_MISC_SHARED_NTHANDLE, &t, &km))) { t_skip("NT-handle keyed textures unsupported"); objs_close(&c); return; }
    wsprintfW(wname, L"Local\\objs_km_dup_%lu", GetCurrentProcessId());
    ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource1, (void **)&r1);
    if (!r1 || FAILED(IDXGIResource1_CreateSharedHandle(r1, NULL, GENERIC_ALL | DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, wname, &h))) {
        t_skip("named shared handle unsupported"); SAFE_RELEASE(r1); DONE_KM(c, t, km); return;
    }
    if (SUCCEEDED(objs_tex2d(c.dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0,
                             D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE, &t2)) &&
        SUCCEEDED(ID3D11Texture2D_QueryInterface(t2, &IID_IDXGIResource1, (void **)&r2))) {
        HRESULT hr = IDXGIResource1_CreateSharedHandle(r2, NULL, GENERIC_ALL | DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, wname, &h2);
        t_note("second CreateSharedHandle with an existing name -> %s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr) && h2) CloseHandle(h2);
        IDXGIResource1_Release(r2);
    }
    SAFE_RELEASE(t2);
    CloseHandle(h);
    IDXGIResource1_Release(r1);
    DONE_KM(c, t, km);
}
