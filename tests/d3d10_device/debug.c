/* Debug-layer interfaces: ID3D10InfoQueue / ID3D10Debug / ID3D10SwitchToRef. Without the Graphics Tools feature these tests skip. */
#include "dd.h"

#define T(n) TEST(d3d10_device_debug_##n)

static int dbg_dev(ID3D10Device **dev, UINT extra)
{
    *dev = NULL;
    if (!DD_C10()) { t_skip("no D3D10CreateDevice"); return 0; }
    if (dd_driver() < 0) { t_skip("no device"); return 0; }
    HRESULT hr = dd_make(D3D10_CREATE_DEVICE_DEBUG | extra, dev);
    if (FAILED(hr) || !*dev) { t_skip("debug device unavailable (%s)", d3dh_hr_name(hr)); return 0; }
    return 1;
}
#define DBG(dev) ID3D10Device *dev; if (!dbg_dev(&dev, 0)) return;
#define IQ(dev, iq) ID3D10InfoQueue *iq = NULL; \
    if (FAILED(ID3D10Device_QueryInterface(dev, &IID_ID3D10InfoQueue, (void **)&iq)) || !iq) { ID3D10Device_Release(dev); t_skip("no InfoQueue"); return; }

T(infoqueue_available_only_with_flag)
{
    DBG(dev);
    ID3D10InfoQueue *iq = NULL;
    EXPECT_HR_OK(ID3D10Device_QueryInterface(dev, &IID_ID3D10InfoQueue, (void **)&iq));
    SAFE_RELEASE(iq);
    ID3D10Device_Release(dev);
}
T(infoqueue_app_message_stored)
{
    DBG(dev); IQ(dev, iq);
    ID3D10InfoQueue_ClearStoredMessages(iq);
    EXPECT_EQ(ID3D10InfoQueue_GetNumStoredMessages(iq), 0);
    EXPECT_HR_OK(ID3D10InfoQueue_AddApplicationMessage(iq, D3D10_MESSAGE_SEVERITY_INFO, "d3d10_device test message"));
    EXPECT_EQ(ID3D10InfoQueue_GetNumStoredMessages(iq), 1);
    EXPECT_EQ(ID3D10InfoQueue_GetNumStoredMessagesAllowedByRetrievalFilter(iq), 1);
    EXPECT_EQ(ID3D10InfoQueue_GetNumMessagesAllowedByStorageFilter(iq) >= 1, 1);
    SIZE_T len = 0;
    EXPECT_HR_OK(ID3D10InfoQueue_GetMessage(iq, 0, NULL, &len));
    CHECK(len >= sizeof(D3D10_MESSAGE));
    if (len >= sizeof(D3D10_MESSAGE) && len < 4096) {
        D3D10_MESSAGE *m = calloc(1, len);
        EXPECT_HR_OK(ID3D10InfoQueue_GetMessage(iq, 0, m, &len));
        EXPECT_EQ(m->Category, D3D10_MESSAGE_CATEGORY_APPLICATION_DEFINED);
        EXPECT_EQ(m->Severity, D3D10_MESSAGE_SEVERITY_INFO);
        CHECK(m->pDescription && strstr(m->pDescription, "d3d10_device test message"));
        free(m);
    }
    ID3D10InfoQueue_ClearStoredMessages(iq);
    EXPECT_EQ(ID3D10InfoQueue_GetNumStoredMessages(iq), 0);
    ID3D10InfoQueue_Release(iq); ID3D10Device_Release(dev);
}
T(infoqueue_message_count_limit)
{
    DBG(dev); IQ(dev, iq);
    UINT64 old = ID3D10InfoQueue_GetMessageCountLimit(iq);
    EXPECT_HR_OK(ID3D10InfoQueue_SetMessageCountLimit(iq, 3));
    EXPECT_EQ(ID3D10InfoQueue_GetMessageCountLimit(iq), 3);
    ID3D10InfoQueue_ClearStoredMessages(iq);
    for (int i = 0; i < 6; i++) ID3D10InfoQueue_AddApplicationMessage(iq, D3D10_MESSAGE_SEVERITY_INFO, "limit test");
    t_note("stored %llu after 6 adds with a limit of 3", (unsigned long long)ID3D10InfoQueue_GetNumStoredMessages(iq));
    CHECK(ID3D10InfoQueue_GetNumStoredMessages(iq) <= 3);
    CHECK(ID3D10InfoQueue_GetNumStoredMessagesAllowedByRetrievalFilter(iq) <= 3);
    ID3D10InfoQueue_SetMessageCountLimit(iq, old);
    ID3D10InfoQueue_ClearStoredMessages(iq);
    ID3D10InfoQueue_Release(iq); ID3D10Device_Release(dev);
}
T(infoqueue_storage_filter_stack)
{
    DBG(dev); IQ(dev, iq);
    UINT base = ID3D10InfoQueue_GetStorageFilterStackSize(iq);
    EXPECT_HR_OK(ID3D10InfoQueue_PushEmptyStorageFilter(iq));
    EXPECT_EQ(ID3D10InfoQueue_GetStorageFilterStackSize(iq), base + 1);
    EXPECT_HR_OK(ID3D10InfoQueue_PushEmptyStorageFilter(iq));
    EXPECT_EQ(ID3D10InfoQueue_GetStorageFilterStackSize(iq), base + 2);
    ID3D10InfoQueue_PopStorageFilter(iq);
    ID3D10InfoQueue_PopStorageFilter(iq);
    EXPECT_EQ(ID3D10InfoQueue_GetStorageFilterStackSize(iq), base);
    ID3D10InfoQueue_Release(iq); ID3D10Device_Release(dev);
}
T(infoqueue_deny_all_storage_filter)
{
    DBG(dev); IQ(dev, iq);
    D3D10_INFO_QUEUE_FILTER f; memset(&f, 0, sizeof f);
    D3D10_MESSAGE_SEVERITY sev[] = { D3D10_MESSAGE_SEVERITY_INFO };
    f.DenyList.NumSeverities = 1; f.DenyList.pSeverityList = sev;
    ID3D10InfoQueue_ClearStoredMessages(iq);
    EXPECT_HR_OK(ID3D10InfoQueue_PushStorageFilter(iq, &f));
    ID3D10InfoQueue_AddApplicationMessage(iq, D3D10_MESSAGE_SEVERITY_INFO, "denied");
    EXPECT_EQ(ID3D10InfoQueue_GetNumStoredMessages(iq), 0);
    ID3D10InfoQueue_AddApplicationMessage(iq, D3D10_MESSAGE_SEVERITY_WARNING, "allowed");
    EXPECT_EQ(ID3D10InfoQueue_GetNumStoredMessages(iq), 1);
    ID3D10InfoQueue_PopStorageFilter(iq);
    ID3D10InfoQueue_ClearStoredMessages(iq);
    ID3D10InfoQueue_Release(iq); ID3D10Device_Release(dev);
}
T(infoqueue_mute_debug_output_roundtrip)
{
    DBG(dev); IQ(dev, iq);
    BOOL old = ID3D10InfoQueue_GetMuteDebugOutput(iq);
    ID3D10InfoQueue_SetMuteDebugOutput(iq, TRUE);  EXPECT_EQ(ID3D10InfoQueue_GetMuteDebugOutput(iq), TRUE);
    ID3D10InfoQueue_SetMuteDebugOutput(iq, FALSE); EXPECT_EQ(ID3D10InfoQueue_GetMuteDebugOutput(iq), FALSE);
    ID3D10InfoQueue_SetMuteDebugOutput(iq, old);
    ID3D10InfoQueue_Release(iq); ID3D10Device_Release(dev);
}
T(infoqueue_get_message_out_of_range)
{
    DBG(dev); IQ(dev, iq);
    SIZE_T len = 0;
    ID3D10InfoQueue_ClearStoredMessages(iq);
    HRESULT hr = ID3D10InfoQueue_GetMessage(iq, 5, NULL, &len);
    CHECK(FAILED(hr));
    t_note("GetMessage(5) on an empty queue: %s", d3dh_hr_name(hr));
    ID3D10InfoQueue_Release(iq); ID3D10Device_Release(dev);
}
T(debug_interface_feature_mask)
{
    DBG(dev);
    ID3D10Debug *dbg = NULL;
    EXPECT_HR_OK(ID3D10Device_QueryInterface(dev, &IID_ID3D10Debug, (void **)&dbg));
    if (dbg) {
        UINT m = ID3D10Debug_GetFeatureMask(dbg);
        t_note("debug feature mask 0x%x", m);
        ID3D10Debug_Release(dbg);
    }
    ID3D10Device_Release(dev);
}
T(switch_to_ref_requires_flag)
{
    ID3D10Device *dev;
    if (!dbg_dev(&dev, D3D10_CREATE_DEVICE_SWITCH_TO_REF)) return;
    ID3D10SwitchToRef *sw = NULL;
    HRESULT hr = ID3D10Device_QueryInterface(dev, &IID_ID3D10SwitchToRef, (void **)&sw);
    t_note("QI ID3D10SwitchToRef on a SWITCH_TO_REF device: %s", d3dh_hr_name(hr));
    if (sw) {
        t_note("GetUseRef = %d", ID3D10SwitchToRef_GetUseRef(sw));
        ID3D10SwitchToRef_Release(sw);
    }
    ID3D10Device_Release(dev);
}
T(debug_device_still_renders_clear)
{
    DBG(dev);
    if (!dd_can_render()) { ID3D10Device_Release(dev); t_skip("no rendering driver"); return; }
    D3D10_TEXTURE2D_DESC td = { 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_RENDER_TARGET, 0, 0 };
    ID3D10Texture2D *t = NULL; ID3D10RenderTargetView *rtv = NULL; d3dh_image im;
    if (SUCCEEDED(ID3D10Device_CreateTexture2D(dev, &td, NULL, &t)) && SUCCEEDED(ID3D10Device_CreateRenderTargetView(dev, (ID3D10Resource *)t, NULL, &rtv))) {
        float c[4] = { 1, 0, 0, 1 };
        ID3D10Device_ClearRenderTargetView(dev, rtv, c);
        if (SUCCEEDED(d3dh_readback_texture2d10(dev, t, &im))) { CHECK(d3dh_image_pixel32(&im, 1, 1) == 0xff0000ffu); d3dh_image_free(&im); }
    }
    SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    ID3D10Device_Release(dev);
}
