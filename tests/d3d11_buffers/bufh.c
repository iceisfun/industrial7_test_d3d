#include "bufh.h"

ID3D11Device *g_dev;
ID3D11DeviceContext *g_ctx;
D3D_FEATURE_LEVEL g_fl;
static int g_tried;

static void bufh_cleanup(void)
{
    if (g_ctx) { ID3D11DeviceContext_ClearState(g_ctx); ID3D11DeviceContext_Flush(g_ctx); }
    SAFE_RELEASE(g_ctx);
    SAFE_RELEASE(g_dev);
}

int bufh_init(void)
{
    if (!g_tried) {
        g_tried = 1;
        if (SUCCEEDED(d3dh_make_device11(&g_dev, &g_ctx, &g_fl, NULL, 0))) atexit(bufh_cleanup);
        else { g_dev = NULL; g_ctx = NULL; }
    }
    return g_dev != NULL;
}

D3D11_BUFFER_DESC bufh_desc(UINT bw, D3D11_USAGE usage, UINT bind, UINT cpu, UINT misc, UINT stride)
{
    D3D11_BUFFER_DESC d;
    memset(&d, 0, sizeof d);
    d.ByteWidth = bw; d.Usage = usage; d.BindFlags = bind; d.CPUAccessFlags = cpu; d.MiscFlags = misc; d.StructureByteStride = stride;
    return d;
}

void bufh_pattern(unsigned char *p, UINT n, unsigned seed)
{
    UINT i;
    for (i = 0; i < n; i++) p[i] = (unsigned char)(seed + i * 7 + (i >> 8) * 13);
}

HRESULT bufh_create(const D3D11_BUFFER_DESC *d, int init, unsigned seed, ID3D11Buffer **out)
{
    D3D11_SUBRESOURCE_DATA sd;
    unsigned char *tmp = NULL;
    HRESULT hr;
    *out = NULL;
    if (!init) return ID3D11Device_CreateBuffer(g_dev, d, NULL, out);
    tmp = malloc(d->ByteWidth ? d->ByteWidth : 1);
    if (!tmp) return E_OUTOFMEMORY;
    bufh_pattern(tmp, d->ByteWidth, seed);
    memset(&sd, 0, sizeof sd);
    sd.pSysMem = tmp;
    hr = ID3D11Device_CreateBuffer(g_dev, d, &sd, out);
    free(tmp);
    return hr;
}

UINT bufh_bytes(ID3D11Buffer *b)
{
    D3D11_BUFFER_DESC d;
    ID3D11Buffer_GetDesc(b, &d);
    return d.ByteWidth;
}

unsigned char *bufh_read(ID3D11Buffer *b, UINT *size)
{
    D3D11_BUFFER_DESC d, sdsc;
    ID3D11Buffer *st = NULL;
    D3D11_MAPPED_SUBRESOURCE m;
    unsigned char *out = NULL;
    ID3D11Buffer_GetDesc(b, &d);
    sdsc = bufh_desc(d.ByteWidth, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ, 0, 0);
    if (FAILED(ID3D11Device_CreateBuffer(g_dev, &sdsc, NULL, &st))) return NULL;
    ID3D11DeviceContext_CopyResource(g_ctx, (ID3D11Resource *)st, (ID3D11Resource *)b);
    if (SUCCEEDED(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &m))) {
        out = malloc(d.ByteWidth);
        if (out) memcpy(out, m.pData, d.ByteWidth);
        ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)st, 0);
    }
    ID3D11Buffer_Release(st);
    if (size) *size = d.ByteWidth;
    return out;
}

int bufh_matches(ID3D11Buffer *b, const unsigned char *want, UINT n)
{
    UINT sz = 0, i;
    unsigned char *got = bufh_read(b, &sz);
    int ok = got && sz >= n;
    for (i = 0; ok && i < n; i++) if (got[i] != want[i]) { t_note("mismatch at byte %u: got %02x want %02x", i, got[i], want[i]); ok = 0; }
    free(got);
    return ok;
}

int bufh_matches_pattern(ID3D11Buffer *b, unsigned seed)
{
    UINT n = bufh_bytes(b);
    unsigned char *w = malloc(n);
    int r;
    if (!w) return 0;
    bufh_pattern(w, n, seed);
    r = bufh_matches(b, w, n);
    free(w);
    return r;
}

const char *bufh_hrn(HRESULT hr) { return d3dh_hr_name(hr); }

HRESULT bufh_priv_device(UINT flags, ID3D11Device **dev, ID3D11DeviceContext **ctx)
{
    D3D_FEATURE_LEVEL fl;
    *dev = NULL; *ctx = NULL;
    return d3dh_make_device11(dev, ctx, &fl, NULL, flags);
}

int bufh_is_brd(ID3D11Device *dev)
{
    IDXGIDevice *dd = NULL;
    IDXGIAdapter *ad = NULL;
    DXGI_ADAPTER_DESC desc;
    int r = 0;
    if (FAILED(ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dd))) return 0;
    if (SUCCEEDED(IDXGIDevice_GetAdapter(dd, &ad)) && SUCCEEDED(IDXGIAdapter_GetDesc(ad, &desc))) r = desc.VendorId == 0x1414;
    if (ad) IDXGIAdapter_Release(ad);
    IDXGIDevice_Release(dd);
    return r;
}
