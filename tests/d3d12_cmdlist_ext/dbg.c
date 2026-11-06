/* D3D12 debug-layer, InfoQueue, DRED and tools interfaces. Each test runs in a child process: enabling the debug layer is process-wide. */
#include "cle.h"

static int g_cb_count; static D3D12_MESSAGE_ID g_cb_id; static D3D12_MESSAGE_SEVERITY g_cb_sev;
static void CALLBACK cle_msg_cb(D3D12_MESSAGE_CATEGORY c, D3D12_MESSAGE_SEVERITY s, D3D12_MESSAGE_ID id, LPCSTR d, void *ctx) { g_cb_count++; g_cb_id = id; g_cb_sev = s; }

#define STEP(n, cond) do { if (!(cond)) { printf("  child step %d failed: %s (line %d)\n", (n), #cond, __LINE__); fails |= 1u << (n); } } while (0)

CHILD(cle_debug_layer) {
    int fails = 0;
    __typeof__(&D3D12GetDebugInterface) gdi = (__typeof__(&D3D12GetDebugInterface))d3dh_getproc("d3d12.dll", "D3D12GetDebugInterface");
    if (!gdi || !d3dh_D3D12CreateDevice()) return 77;
    ID3D12Debug *dbg = NULL; HRESULT hr = gdi(&IID_ID3D12Debug, (void **)&dbg);
    printf("  D3D12GetDebugInterface(ID3D12Debug) = %s\n", d3dh_hr_name(hr));
    if (FAILED(hr) || !dbg) return 78;
    ID3D12Debug_EnableDebugLayer(dbg);
    ID3D12Debug1 *d1 = NULL; hr = ID3D12Debug_QueryInterface(dbg, &IID_ID3D12Debug1, (void **)&d1);
    printf("  QI Debug1 = %s\n", d3dh_hr_name(hr)); STEP(8, SUCCEEDED(hr));
    if (d1) { ID3D12Debug1_SetEnableGPUBasedValidation(d1, FALSE); ID3D12Debug1_SetEnableSynchronizedCommandQueueValidation(d1, TRUE); ID3D12Debug1_Release(d1); }
    ID3D12Debug2 *d2 = NULL; hr = ID3D12Debug_QueryInterface(dbg, &IID_ID3D12Debug2, (void **)&d2);
    printf("  QI Debug2 = %s\n", d3dh_hr_name(hr)); STEP(9, SUCCEEDED(hr));
    if (d2) { ID3D12Debug2_SetGPUBasedValidationFlags(d2, D3D12_GPU_BASED_VALIDATION_FLAGS_NONE); ID3D12Debug2_Release(d2); }
    ID3D12Debug3 *d3 = NULL; hr = ID3D12Debug_QueryInterface(dbg, &IID_ID3D12Debug3, (void **)&d3); printf("  QI Debug3 = %s\n", d3dh_hr_name(hr)); STEP(10, SUCCEEDED(hr)); if (d3) ID3D12Debug3_Release(d3);
    ID3D12Debug4 *d4 = NULL; hr = ID3D12Debug_QueryInterface(dbg, &IID_ID3D12Debug4, (void **)&d4); printf("  QI Debug4 = %s\n", d3dh_hr_name(hr)); if (d4) ID3D12Debug4_Release(d4);
    ID3D12Debug5 *d5 = NULL; hr = ID3D12Debug_QueryInterface(dbg, &IID_ID3D12Debug5, (void **)&d5); printf("  QI Debug5 = %s\n", d3dh_hr_name(hr)); if (d5) { ID3D12Debug5_SetEnableAutoName(d5, TRUE); ID3D12Debug5_Release(d5); }
    ID3D12Device *dev = NULL; int warp = 0;
    hr = d3dh_make_device12(&dev, D3D_FEATURE_LEVEL_11_0, &warp);
    printf("  device with debug layer = %s warp=%d\n", d3dh_hr_name(hr), warp);
    STEP(1, SUCCEEDED(hr) && dev);
    if (!dev) return 101;
    ID3D12DebugDevice *dd = NULL; hr = ID3D12Device_QueryInterface(dev, &IID_ID3D12DebugDevice, (void **)&dd);
    printf("  QI DebugDevice = %s\n", d3dh_hr_name(hr)); STEP(2, SUCCEEDED(hr) && dd);
    if (dd) {
        D3D12_DEBUG_FEATURE f = ID3D12DebugDevice_GetFeatureMask(dd); printf("  FeatureMask = 0x%x\n", (unsigned)f); STEP(11, f == 0);
        hr = ID3D12DebugDevice_SetFeatureMask(dd, D3D12_DEBUG_FEATURE_NONE); printf("  SetFeatureMask(NONE) = %s mask now 0x%x\n", d3dh_hr_name(hr), (unsigned)ID3D12DebugDevice_GetFeatureMask(dd));
        ID3D12DebugDevice_SetFeatureMask(dd, f);
        hr = ID3D12DebugDevice_ReportLiveDeviceObjects(dd, D3D12_RLDO_SUMMARY); printf("  ReportLiveDeviceObjects = %s\n", d3dh_hr_name(hr)); STEP(3, SUCCEEDED(hr));
        ID3D12DebugDevice1 *dd1 = NULL; hr = ID3D12DebugDevice_QueryInterface(dd, &IID_ID3D12DebugDevice1, (void **)&dd1); printf("  QI DebugDevice1 = %s\n", d3dh_hr_name(hr));
        if (dd1) { D3D12_DEBUG_DEVICE_GPU_BASED_VALIDATION_SETTINGS gs; memset(&gs, 0, sizeof gs); hr = ID3D12DebugDevice1_GetDebugParameter(dd1, D3D12_DEBUG_DEVICE_PARAMETER_GPU_BASED_VALIDATION_SETTINGS, &gs, sizeof gs); printf("  GetDebugParameter(GBV) = %s\n", d3dh_hr_name(hr)); ID3D12DebugDevice1_Release(dd1); }
        ID3D12DebugDevice_Release(dd);
    }
    ID3D12InfoQueue *iq = NULL; hr = ID3D12Device_QueryInterface(dev, &IID_ID3D12InfoQueue, (void **)&iq);
    printf("  QI InfoQueue = %s\n", d3dh_hr_name(hr)); STEP(4, SUCCEEDED(hr) && iq);
    if (iq) {
        ID3D12InfoQueue_ClearStoredMessages(iq);
        STEP(13, ID3D12InfoQueue_GetNumStoredMessages(iq) == 0);
        ID3D12InfoQueue1 *iq1 = NULL; hr = ID3D12InfoQueue_QueryInterface(iq, &IID_ID3D12InfoQueue1, (void **)&iq1); printf("  QI InfoQueue1 = %s\n", d3dh_hr_name(hr)); STEP(12, hr == E_NOINTERFACE);   /* ID3D12InfoQueue1 is not on this build */
        DWORD cookie = 0;
        if (iq1) { hr = ID3D12InfoQueue1_RegisterMessageCallback(iq1, cle_msg_cb, D3D12_MESSAGE_CALLBACK_FLAG_NONE, NULL, &cookie); printf("  RegisterMessageCallback = %s cookie=%lu\n", d3dh_hr_name(hr), cookie); STEP(5, SUCCEEDED(hr)); }
        /* provoke an ERROR message: committed resource with a zero-width buffer */
        D3D12_HEAP_PROPERTIES hp = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT); D3D12_RESOURCE_DESC bd = d12_buf_desc(0, D3D12_RESOURCE_FLAG_NONE); ID3D12Resource *r = NULL;
        hr = ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r);
        printf("  CreateCommittedResource(width 0) = %s\n", d3dh_hr_name(hr)); if (r) ID3D12Resource_Release(r);
        UINT64 n = ID3D12InfoQueue_GetNumStoredMessages(iq);
        printf("  stored after provoking = %llu, callback count=%d id=%d sev=%d\n", (unsigned long long)n, g_cb_count, g_cb_id, g_cb_sev);
        STEP(6, n == 1 && hr == E_INVALIDARG);
        if (n >= 1) {
            SIZE_T sz = 0; hr = ID3D12InfoQueue_GetMessage(iq, 0, NULL, &sz); printf("  GetMessage(NULL) = %s size=%zu\n", d3dh_hr_name(hr), (size_t)sz);
            D3D12_MESSAGE *m = malloc(sz ? sz : 1); hr = ID3D12InfoQueue_GetMessage(iq, 0, m, &sz);
            STEP(14, SUCCEEDED(hr) && (int)m->Category == 5 && (int)m->Severity == D3D12_MESSAGE_SEVERITY_ERROR && (int)m->ID == 597 && m->DescriptionByteLength > 0   /* category STATE_CREATION, id 597 */);
            free(m);
        }
        ID3D12InfoQueue_AddApplicationMessage(iq, D3D12_MESSAGE_SEVERITY_WARNING, "cle app message");
        STEP(15, ID3D12InfoQueue_GetNumStoredMessages(iq) == 2);
        D3D12_INFO_QUEUE_FILTER flt; memset(&flt, 0, sizeof flt); D3D12_MESSAGE_SEVERITY sv[1] = { D3D12_MESSAGE_SEVERITY_WARNING };
        flt.DenyList.NumSeverities = 1; flt.DenyList.pSeverityList = sv;
        hr = ID3D12InfoQueue_PushStorageFilter(iq, &flt); printf("  PushStorageFilter = %s\n", d3dh_hr_name(hr)); STEP(7, SUCCEEDED(hr));
        STEP(16, ID3D12InfoQueue_GetStorageFilterStackSize(iq) == 2);   /* the queue starts with one default filter */
        UINT64 before = ID3D12InfoQueue_GetNumStoredMessages(iq);
        ID3D12InfoQueue_AddApplicationMessage(iq, D3D12_MESSAGE_SEVERITY_WARNING, "denied");
        STEP(17, ID3D12InfoQueue_GetNumStoredMessages(iq) == before);
        ID3D12InfoQueue_PopStorageFilter(iq);
        ID3D12InfoQueue_SetMuteDebugOutput(iq, TRUE); STEP(18, ID3D12InfoQueue_GetMuteDebugOutput(iq) == TRUE);
        STEP(19, ID3D12InfoQueue_GetMessageCountLimit(iq) == 1024);
        if (iq1) { hr = ID3D12InfoQueue1_UnregisterMessageCallback(iq1, cookie); printf("  UnregisterMessageCallback = %s\n", d3dh_hr_name(hr)); ID3D12InfoQueue1_Release(iq1); }
        ID3D12InfoQueue_Release(iq);
    }
    ID3D12Device_Release(dev); ID3D12Debug_Release(dbg);
    return fails ? 100 : 0;
}
TEST(cle_debug_layer_interfaces) {
    if (!d3dh_D3D12CreateDevice()) { t_skip("no D3D12"); return; }
    DWORD rc = t_run_child("cle_debug_layer", "", 120000);
    if (rc == 77 || rc == 78) { t_skip("debug layer not installed (child rc %lu)", rc); return; }
    EXPECT_EQ(rc, 0);
}

CHILD(cle_dred) {
    int bad = 0;
    if (!d3dh_D3D12CreateDevice()) return 77;
    __typeof__(&D3D12GetDebugInterface) gdi = (__typeof__(&D3D12GetDebugInterface))d3dh_getproc("d3d12.dll", "D3D12GetDebugInterface");
    if (!gdi) return 77;
    ID3D12DeviceRemovedExtendedDataSettings *st = NULL; HRESULT hr = gdi(&IID_ID3D12DeviceRemovedExtendedDataSettings, (void **)&st);
    printf("  DRED settings interface = %s\n", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && st) {
        ID3D12DeviceRemovedExtendedDataSettings_SetAutoBreadcrumbsEnablement(st, D3D12_DRED_ENABLEMENT_FORCED_ON);
        ID3D12DeviceRemovedExtendedDataSettings_SetPageFaultEnablement(st, D3D12_DRED_ENABLEMENT_FORCED_ON);
        ID3D12DeviceRemovedExtendedDataSettings_SetWatsonDumpEnablement(st, D3D12_DRED_ENABLEMENT_FORCED_OFF);
        ID3D12DeviceRemovedExtendedDataSettings1 *s1 = NULL; hr = ID3D12DeviceRemovedExtendedDataSettings_QueryInterface(st, &IID_ID3D12DeviceRemovedExtendedDataSettings1, (void **)&s1);
        printf("  QI settings1 = %s\n", d3dh_hr_name(hr)); if (s1) { ID3D12DeviceRemovedExtendedDataSettings1_SetBreadcrumbContextEnablement(s1, D3D12_DRED_ENABLEMENT_FORCED_ON); ID3D12DeviceRemovedExtendedDataSettings1_Release(s1); }
        ID3D12DeviceRemovedExtendedDataSettings_Release(st);
    }
    ID3D12Device *dev = NULL; int warp = 0; hr = d3dh_make_device12(&dev, D3D_FEATURE_LEVEL_11_0, &warp); if (FAILED(hr) || !dev) return 78;
    ID3D12DeviceRemovedExtendedData *dd = NULL; hr = ID3D12Device_QueryInterface(dev, &IID_ID3D12DeviceRemovedExtendedData, (void **)&dd);
    printf("  QI DRED = %s\n", d3dh_hr_name(hr));
    if (dd) {
        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT ab; memset(&ab, 0xCD, sizeof ab); hr = ID3D12DeviceRemovedExtendedData_GetAutoBreadcrumbsOutput(dd, &ab); if (hr != DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) bad |= 1;
        D3D12_DRED_PAGE_FAULT_OUTPUT pf; memset(&pf, 0xCD, sizeof pf); hr = ID3D12DeviceRemovedExtendedData_GetPageFaultAllocationOutput(dd, &pf); if (hr != DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) bad |= 2;
        ID3D12DeviceRemovedExtendedData_Release(dd);
    }
    if (ID3D12Device_GetDeviceRemovedReason(dev) != S_OK) bad |= 4;
    ID3D12Device_Release(dev);
    return bad ? 100 + bad : 0;
}
TEST(cle_dred_interfaces) {
    if (!d3dh_D3D12CreateDevice()) { t_skip("no D3D12"); return; }
    DWORD rc = t_run_child("cle_dred", "", 120000);
    if (rc == 77 || rc == 78) { t_skip("DRED unavailable (child rc %lu)", rc); return; }
    EXPECT_EQ(rc, 0);
}
