/* Shared helpers for the cross_com_threads_stress area. */
#ifndef CTS_H
#define CTS_H
#include "d3dh.h"
#include <d3d11_4.h>
#include <dxgi1_6.h>

#define T(n) TEST(cross_com_threads_stress_##n)

/* ---- thread runner --------------------------------------------------------- */
typedef struct cts_job {
    int idx, nthreads, ops;
    void *shared;               /* test specific */
    void *priv;                 /* test specific per-thread */
    volatile LONG fails;        /* failure counter filled by the worker */
    volatile LONG done;         /* operations completed */
    HRESULT hr;                 /* first failing HRESULT (worker side) */
    int apt;                    /* CTS_APT_* applied by the runner before the worker body */
    HRESULT coinit_hr;
    DWORD (WINAPI *fn)(struct cts_job *);
} cts_job;

enum { CTS_APT_NONE = 0, CTS_APT_STA, CTS_APT_STA_NODDE, CTS_APT_MTA, CTS_APT_MTA_SPEED, CTS_APT_STA_THEN_UNINIT, CTS_APT_OLE };

static inline const char *cts_apt_name(int a)
{
    static const char *n[] = { "none", "sta", "sta+nodde", "mta", "mta+speed", "sta-then-uninit", "oleinitialize" };
    return a >= 0 && a <= CTS_APT_OLE ? n[a] : "?";
}

static inline DWORD WINAPI cts_thread_main(LPVOID p)
{
    cts_job *j = p;
    int inited = 0, ole = 0;
    switch (j->apt) {
    case CTS_APT_STA: j->coinit_hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); inited = SUCCEEDED(j->coinit_hr); break;
    case CTS_APT_STA_NODDE: j->coinit_hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); inited = SUCCEEDED(j->coinit_hr); break;
    case CTS_APT_MTA: j->coinit_hr = CoInitializeEx(NULL, COINIT_MULTITHREADED); inited = SUCCEEDED(j->coinit_hr); break;
    case CTS_APT_MTA_SPEED: j->coinit_hr = CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY); inited = SUCCEEDED(j->coinit_hr); break;
    case CTS_APT_STA_THEN_UNINIT: j->coinit_hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); if (SUCCEEDED(j->coinit_hr)) CoUninitialize(); break;
    case CTS_APT_OLE: j->coinit_hr = OleInitialize(NULL); ole = SUCCEEDED(j->coinit_hr); break;
    default: j->coinit_hr = S_OK; break;
    }
    j->fn(j);
    if (ole) OleUninitialize();
    if (inited) CoUninitialize();
    return 0;
}

/* Runs n workers (each in a fresh thread, apartment given by jobs[i].apt) and waits at most timeout_ms.
 * Returns 1 when all finished. On timeout the threads are left alone (never terminated). */
static inline int cts_run(int n, cts_job *jobs, DWORD (WINAPI *fn)(cts_job *), DWORD timeout_ms)
{
    HANDLE h[64]; int i, ok = 1;
    if (n > 64) n = 64;
    for (i = 0; i < n; i++) {
        jobs[i].idx = i; jobs[i].nthreads = n; jobs[i].fn = fn; jobs[i].fails = 0; jobs[i].done = 0; jobs[i].hr = S_OK;
        h[i] = CreateThread(NULL, 0, cts_thread_main, &jobs[i], 0, NULL);
        if (!h[i]) ok = 0;
    }
    for (i = 0; i < n; i++) if (!h[i]) { n = i; break; }
    if (n && WaitForMultipleObjects(n, h, TRUE, timeout_ms) != WAIT_OBJECT_0) ok = 0;
    for (i = 0; i < n; i++) CloseHandle(h[i]);
    return ok;
}
static inline LONG cts_total_fails(int n, cts_job *jobs) { LONG s = 0; for (int i = 0; i < n; i++) s += jobs[i].fails; return s; }
static inline LONG cts_total_done(int n, cts_job *jobs) { LONG s = 0; for (int i = 0; i < n; i++) s += jobs[i].done; return s; }
#define CTS_FAIL(j, hr_) do { InterlockedIncrement(&(j)->fails); if (!(j)->hr) (j)->hr = (hr_); } while (0)

/* ---- devices ------------------------------------------------------------------ */
static inline HRESULT cts_dev11(UINT flags, ID3D11Device **d, ID3D11DeviceContext **c)
{
    return d3dh_make_device11(d, c, NULL, NULL, flags);
}

static inline HRESULT cts_make_buffer(ID3D11Device *d, UINT bytes, UINT bind, ID3D11Buffer **b)
{
    D3D11_BUFFER_DESC bd; memset(&bd, 0, sizeof bd);
    bd.ByteWidth = bytes; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = bind;
    return ID3D11Device_CreateBuffer(d, &bd, NULL, b);
}
static inline HRESULT cts_make_tex(ID3D11Device *d, UINT w, UINT h, ID3D11Texture2D **t)
{
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof td);
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    return ID3D11Device_CreateTexture2D(d, &td, NULL, t);
}

/* ---- process counters ---------------------------------------------------------- */
typedef struct { DWORD cb, PageFaultCount; SIZE_T PeakWorkingSetSize, WorkingSetSize, QuotaPeakPagedPoolUsage, QuotaPagedPoolUsage,
                 QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage, PagefileUsage, PeakPagefileUsage, PrivateUsage; } cts_pmc;
static inline SIZE_T cts_private_bytes(void)
{
    typedef BOOL (WINAPI *fn_t)(HANDLE, cts_pmc *, DWORD);
    static fn_t f; cts_pmc m;
    if (!f) f = (fn_t)GetProcAddress(GetModuleHandleA("kernel32.dll"), "K32GetProcessMemoryInfo");
    if (!f) return 0;
    memset(&m, 0, sizeof m); m.cb = sizeof m;
    return f(GetCurrentProcess(), &m, sizeof m) ? m.PrivateUsage : 0;
}
static inline DWORD cts_handle_count(void) { DWORD n = 0; GetProcessHandleCount(GetCurrentProcess(), &n); return n; }

#define CTS_HEADROOM_MB(n) ((SIZE_T)(n) * 1024 * 1024)
#endif
