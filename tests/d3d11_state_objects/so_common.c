#include "so_common.h"

ID3D11Device *so_dev;
ID3D11DeviceContext *so_ctx;
D3D_FEATURE_LEVEL so_fl;
D3D_DRIVER_TYPE so_drv;
static int g_tried;
static ID3D11Device1 *g_d1;
static ID3D11Device3 *g_d3;

static void so_cleanup(void)
{
    SAFE_RELEASE(g_d1);
    SAFE_RELEASE(g_d3);
    if (so_ctx) { ID3D11DeviceContext_ClearState(so_ctx); }
    SAFE_RELEASE(so_ctx);
    SAFE_RELEASE(so_dev);
}

int so_init(void)
{
    if (!g_tried) {
        HRESULT hr;
        g_tried = 1;
        hr = d3dh_make_device11(&so_dev, &so_ctx, &so_fl, &so_drv, 0);
        if (FAILED(hr)) { so_dev = NULL; so_ctx = NULL; return 0; }
        t_note("device: feature level 0x%x driver type %d", (unsigned)so_fl, (int)so_drv);
        ID3D11Device_QueryInterface(so_dev, &IID_ID3D11Device1, (void **)&g_d1);
        ID3D11Device_QueryInterface(so_dev, &IID_ID3D11Device3, (void **)&g_d3);
        atexit(so_cleanup);
    }
    return so_dev != NULL;
}
ID3D11Device1 *so_dev1(void) { return so_init() ? g_d1 : NULL; }
ID3D11Device3 *so_dev3(void) { return so_init() ? g_d3 : NULL; }

int so_logic_ops(void)
{
    D3D11_FEATURE_DATA_D3D11_OPTIONS o;
    memset(&o, 0, sizeof o);
    if (!so_init()) return 0;
    if (FAILED(ID3D11Device_CheckFeatureSupport(so_dev, D3D11_FEATURE_D3D11_OPTIONS, &o, sizeof o))) return 0;
    return o.OutputMergerLogicOp != 0;
}

static struct { const char *src; ID3DBlob *blob; } g_vs[16];
ID3DBlob *so_compile_vs(const char *src)
{
    int i;
    for (i = 0; i < 16; i++) {
        if (g_vs[i].src == src) return g_vs[i].blob;
        if (!g_vs[i].src) {
            ID3DBlob *b = NULL;
            g_vs[i].src = src;
            if (SUCCEEDED(d3dh_compile(src, "main", "vs_5_0", 0, &b))) g_vs[i].blob = b;
            return g_vs[i].blob;
        }
    }
    return NULL;
}
