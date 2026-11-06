/* d3d12_cmdlist_ext: ID3D12GraphicsCommandList1..7 / ID3D12Device1..10 methods that the other D3D12 areas do not call.
 * Every pin was measured on Windows 10 22H2 with the Basic Render Driver (WARP12). Capability-gated methods are only
 * called when CheckFeatureSupport reports the capability, and the reported capability itself is pinned on WARP. */
#ifndef CLE_H
#define CLE_H
#include "../d3d12_objects/d12o.h"

#define CFS(d, f, p) ID3D12Device_CheckFeatureSupport((d), (f), &(p), sizeof(p))
static int cle_warp(void) { return g_d12warp; }
/* QI the shared device for interface version iid; NULL when unsupported. */
static void *cle_qi(ID3D12Device *dev, REFIID iid) { void *p = NULL; if (FAILED(ID3D12Device_QueryInterface(dev, iid, &p))) return NULL; return p; }
static void *cle_qi_cl(ID3D12GraphicsCommandList *cl, REFIID iid) { void *p = NULL; if (FAILED(ID3D12GraphicsCommandList_QueryInterface(cl, iid, &p))) return NULL; return p; }
/* A resource in the DEFAULT heap with UAV flag (buffer). */
static HRESULT cle_uav_buf(ID3D12Device *dev, UINT64 size, ID3D12Resource **out) {
    return d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, out);
}
/* Copy a default-heap buffer (state `before`) to a readback buffer and return the bytes (caller free()s). */
static void *cle_read(ID3D12Device *dev, ID3D12Resource *src, D3D12_RESOURCE_STATES before, size_t *n) {
    void *p = NULL; if (FAILED(d3dh_readback_buffer12(dev, src, before, &p, n))) return NULL; return p;
}
/* Pin a value measured on the Basic Render Driver (WARP12); on other adapters it is only logged. */
#define WARP_HR(hr, want) do { HRESULT wh_ = (HRESULT)(hr); if (cle_warp()) EXPECT_HR(wh_, want); else t_note(#hr " = %s", d3dh_hr_name(wh_)); } while (0)
#define WARP_EQ(a, b) do { ULONG_PTR wa_ = (ULONG_PTR)(a), wb_ = (ULONG_PTR)(b); if (cle_warp()) { EXPECT_EQ(wa_, wb_); } else t_note(#a " = 0x%llx", (unsigned long long)wa_); } while (0)
#endif
