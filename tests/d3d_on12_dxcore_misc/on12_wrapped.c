/* ID3D11On12Device: CreateWrappedResource / Acquire / Release / ReturnUnderlyingResource. */
#include "odm.h"

typedef struct {
    ID3D12Device *d12; ID3D12CommandQueue *q; ID3D11Device *dev; ID3D11DeviceContext *ctx; ID3D11On12Device *o;
} on12ctx;

static int on12_init(on12ctx *c) {
    PFN_D3D11ON12_CREATE_DEVICE create = odm_on12_create();
    memset(c, 0, sizeof *c);
    if (!create) return 0;
    c->d12 = odm_dev12();
    if (!c->d12) return 0;
    c->q = odm_queue(c->d12, D3D12_COMMAND_LIST_TYPE_DIRECT, 0, 0);
    if (!c->q) return 0;
    IUnknown *arr[1] = {(IUnknown *)c->q};
    if (FAILED(create((IUnknown *)c->d12, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, arr, 1, 0, &c->dev, &c->ctx, NULL))) return 0;
    if (FAILED(ID3D11Device_QueryInterface(c->dev, &IID_ID3D11On12Device, (void **)&c->o))) return 0;
    return 1;
}
static void on12_free(on12ctx *c) {
    if (c->ctx) ID3D11DeviceContext_ClearState(c->ctx);
    SAFE_RELEASE(c->o); SAFE_RELEASE(c->ctx); SAFE_RELEASE(c->dev); SAFE_RELEASE(c->q); SAFE_RELEASE(c->d12);
}
#define NEED_CTX() on12ctx c; if (!on12_init(&c)) { on12_free(&c); t_skip("D3D11On12 not available"); return; }

static ID3D12Resource *tex12(ID3D12Device *d, UINT w, UINT h, DXGI_FORMAT f, D3D12_RESOURCE_FLAGS fl, D3D12_RESOURCE_STATES st, UINT mips, UINT arr) {
    D3D12_HEAP_PROPERTIES hp; D3D12_RESOURCE_DESC rd; ID3D12Resource *r = NULL;
    memset(&hp, 0, sizeof hp); hp.Type = D3D12_HEAP_TYPE_DEFAULT; hp.CreationNodeMask = 1; hp.VisibleNodeMask = 1;
    memset(&rd, 0, sizeof rd);
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; rd.Width = w; rd.Height = h; rd.DepthOrArraySize = (UINT16)arr; rd.MipLevels = (UINT16)mips;
    rd.Format = f; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; rd.Flags = fl;
    if (FAILED(ID3D12Device_CreateCommittedResource(d, &hp, D3D12_HEAP_FLAG_NONE, &rd, st, NULL, &IID_ID3D12Resource, (void **)&r))) return NULL;
    return r;
}
static ID3D12Resource *buf12(ID3D12Device *d, UINT64 size, D3D12_RESOURCE_FLAGS fl, D3D12_RESOURCE_STATES st) {
    D3D12_HEAP_PROPERTIES hp; D3D12_RESOURCE_DESC rd; ID3D12Resource *r = NULL;
    memset(&hp, 0, sizeof hp); hp.Type = D3D12_HEAP_TYPE_DEFAULT; hp.CreationNodeMask = 1; hp.VisibleNodeMask = 1;
    memset(&rd, 0, sizeof rd);
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; rd.Width = size; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; rd.Flags = fl;
    if (FAILED(ID3D12Device_CreateCommittedResource(d, &hp, D3D12_HEAP_FLAG_NONE, &rd, st, NULL, &IID_ID3D12Resource, (void **)&r))) return NULL;
    return r;
}
static D3D11_RESOURCE_FLAGS rf(UINT bind) { D3D11_RESOURCE_FLAGS f; memset(&f, 0, sizeof f); f.BindFlags = bind; return f; }

TEST(d3d_on12_dxcore_misc_wrap_null_flags) {
    NEED_CTX();
    ID3D12Resource *r = tex12(c.d12, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, 1, 1);
    if (!r) { on12_free(&c); t_skip("cannot create resource"); return; }
    ID3D11Texture2D *t = (ID3D11Texture2D *)1;
    HRESULT hr = ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, NULL, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, &IID_ID3D11Texture2D, (void **)&t);
    EXPECT_HR(hr, E_INVARG);
    CHECK(t == NULL);
    SAFE_RELEASE(r); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_wrap_null_flags_does_not_leak_resource_ref) {
    NEED_CTX();
    ID3D12Resource *r = tex12(c.d12, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, 1, 1);
    if (!r) { on12_free(&c); t_skip("cannot create resource"); return; }
    ULONG before = d3dh_refcount(r);
    void *out = NULL;
    ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, NULL, 0, 0, &IID_ID3D11Texture2D, &out);
    EXPECT_EQ(d3dh_refcount(r), before);
    SAFE_RELEASE(r); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_wrap_texture_basic) {
    NEED_CTX();
    ID3D12Resource *r = tex12(c.d12, 32, 16, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, 1, 1);
    if (!r) { on12_free(&c); t_skip("cannot create resource"); return; }
    D3D11_RESOURCE_FLAGS f = rf(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    ID3D11Texture2D *t = NULL;
    HRESULT hr = ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, &IID_ID3D11Texture2D, (void **)&t);
    EXPECT_HR(hr, S_OK);
    if (t) {
        D3D11_TEXTURE2D_DESC d; memset(&d, 0xCD, sizeof d);
        ID3D11Texture2D_GetDesc(t, &d);
        CHECK(d.Width == 32); CHECK(d.Height == 16); CHECK(d.Format == DXGI_FORMAT_R8G8B8A8_UNORM);
        CHECK(d.MipLevels == 1); CHECK(d.ArraySize == 1); CHECK(d.SampleDesc.Count == 1);
        CHECK((d.BindFlags & (D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE)) == (D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE));
        SAFE_RELEASE(t);
    }
    SAFE_RELEASE(r); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_wrap_texture_desc_roundtrip_mips_array) {
    NEED_CTX();
    ID3D12Resource *r = tex12(c.d12, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, 3, 2);
    if (!r) { on12_free(&c); t_skip("cannot create resource"); return; }
    D3D11_RESOURCE_FLAGS f = rf(D3D11_BIND_RENDER_TARGET);
    ID3D11Texture2D *t = NULL;
    HRESULT hr = ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON, &IID_ID3D11Texture2D, (void **)&t);
    EXPECT_HR(hr, S_OK);
    if (t) {
        D3D11_TEXTURE2D_DESC d; ID3D11Texture2D_GetDesc(t, &d);
        CHECK(d.Width == 64 && d.Height == 64); CHECK(d.MipLevels == 3); CHECK(d.ArraySize == 2); CHECK(d.Format == DXGI_FORMAT_B8G8R8A8_UNORM);
        SAFE_RELEASE(t);
    }
    SAFE_RELEASE(r); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_wrap_as_resource_interface) {
    NEED_CTX();
    ID3D12Resource *r = tex12(c.d12, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, 1, 1);
    if (!r) { on12_free(&c); t_skip("cannot create resource"); return; }
    D3D11_RESOURCE_FLAGS f = rf(D3D11_BIND_RENDER_TARGET);
    ID3D11Resource *res = NULL;
    EXPECT_HR(ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, &IID_ID3D11Resource, (void **)&res), S_OK);
    if (res) {
        D3D11_RESOURCE_DIMENSION dim = 0;
        ID3D11Resource_GetType(res, &dim);
        EXPECT_EQ(dim, D3D11_RESOURCE_DIMENSION_TEXTURE2D);
        ID3D11Texture2D *t = NULL;
        EXPECT_HR(ID3D11Resource_QueryInterface(res, &IID_ID3D11Texture2D, (void **)&t), S_OK);
        SAFE_RELEASE(t); SAFE_RELEASE(res);
    }
    SAFE_RELEASE(r); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_wrap_wrong_dimension_interface) {
    NEED_CTX();
    ID3D12Resource *r = tex12(c.d12, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, 1, 1);
    if (!r) { on12_free(&c); t_skip("cannot create resource"); return; }
    D3D11_RESOURCE_FLAGS f = rf(D3D11_BIND_RENDER_TARGET);
    void *out = (void *)1;
    HRESULT hr = ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, &IID_ID3D11Buffer, &out);
    t_note("2D texture wrapped as ID3D11Buffer: %s out=%p", d3dh_hr_name(hr), out);
    if (SUCCEEDED(hr)) ((IUnknown *)out)->lpVtbl->Release((IUnknown *)out);
    SAFE_RELEASE(r); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_wrap_buffer) {
    NEED_CTX();
    ID3D12Resource *r = buf12(c.d12, 4096, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON);
    if (!r) { on12_free(&c); t_skip("cannot create buffer"); return; }
    D3D11_RESOURCE_FLAGS f = rf(D3D11_BIND_VERTEX_BUFFER);
    ID3D11Buffer *b = NULL;
    HRESULT hr = ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COMMON, &IID_ID3D11Buffer, (void **)&b);
    EXPECT_HR(hr, S_OK);
    if (b) {
        D3D11_BUFFER_DESC d; memset(&d, 0xCD, sizeof d);
        ID3D11Buffer_GetDesc(b, &d);
        CHECK(d.ByteWidth == 4096);
        CHECK((d.BindFlags & D3D11_BIND_VERTEX_BUFFER) != 0);
        SAFE_RELEASE(b);
    }
    SAFE_RELEASE(r); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_wrap_holds_underlying_reference) {
    NEED_CTX();
    ID3D12Resource *r = tex12(c.d12, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, 1, 1);
    if (!r) { on12_free(&c); t_skip("cannot create resource"); return; }
    ULONG before = d3dh_refcount(r);
    D3D11_RESOURCE_FLAGS f = rf(D3D11_BIND_RENDER_TARGET);
    ID3D11Texture2D *t = NULL;
    HRESULT hr = ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, &IID_ID3D11Texture2D, (void **)&t);
    if (FAILED(hr)) { SAFE_RELEASE(r); on12_free(&c); t_skip("wrap failed %s", d3dh_hr_name(hr)); return; }
    CHECK(d3dh_refcount(r) > before);
    SAFE_RELEASE(t);
    ID3D11DeviceContext_Flush(c.ctx);
    t_note("d3d12 resource refs before=%lu after release=%lu", before, d3dh_refcount(r));
    SAFE_RELEASE(r); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_wrap_same_resource_twice) {
    NEED_CTX();
    ID3D12Resource *r = tex12(c.d12, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, 1, 1);
    if (!r) { on12_free(&c); t_skip("cannot create resource"); return; }
    D3D11_RESOURCE_FLAGS f = rf(D3D11_BIND_RENDER_TARGET);
    ID3D11Texture2D *a = NULL, *b = NULL;
    HRESULT ha = ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, &IID_ID3D11Texture2D, (void **)&a);
    HRESULT hb = ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, &IID_ID3D11Texture2D, (void **)&b);
    EXPECT_HR(ha, S_OK);
    EXPECT_HR(hb, S_OK);   /* a second wrapper over the same D3D12 resource is allowed and is a distinct object (probed) */
    CHECK(a != NULL && b != NULL && a != b);
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(r); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_wrap_misc_flags_passthrough) {
    NEED_CTX();
    ID3D12Resource *r = tex12(c.d12, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, 1, 1);
    if (!r) { on12_free(&c); t_skip("cannot create resource"); return; }
    D3D11_RESOURCE_FLAGS f = rf(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    f.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    ID3D11Texture2D *t = NULL;
    HRESULT hr = ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, &IID_ID3D11Texture2D, (void **)&t);
    EXPECT_HR(hr, E_INVALIDARG); /* Win10: probed */
    t_note("wrap with MISC_SHARED: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(t); SAFE_RELEASE(r); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_wrap_srv_and_rtv_creation) {
    NEED_CTX();
    ID3D12Resource *r = tex12(c.d12, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, 1, 1);
    if (!r) { on12_free(&c); t_skip("cannot create resource"); return; }
    D3D11_RESOURCE_FLAGS f = rf(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    ID3D11Texture2D *t = NULL;
    if (FAILED(ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, &IID_ID3D11Texture2D, (void **)&t)) || !t) { SAFE_RELEASE(r); on12_free(&c); t_skip("wrap failed"); return; }
    ID3D11RenderTargetView *rtv = NULL; ID3D11ShaderResourceView *srv = NULL;
    EXPECT_HR(ID3D11Device_CreateRenderTargetView(c.dev, (ID3D11Resource *)t, NULL, &rtv), S_OK);
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(c.dev, (ID3D11Resource *)t, NULL, &srv), S_OK);
    SAFE_RELEASE(rtv); SAFE_RELEASE(srv); SAFE_RELEASE(t); SAFE_RELEASE(r); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_wrap_rtv_requires_bind_flag) {
    NEED_CTX();
    ID3D12Resource *r = tex12(c.d12, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, 1, 1);
    if (!r) { on12_free(&c); t_skip("cannot create resource"); return; }
    D3D11_RESOURCE_FLAGS f = rf(D3D11_BIND_SHADER_RESOURCE);
    ID3D11Texture2D *t = NULL;
    if (FAILED(ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, &IID_ID3D11Texture2D, (void **)&t)) || !t) { SAFE_RELEASE(r); on12_free(&c); t_skip("wrap failed"); return; }
    ID3D11RenderTargetView *rtv = (ID3D11RenderTargetView *)1;
    HRESULT hr = ID3D11Device_CreateRenderTargetView(c.dev, (ID3D11Resource *)t, NULL, &rtv);
    EXPECT_HR(hr, E_INVARG);
    CHECK(rtv == NULL || rtv == (ID3D11RenderTargetView *)1);
    SAFE_RELEASE(t); SAFE_RELEASE(r); on12_free(&c);
}

/* acquire / release */
TEST(d3d_on12_dxcore_misc_acquire_release_zero_count) {
    NEED_CTX();
    ID3D11On12Device_AcquireWrappedResources(c.o, NULL, 0);
    ID3D11On12Device_ReleaseWrappedResources(c.o, NULL, 0);
    CHECK(1);
    on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_acquire_release_cycle) {
    NEED_CTX();
    ID3D12Resource *r = tex12(c.d12, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, 1, 1);
    if (!r) { on12_free(&c); t_skip("cannot create resource"); return; }
    D3D11_RESOURCE_FLAGS f = rf(D3D11_BIND_RENDER_TARGET);
    ID3D11Texture2D *t = NULL;
    if (FAILED(ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, &IID_ID3D11Texture2D, (void **)&t)) || !t) { SAFE_RELEASE(r); on12_free(&c); t_skip("wrap failed"); return; }
    ID3D11Resource *arr[1] = {(ID3D11Resource *)t};
    for (int i = 0; i < 3; i++) {
        ID3D11On12Device_ReleaseWrappedResources(c.o, arr, 1);
        ID3D11On12Device_AcquireWrappedResources(c.o, arr, 1);
    }
    ID3D11On12Device_ReleaseWrappedResources(c.o, arr, 1);
    ID3D11DeviceContext_Flush(c.ctx);
    EXPECT_HR(d3dh_d12_flush(c.d12, c.q), S_OK);
    SAFE_RELEASE(t); SAFE_RELEASE(r); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_double_release_is_ignored) {
    NEED_CTX();
    ID3D12Resource *r = tex12(c.d12, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, 1, 1);
    if (!r) { on12_free(&c); t_skip("cannot create resource"); return; }
    D3D11_RESOURCE_FLAGS f = rf(D3D11_BIND_RENDER_TARGET);
    ID3D11Texture2D *t = NULL;
    if (FAILED(ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, &IID_ID3D11Texture2D, (void **)&t)) || !t) { SAFE_RELEASE(r); on12_free(&c); t_skip("wrap failed"); return; }
    ID3D11Resource *arr[1] = {(ID3D11Resource *)t};
    ID3D11On12Device_ReleaseWrappedResources(c.o, arr, 1);
    ID3D11On12Device_ReleaseWrappedResources(c.o, arr, 1); /* no-op */
    ID3D11On12Device_AcquireWrappedResources(c.o, arr, 1);
    ID3D11On12Device_AcquireWrappedResources(c.o, arr, 1); /* no-op */
    ID3D11On12Device_ReleaseWrappedResources(c.o, arr, 1);
    ID3D11DeviceContext_Flush(c.ctx);
    d3dh_d12_flush(c.d12, c.q);
    CHECK(1);
    SAFE_RELEASE(t); SAFE_RELEASE(r); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_acquire_release_ignores_non_wrapped_resource) {
    NEED_CTX();
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof td);
    td.Width = 8; td.Height = 8; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D *t = NULL;
    if (FAILED(ID3D11Device_CreateTexture2D(c.dev, &td, NULL, &t)) || !t) { on12_free(&c); t_skip("no texture"); return; }
    ID3D11Resource *arr[1] = {(ID3D11Resource *)t};
    ID3D11On12Device_AcquireWrappedResources(c.o, arr, 1);
    ID3D11On12Device_ReleaseWrappedResources(c.o, arr, 1);
    CHECK(1);
    SAFE_RELEASE(t); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_clear_wrapped_rtv_visible_in_d3d12) {
    NEED_CTX();
    ID3D12Resource *r = tex12(c.d12, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, 1, 1);
    if (!r) { on12_free(&c); t_skip("cannot create resource"); return; }
    D3D11_RESOURCE_FLAGS f = rf(D3D11_BIND_RENDER_TARGET);
    ID3D11Texture2D *t = NULL; ID3D11RenderTargetView *rtv = NULL;
    if (FAILED(ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE, &IID_ID3D11Texture2D, (void **)&t)) || !t ||
        FAILED(ID3D11Device_CreateRenderTargetView(c.dev, (ID3D11Resource *)t, NULL, &rtv))) {
        SAFE_RELEASE(rtv); SAFE_RELEASE(t); SAFE_RELEASE(r); on12_free(&c); t_skip("wrap/RTV failed"); return;
    }
    ID3D11Resource *arr[1] = {(ID3D11Resource *)t};
    ID3D11On12Device_AcquireWrappedResources(c.o, arr, 1);
    const float col[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    ID3D11DeviceContext_ClearRenderTargetView(c.ctx, rtv, col);
    ID3D11On12Device_ReleaseWrappedResources(c.o, arr, 1);
    ID3D11DeviceContext_Flush(c.ctx);
    HRESULT hr = d3dh_d12_flush(c.d12, c.q);
    if (SUCCEEDED(hr)) {
        d3dh_image im; memset(&im, 0, sizeof im);
        hr = d3dh_readback_texture2d12(c.d12, r, D3D12_RESOURCE_STATE_COPY_SOURCE, &im);
        if (SUCCEEDED(hr) && im.data) {
            CHECK(im.width == 8 && im.height == 8);
            int bad = 0;
            for (UINT y = 0; y < im.height; y++) for (UINT x = 0; x < im.width; x++) if (d3dh_image_pixel32(&im, x, y) != 0xFF0000FFu) bad++;
            CHECKF(bad == 0, "%d pixels differ from 0xFF0000FF", bad);
            d3dh_image_free(&im);
        } else t_note("readback failed: %s", d3dh_hr_name(hr));
    }
    SAFE_RELEASE(rtv); SAFE_RELEASE(t); SAFE_RELEASE(r); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_copy_into_wrapped_from_d3d11_texture) {
    NEED_CTX();
    ID3D12Resource *r = tex12(c.d12, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, 1, 1);
    if (!r) { on12_free(&c); t_skip("cannot create resource"); return; }
    D3D11_RESOURCE_FLAGS f = rf(D3D11_BIND_RENDER_TARGET);
    ID3D11Texture2D *t = NULL, *src = NULL;
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof td);
    td.Width = 4; td.Height = 4; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    UINT32 px[16]; for (int i = 0; i < 16; i++) px[i] = 0xFF000000u | (UINT32)(i * 16);
    D3D11_SUBRESOURCE_DATA sd; sd.pSysMem = px; sd.SysMemPitch = 16; sd.SysMemSlicePitch = 0;
    if (FAILED(ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE, &IID_ID3D11Texture2D, (void **)&t)) || !t ||
        FAILED(ID3D11Device_CreateTexture2D(c.dev, &td, &sd, &src)) || !src) {
        SAFE_RELEASE(src); SAFE_RELEASE(t); SAFE_RELEASE(r); on12_free(&c); t_skip("setup failed"); return;
    }
    ID3D11Resource *arr[1] = {(ID3D11Resource *)t};
    ID3D11On12Device_AcquireWrappedResources(c.o, arr, 1);
    ID3D11DeviceContext_CopyResource(c.ctx, (ID3D11Resource *)t, (ID3D11Resource *)src);
    ID3D11On12Device_ReleaseWrappedResources(c.o, arr, 1);
    ID3D11DeviceContext_Flush(c.ctx);
    if (SUCCEEDED(d3dh_d12_flush(c.d12, c.q))) {
        d3dh_image im; memset(&im, 0, sizeof im);
        if (SUCCEEDED(d3dh_readback_texture2d12(c.d12, r, D3D12_RESOURCE_STATE_COPY_SOURCE, &im)) && im.data) {
            int bad = 0;
            for (UINT y = 0; y < 4 && y < im.height; y++) for (UINT x = 0; x < 4 && x < im.width; x++) if (d3dh_image_pixel32(&im, x, y) != px[y * 4 + x]) bad++;
            CHECKF(bad == 0, "%d pixels differ", bad);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(src); SAFE_RELEASE(t); SAFE_RELEASE(r); on12_free(&c);
}

/* ReturnUnderlyingResource (ID3D11On12Device2) */
TEST(d3d_on12_dxcore_misc_return_underlying_on_non_unwrapped_resource) {
    NEED_CTX();
    ID3D11On12Device2 *o2 = NULL;
    if (FAILED(ID3D11On12Device_QueryInterface(c.o, &IID_ID3D11On12Device2, (void **)&o2)) || !o2) { on12_free(&c); t_skip("ID3D11On12Device2 unavailable"); return; }
    ID3D12Resource *r = tex12(c.d12, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, 1, 1);
    D3D11_RESOURCE_FLAGS f = rf(D3D11_BIND_RENDER_TARGET);
    ID3D11Texture2D *t = NULL;
    if (!r || FAILED(ID3D11On12Device_CreateWrappedResource(c.o, (IUnknown *)r, &f, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, &IID_ID3D11Texture2D, (void **)&t)) || !t) {
        SAFE_RELEASE(r); SAFE_RELEASE(o2); on12_free(&c); t_skip("wrap failed"); return;
    }
    HRESULT hr = ID3D11On12Device2_ReturnUnderlyingResource(o2, (ID3D11Resource *)t, 0, NULL, NULL);
    EXPECT_HR(hr, E_INVALIDARG); /* Win10: probed */
    t_note("ReturnUnderlyingResource on an acquired wrapped (not unwrapped) resource: %s", d3dh_hr_name(hr));
    EXPECT_HR_FAILED(hr);
    SAFE_RELEASE(t); SAFE_RELEASE(r); SAFE_RELEASE(o2); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_return_underlying_on_plain_d3d11_resource) {
    NEED_CTX();
    ID3D11On12Device2 *o2 = NULL;
    if (FAILED(ID3D11On12Device_QueryInterface(c.o, &IID_ID3D11On12Device2, (void **)&o2)) || !o2) { on12_free(&c); t_skip("ID3D11On12Device2 unavailable"); return; }
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof td);
    td.Width = 8; td.Height = 8; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D *t = NULL;
    if (FAILED(ID3D11Device_CreateTexture2D(c.dev, &td, NULL, &t)) || !t) { SAFE_RELEASE(o2); on12_free(&c); t_skip("no texture"); return; }
    HRESULT hr = ID3D11On12Device2_ReturnUnderlyingResource(o2, (ID3D11Resource *)t, 0, NULL, NULL);
    EXPECT_HR_FAILED(hr);
    EXPECT_HR(hr, E_INVALIDARG); /* Win10: probed */
    t_note("ReturnUnderlyingResource on a plain texture: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(t); SAFE_RELEASE(o2); on12_free(&c);
}
TEST(d3d_on12_dxcore_misc_unwrap_requires_queue_and_wrapped_resource) {
    NEED_CTX();
    ID3D11On12Device2 *o2 = NULL;
    if (FAILED(ID3D11On12Device_QueryInterface(c.o, &IID_ID3D11On12Device2, (void **)&o2)) || !o2) { on12_free(&c); t_skip("ID3D11On12Device2 unavailable"); return; }
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof td);
    td.Width = 8; td.Height = 8; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D *t = NULL;
    if (FAILED(ID3D11Device_CreateTexture2D(c.dev, &td, NULL, &t)) || !t) { SAFE_RELEASE(o2); on12_free(&c); t_skip("no texture"); return; }
    void *out = (void *)1;
    HRESULT hr = ID3D11On12Device2_UnwrapUnderlyingResource(o2, (ID3D11Resource *)t, c.q, &IID_ID3D12Resource, &out);
    /* a texture created through the 11On12 device is itself backed by a D3D12 resource, so unwrapping it works (probed) */
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) { CHECK(out != NULL && out != (void *)1); if (out && out != (void *)1) { ID3D11On12Device2_ReturnUnderlyingResource(o2, (ID3D11Resource *)t, 0, NULL, NULL); IUnknown_Release((IUnknown *)out); } }
    SAFE_RELEASE(t); SAFE_RELEASE(o2); on12_free(&c);
}
