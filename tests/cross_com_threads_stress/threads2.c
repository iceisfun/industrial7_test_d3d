/* More cross-thread contracts: D3D9 multithreaded devices, a D3D9 device used from another thread, a shared D3D12 queue,
 * concurrent D3D10 device creation and an event query polled from a second thread. */
#include "cts.h"

/* ---- D3D9 ------------------------------------------------------------------------------------------------------------ */
typedef struct { IDirect3D9 *d3d; IDirect3DDevice9 *dev; HWND hwnd; } d9ctx;
static int d9_open(d9ctx *c, DWORD behavior)
{
    D3DPRESENT_PARAMETERS pp; HRESULT hr;
    memset(c, 0, sizeof *c);
    if (!d3dh_Direct3DCreate9()) { t_skip("no Direct3DCreate9"); return 0; }
    c->d3d = d3dh_Direct3DCreate9()(D3D_SDK_VERSION);
    if (!c->d3d) { t_skip("Direct3DCreate9 failed"); return 0; }
    c->hwnd = d3dh_create_hidden_window();
    memset(&pp, 0, sizeof pp);
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN; pp.BackBufferWidth = 64; pp.BackBufferHeight = 64; pp.hDeviceWindow = c->hwnd;
    hr = IDirect3D9_CreateDevice(c->d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, c->hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | behavior, &pp, &c->dev);
    if (FAILED(hr) || !c->dev) { t_skip("no D3D9 HAL device (%s)", d3dh_hr_name(hr)); IDirect3D9_Release(c->d3d); d3dh_destroy_window(c->hwnd); memset(c, 0, sizeof *c); return 0; }
    return 1;
}
static void d9_close(d9ctx *c) { if (c->dev) IDirect3DDevice9_Release(c->dev); if (c->d3d) IDirect3D9_Release(c->d3d); d3dh_destroy_window(c->hwnd); }

static DWORD WINAPI d9_hammer(cts_job *j)
{
    IDirect3DDevice9 *dev = j->shared;
    DWORD stage = (DWORD)j->idx;
    for (int i = 0; i < j->ops; i++) {
        DWORD v = 0, want = 0x1000u * (stage + 1) + (DWORD)i;
        IDirect3DTexture9 *t = NULL;
        HRESULT h = IDirect3DDevice9_SetTextureStageState(dev, stage, D3DTSS_CONSTANT, want);
        if (FAILED(h)) CTS_FAIL(j, h);
        h = IDirect3DDevice9_GetTextureStageState(dev, stage, D3DTSS_CONSTANT, &v);
        if (FAILED(h) || v != want) CTS_FAIL(j, FAILED(h) ? h : E_FAIL);
        h = IDirect3DDevice9_CreateTexture(dev, 8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, NULL);
        if (FAILED(h)) CTS_FAIL(j, h);
        else {
            h = IDirect3DDevice9_SetTexture(dev, stage, (IDirect3DBaseTexture9 *)t);
            if (FAILED(h)) CTS_FAIL(j, h);
            IDirect3DDevice9_SetTexture(dev, stage, NULL);
            IDirect3DTexture9_Release(t);
        }
        j->done++;
    }
    return 0;
}
T(threads_d3d9_multithreaded_device_hammer)
{
    d9ctx c; cts_job j[4]; D3DDEVICE_CREATION_PARAMETERS cp;
    if (!d9_open(&c, D3DCREATE_MULTITHREADED)) return;
    EXPECT_HR_OK(IDirect3DDevice9_GetCreationParameters(c.dev, &cp));
    CHECK(cp.BehaviorFlags & D3DCREATE_MULTITHREADED);
    memset(j, 0, sizeof j);
    for (int i = 0; i < 4; i++) { j[i].shared = c.dev; j[i].ops = 200; }
    CHECK(cts_run(4, j, d9_hammer, 120000));
    EXPECT_EQ(cts_total_fails(4, j), 0);
    EXPECT_EQ(cts_total_done(4, j), 800);
    EXPECT_HR(IDirect3DDevice9_TestCooperativeLevel(c.dev), D3D_OK);
    d9_close(&c);
}

static DWORD WINAPI d9_other_thread(cts_job *j)
{
    IDirect3DDevice9 *dev = j->shared; HRESULT h; DWORD v = 0;
    h = IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_CW); if (FAILED(h)) CTS_FAIL(j, h);
    h = IDirect3DDevice9_GetRenderState(dev, D3DRS_CULLMODE, &v); if (FAILED(h) || v != D3DCULL_CW) CTS_FAIL(j, E_FAIL);
    h = IDirect3DDevice9_BeginScene(dev); if (FAILED(h)) CTS_FAIL(j, h);
    h = IDirect3DDevice9_Clear(dev, 0, NULL, D3DCLEAR_TARGET, 0xff102030u, 1.0f, 0); if (FAILED(h)) CTS_FAIL(j, h);
    h = IDirect3DDevice9_EndScene(dev); if (FAILED(h)) CTS_FAIL(j, h);
    h = IDirect3DDevice9_Present(dev, NULL, NULL, NULL, NULL); if (FAILED(h)) CTS_FAIL(j, h);
    j->done++;
    return 0;
}
/* Windows 10: a D3D9 device has no thread affinity; creating it on one thread and rendering/presenting on another works (even without D3DCREATE_MULTITHREADED). */
T(threads_d3d9_device_used_from_other_thread)
{
    d9ctx c; cts_job j[1];
    if (!d9_open(&c, 0)) return;
    memset(j, 0, sizeof j);
    j[0].shared = c.dev; j[0].ops = 1;
    CHECK(cts_run(1, j, d9_other_thread, 60000));
    EXPECT_EQ(cts_total_fails(1, j), 0);
    EXPECT_EQ(cts_total_done(1, j), 1);
    EXPECT_HR(IDirect3DDevice9_TestCooperativeLevel(c.dev), D3D_OK);
    d9_close(&c);
}

/* ---- D3D10 ----------------------------------------------------------------------------------------------------------- */
static DWORD WINAPI d10_create(cts_job *j)
{
    for (int i = 0; i < j->ops; i++) {
        ID3D10Device *d = NULL; ID3D10Buffer *b = NULL; D3D10_BUFFER_DESC bd;
        HRESULT h = d3dh_make_device10(&d, NULL, 0);
        if (FAILED(h) || !d) { CTS_FAIL(j, FAILED(h) ? h : E_FAIL); continue; }
        memset(&bd, 0, sizeof bd); bd.ByteWidth = 256; bd.Usage = D3D10_USAGE_DEFAULT; bd.BindFlags = D3D10_BIND_VERTEX_BUFFER;
        h = ID3D10Device_CreateBuffer(d, &bd, NULL, &b);
        if (FAILED(h)) CTS_FAIL(j, h);
        SAFE_RELEASE(b);
        if (ID3D10Device_Release(d) != 0) CTS_FAIL(j, E_FAIL);
        j->done++;
    }
    return 0;
}
T(threads_d3d10_devices_created_concurrently_4x10)
{
    cts_job j[4]; ID3D10Device *probe = NULL;
    REQUIRE(d3dh_D3D10CreateDevice());
    if (FAILED(d3dh_make_device10(&probe, NULL, 0)) || !probe) { t_skip("no D3D10 device"); return; }
    SAFE_RELEASE(probe);
    memset(j, 0, sizeof j);
    for (int i = 0; i < 4; i++) j[i].ops = 10;
    CHECK(cts_run(4, j, d10_create, 120000));
    EXPECT_EQ(cts_total_fails(4, j), 0);
    EXPECT_EQ(cts_total_done(4, j), 40);
}

/* ---- D3D12: one queue, several recording threads ------------------------------------------------------------------- */
typedef struct { ID3D12Device *d; ID3D12CommandQueue *q; } d12q;
static DWORD WINAPI d12_queue_worker(cts_job *j)
{
    d12q *s = j->shared; ID3D12Fence *fe = NULL;
    HRESULT h = ID3D12Device_CreateFence(s->d, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&fe);
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (FAILED(h) || !ev) { CTS_FAIL(j, FAILED(h) ? h : E_FAIL); if (ev) CloseHandle(ev); SAFE_RELEASE(fe); return 0; }
    for (int i = 0; i < j->ops; i++) {
        ID3D12CommandAllocator *ca = NULL; ID3D12GraphicsCommandList *cl = NULL; UINT64 v = (UINT64)i + 1;
        h = ID3D12Device_CreateCommandAllocator(s->d, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&ca);
        if (SUCCEEDED(h)) h = ID3D12Device_CreateCommandList(s->d, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, ca, NULL, &IID_ID3D12GraphicsCommandList, (void **)&cl);
        if (SUCCEEDED(h)) h = ID3D12GraphicsCommandList_Close(cl);
        if (FAILED(h)) { CTS_FAIL(j, h); SAFE_RELEASE(cl); SAFE_RELEASE(ca); continue; }
        ID3D12CommandQueue_ExecuteCommandLists(s->q, 1, (ID3D12CommandList *const *)&cl);
        h = ID3D12CommandQueue_Signal(s->q, fe, v);
        if (FAILED(h)) CTS_FAIL(j, h);
        else {
            h = ID3D12Fence_SetEventOnCompletion(fe, v, ev);
            if (FAILED(h)) CTS_FAIL(j, h);
            else if (WaitForSingleObject(ev, 20000) != WAIT_OBJECT_0) CTS_FAIL(j, E_FAIL);
            else if (ID3D12Fence_GetCompletedValue(fe) < v) CTS_FAIL(j, E_FAIL);
        }
        SAFE_RELEASE(cl); SAFE_RELEASE(ca);
        j->done++;
    }
    CloseHandle(ev); SAFE_RELEASE(fe);
    return 0;
}
T(threads_d3d12_shared_queue_four_recorders)
{
    d12q s; cts_job j[4]; int w; D3D12_COMMAND_QUEUE_DESC qd;
    REQUIRE(d3dh_D3D12CreateDevice());
    if (FAILED(d3dh_make_device12(&s.d, D3D_FEATURE_LEVEL_11_0, &w))) { t_skip("no d3d12 device"); return; }
    memset(&qd, 0, sizeof qd); qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    s.q = NULL;
    EXPECT_HR_OK(ID3D12Device_CreateCommandQueue(s.d, &qd, &IID_ID3D12CommandQueue, (void **)&s.q));
    if (!s.q) { SAFE_RELEASE(s.d); return; }
    memset(j, 0, sizeof j);
    for (int i = 0; i < 4; i++) { j[i].shared = &s; j[i].ops = 25; }
    CHECK(cts_run(4, j, d12_queue_worker, 120000));
    EXPECT_EQ(cts_total_fails(4, j), 0);
    EXPECT_EQ(cts_total_done(4, j), 100);
    EXPECT_HR(ID3D12Device_GetDeviceRemovedReason(s.d), S_OK);
    SAFE_RELEASE(s.q); SAFE_RELEASE(s.d);
}

/* ---- D3D11: an event query issued on one thread is observed from another -------------------------------------------- */
typedef struct { ID3D11DeviceContext *c; ID3D11Query *q; } qshared;
static DWORD WINAPI qpoll(cts_job *j)
{
    qshared *s = j->shared; DWORD start = GetTickCount(); BOOL b = FALSE; HRESULT h;
    while ((h = ID3D11DeviceContext_GetData(s->c, (ID3D11Asynchronous *)s->q, &b, sizeof b, 0)) == S_FALSE) {
        if (GetTickCount() - start > 10000) { CTS_FAIL(j, E_FAIL); return 0; }
        Sleep(1);
    }
    if (h != S_OK || b != TRUE) CTS_FAIL(j, h);
    j->done++;
    return 0;
}
T(threads_d3d11_event_query_observed_on_other_thread)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; ID3D11Multithread *mt = NULL; ID3D11Query *q = NULL; D3D11_QUERY_DESC qd; cts_job j[1]; qshared s;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    if (SUCCEEDED(ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11Multithread, (void **)&mt))) { ID3D11Multithread_SetMultithreadProtected(mt, TRUE); SAFE_RELEASE(mt); }
    memset(&qd, 0, sizeof qd); qd.Query = D3D11_QUERY_EVENT;
    EXPECT_HR_OK(ID3D11Device_CreateQuery(d, &qd, &q));
    if (q) {
        s.c = c; s.q = q;
        ID3D11DeviceContext_End(c, (ID3D11Asynchronous *)q);
        ID3D11DeviceContext_Flush(c);
        memset(j, 0, sizeof j); j[0].shared = &s; j[0].ops = 1;
        CHECK(cts_run(1, j, qpoll, 30000));
        EXPECT_EQ(cts_total_fails(1, j), 0);
        EXPECT_EQ(cts_total_done(1, j), 1);
    }
    SAFE_RELEASE(q); SAFE_RELEASE(c); SAFE_RELEASE(d);
}
