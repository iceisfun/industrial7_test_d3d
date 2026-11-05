#include "objs.h"

/* DXGI_ERROR_* numeric values and facility. */
TEST(objs_extra_hresult_facility)
{
    static const struct { HRESULT hr; unsigned code; } t[] = {
        { DXGI_ERROR_INVALID_CALL, 1 }, { DXGI_ERROR_NOT_FOUND, 2 }, { DXGI_ERROR_MORE_DATA, 3 },
        { DXGI_ERROR_UNSUPPORTED, 4 }, { DXGI_ERROR_DEVICE_REMOVED, 5 }, { DXGI_ERROR_DEVICE_HUNG, 6 },
        { DXGI_ERROR_DEVICE_RESET, 7 }, { DXGI_ERROR_WAS_STILL_DRAWING, 10 },
    };
    int i;
    for (i = 0; i < OBJS_ARRAY(t); i++) {
        CHECKF((unsigned)t[i].hr == (0x887A0000u | t[i].code), "entry %d: %#x", i, (unsigned)t[i].hr);
        CHECKF(HRESULT_FACILITY(t[i].hr) == 0x87A, "facility %d", i);
        CHECKF(FAILED(t[i].hr), "failed %d", i);
    }
}

/* Private data set on one object is not visible through another object. */
TEST(objs_extra_privdata_per_object)
{
    NEED_DEV(c);
    ID3D11Texture2D *a = NULL, *b = NULL; IDXGIObject *oa = NULL, *ob = NULL;
    UINT v = 0x1234, sz = sizeof(v), out = 0;
    EXPECT_HR_OK(objs_rt(c.dev, &a));
    EXPECT_HR_OK(objs_rt(c.dev, &b));
    if (!a || !b) { SAFE_RELEASE(a); SAFE_RELEASE(b); objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(a, &IID_IDXGIObject, (void **)&oa));
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(b, &IID_IDXGIObject, (void **)&ob));
    EXPECT_HR_OK(IDXGIObject_SetPrivateData(oa, &OBJS_G1, sizeof(v), &v));
    EXPECT_HR(IDXGIObject_GetPrivateData(ob, &OBJS_G1, &sz, &out), DXGI_ERROR_NOT_FOUND);
    sz = sizeof(out);
    EXPECT_HR_OK(IDXGIObject_GetPrivateData(oa, &OBJS_G1, &sz, &out));
    EXPECT_EQ(out, 0x1234);
    SAFE_RELEASE(oa); SAFE_RELEASE(ob); ID3D11Texture2D_Release(a); ID3D11Texture2D_Release(b);
    objs_close(&c);
}

/* A device can be asked for its adapter repeatedly and the adapter's parent factory is stable. */
TEST(objs_extra_adapter_factory_stable)
{
    NEED_DEV(c);
    IDXGIAdapter *a = NULL; IDXGIFactory *f1 = NULL, *f2 = NULL;
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a));
    if (a) {
        EXPECT_HR_OK(IDXGIAdapter_GetParent(a, &IID_IDXGIFactory, (void **)&f1));
        EXPECT_HR_OK(IDXGIAdapter_GetParent(a, &IID_IDXGIFactory, (void **)&f2));
        EXPECT_EQ(f1, f2);
        SAFE_RELEASE(f1); SAFE_RELEASE(f2); IDXGIAdapter_Release(a);
    }
    objs_close(&c);
}

/* SetMaximumFrameLatency: valid range boundaries keep value verbatim (1 and 16). */
TEST(objs_extra_latency_bounds)
{
    NEED_DEV(c);
    IDXGIDevice1 *d = NULL; UINT l = 0;
    if (FAILED(IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIDevice1, (void **)&d))) { t_skip("no IDXGIDevice1"); objs_close(&c); return; }
    EXPECT_HR_OK(IDXGIDevice1_SetMaximumFrameLatency(d, 16));
    EXPECT_HR_OK(IDXGIDevice1_GetMaximumFrameLatency(d, &l));
    EXPECT_EQ(l, 16);
    EXPECT_HR(IDXGIDevice1_SetMaximumFrameLatency(d, 17), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR_OK(IDXGIDevice1_GetMaximumFrameLatency(d, &l));
    EXPECT_EQ(l, 16);
    EXPECT_HR_OK(IDXGIDevice1_SetMaximumFrameLatency(d, 1));
    EXPECT_HR_OK(IDXGIDevice1_GetMaximumFrameLatency(d, &l));
    EXPECT_EQ(l, 1);
    IDXGIDevice1_Release(d); objs_close(&c);
}
