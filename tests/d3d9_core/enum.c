/* IDirect3D9 / IDirect3D9Ex adapter and display-mode enumeration. */
#include "d9c.h"

static const D3DFORMAT k_modefmts[] = { D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, D3DFMT_A2R10G10B10, D3DFMT_A1R5G5B5, D3DFMT_A8R8G8B8 };
static const D3DFORMAT k_nonmodefmts[] = { D3DFMT_UNKNOWN, D3DFMT_DXT1, D3DFMT_D16, D3DFMT_D24S8, D3DFMT_L8, D3DFMT_A8, D3DFMT_V8U8, D3DFMT_INDEX16, D3DFMT_VERTEXDATA, D3DFMT_R16F, D3DFMT_A32B32G32R32F, D3DFMT_UYVY };
#define NMODEFMTS (sizeof k_modefmts / sizeof *k_modefmts)
#define NNONMODEFMTS (sizeof k_nonmodefmts / sizeof *k_nonmodefmts)

static int is_display_fmt(D3DFORMAT f) { return f == D3DFMT_X8R8G8B8 || f == D3DFMT_R5G6B5 || f == D3DFMT_X1R5G5B5 || f == D3DFMT_A2R10G10B10; }

/* ---- GetAdapterCount ---- */
TEST(d3d9_core_enum_adapter_count_sane) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    t_note("adapter count %u", n);
    CHECK(n <= 12);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_adapter_count_stable) {
    D9_GET(d);
    UINT a = IDirect3D9_GetAdapterCount(d);
    for (int i = 0; i < 10; i++) EXPECT_EQ(IDirect3D9_GetAdapterCount(d), a);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_adapter_count_plain_vs_ex) {
    D9_GET(a); D9_GET_EX(b);
    EXPECT_EQ(IDirect3D9_GetAdapterCount(a), IDirect3D9Ex_GetAdapterCount(b));
    IDirect3D9_Release(a); IDirect3D9Ex_Release(b);
}
TEST(d3d9_core_enum_adapter_count_two_objects_agree) {
    D9_GET(a); D9_GET(b);
    EXPECT_EQ(IDirect3D9_GetAdapterCount(a), IDirect3D9_GetAdapterCount(b));
    IDirect3D9_Release(a); IDirect3D9_Release(b);
}

/* ---- GetAdapterIdentifier ---- */
TEST(d3d9_core_enum_identifier_bad_adapter) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    UINT bad[] = { n, n + 1, n + 100, 12, 0x7FFFFFFF, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        D3DADAPTER_IDENTIFIER9 id; memset(&id, 0xCC, sizeof id);
        HRESULT hr = IDirect3D9_GetAdapterIdentifier(d, bad[i], 0, &id);
        CHECKF(hr == D9_INVALIDCALL, "adapter %u -> %s", bad[i], d3dh_hr_name(hr));
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_identifier_bad_flags) {
    D9_GET(d);
    UINT flags[] = { 1, 8, 0x10, 0x100, 0x80000000u, 0xFFFFFFFFu, 3, 5, 7 };
    for (unsigned i = 0; i < sizeof flags / sizeof *flags; i++) {
        D3DADAPTER_IDENTIFIER9 id;
        HRESULT hr = IDirect3D9_GetAdapterIdentifier(d, 0, flags[i], &id);
        CHECKF(hr == D9_INVALIDCALL, "flags 0x%x -> %s", flags[i], d3dh_hr_name(hr));
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_identifier_flags_checked_before_adapter) {
    /* Bad flags are reported even for a bad adapter: both are INVALIDCALL. */
    D9_GET(d);
    D3DADAPTER_IDENTIFIER9 id;
    EXPECT_HR(IDirect3D9_GetAdapterIdentifier(d, 0xFFFFFFFF, 1, &id), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_identifier_valid_flags) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    UINT flags[] = { 0, D3DENUM_WHQL_LEVEL, 4, D3DENUM_WHQL_LEVEL | 4 };
    for (unsigned i = 0; i < 4; i++) {
        D3DADAPTER_IDENTIFIER9 id; memset(&id, 0, sizeof id);
        HRESULT hr = IDirect3D9_GetAdapterIdentifier(d, 0, flags[i], &id);
        CHECKF(hr == S_OK, "flags 0x%x -> %s", flags[i], d3dh_hr_name(hr));
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_identifier_strings_terminated) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    if (!n) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (UINT a = 0; a < n; a++) {
        D3DADAPTER_IDENTIFIER9 id; memset(&id, 0xCC, sizeof id);
        if (FAILED(IDirect3D9_GetAdapterIdentifier(d, a, 0, &id))) { CHECKF(0, "adapter %u failed", a); continue; }
        CHECKF(memchr(id.Driver, 0, sizeof id.Driver) != NULL, "Driver unterminated");
        CHECKF(memchr(id.Description, 0, sizeof id.Description) != NULL, "Description unterminated");
        CHECKF(memchr(id.DeviceName, 0, sizeof id.DeviceName) != NULL, "DeviceName unterminated");
        t_note("adapter %u: driver='%s' desc='%s' dev='%s' vid=%04x did=%04x sub=%08x rev=%u whql=%lu drvver=%08lx.%08lx", a,
               id.Driver, id.Description, id.DeviceName, id.VendorId, id.DeviceId, id.SubSysId, id.Revision, id.WHQLLevel,
               (unsigned long)id.DriverVersion.HighPart, (unsigned long)id.DriverVersion.LowPart);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_identifier_device_name_format) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    if (!n) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (UINT a = 0; a < n; a++) {
        D3DADAPTER_IDENTIFIER9 id; memset(&id, 0, sizeof id);
        if (FAILED(IDirect3D9_GetAdapterIdentifier(d, a, 0, &id))) continue;
        CHECKF(!strncmp(id.DeviceName, "\\\\.\\DISPLAY", 11), "adapter %u DeviceName '%s'", a, id.DeviceName);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_identifier_device_names_unique) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    if (n < 2) { IDirect3D9_Release(d); t_skip("fewer than two adapters"); return; }
    for (UINT a = 0; a < n; a++) for (UINT b = a + 1; b < n; b++) {
        D3DADAPTER_IDENTIFIER9 x, y; memset(&x, 0, sizeof x); memset(&y, 0, sizeof y);
        IDirect3D9_GetAdapterIdentifier(d, a, 0, &x); IDirect3D9_GetAdapterIdentifier(d, b, 0, &y);
        CHECK(strcmp(x.DeviceName, y.DeviceName) != 0);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_identifier_repeatable) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DADAPTER_IDENTIFIER9 a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    EXPECT_HR(IDirect3D9_GetAdapterIdentifier(d, 0, 0, &a), S_OK);
    EXPECT_HR(IDirect3D9_GetAdapterIdentifier(d, 0, 0, &b), S_OK);
    EXPECT_STR(a.Driver, b.Driver);
    EXPECT_STR(a.Description, b.Description);
    EXPECT_STR(a.DeviceName, b.DeviceName);
    EXPECT_EQ(a.VendorId, b.VendorId);
    EXPECT_EQ(a.DeviceId, b.DeviceId);
    CHECK(!memcmp(&a.DeviceIdentifier, &b.DeviceIdentifier, sizeof(GUID)));
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_identifier_plain_vs_ex) {
    D9_GET(a); D9_GET_EX(b);
    if (!IDirect3D9_GetAdapterCount(a)) { IDirect3D9_Release(a); IDirect3D9Ex_Release(b); t_skip("no adapters"); return; }
    D3DADAPTER_IDENTIFIER9 x, y; memset(&x, 0, sizeof x); memset(&y, 0, sizeof y);
    IDirect3D9_GetAdapterIdentifier(a, 0, 0, &x);
    IDirect3D9Ex_GetAdapterIdentifier(b, 0, 0, &y);
    EXPECT_STR(x.Description, y.Description);
    EXPECT_STR(x.DeviceName, y.DeviceName);
    EXPECT_EQ(x.VendorId, y.VendorId);
    EXPECT_EQ(x.DeviceId, y.DeviceId);
    IDirect3D9_Release(a); IDirect3D9Ex_Release(b);
}
TEST(d3d9_core_enum_identifier_nonzero_description_note) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DADAPTER_IDENTIFIER9 id; memset(&id, 0, sizeof id);
    IDirect3D9_GetAdapterIdentifier(d, 0, 0, &id);
    t_note("description length %zu, driver '%s', identifier %s", strlen(id.Description), id.Driver, d3dh_guid_str(&id.DeviceIdentifier));
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_identifier_no_driverversion_flag_note) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DADAPTER_IDENTIFIER9 a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    IDirect3D9_GetAdapterIdentifier(d, 0, 0, &a);
    IDirect3D9_GetAdapterIdentifier(d, 0, 4, &b);
    t_note("DriverVersion default %08lx.%08lx, with flag 4: %08lx.%08lx",
           (unsigned long)a.DriverVersion.HighPart, (unsigned long)a.DriverVersion.LowPart,
           (unsigned long)b.DriverVersion.HighPart, (unsigned long)b.DriverVersion.LowPart);
    IDirect3D9_Release(d);
}

/* ---- GetAdapterDisplayMode ---- */
TEST(d3d9_core_enum_displaymode_bad_adapter) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    UINT bad[] = { n, n + 1, 0xFFFFFFFF };
    for (unsigned i = 0; i < 3; i++) {
        D3DDISPLAYMODE m; memset(&m, 0xAB, sizeof m);
        HRESULT hr = IDirect3D9_GetAdapterDisplayMode(d, bad[i], &m);
        CHECKF(hr == D9_INVALIDCALL, "adapter %u -> %s", bad[i], d3dh_hr_name(hr));
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_displaymode_current) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    if (!n) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (UINT a = 0; a < n; a++) {
        D3DDISPLAYMODE m; memset(&m, 0, sizeof m);
        HRESULT hr = IDirect3D9_GetAdapterDisplayMode(d, a, &m);
        if (hr == D9_NOTAVAILABLE) { t_note("adapter %u has no monitor attached", a); continue; }
        CHECKF(hr == S_OK, "adapter %u -> %s", a, d3dh_hr_name(hr));
        if (FAILED(hr)) continue;
        CHECK(m.Width > 0 && m.Height > 0);
        CHECKF(is_display_fmt(m.Format), "format %u", m.Format);
        t_note("adapter %u current mode %ux%u @%u fmt %u", a, m.Width, m.Height, m.RefreshRate, m.Format);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_displaymode_primary_matches_desktop_note) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DDISPLAYMODE m; memset(&m, 0, sizeof m);
    if (SUCCEEDED(IDirect3D9_GetAdapterDisplayMode(d, 0, &m))) {
        int cx = GetSystemMetrics(SM_CXSCREEN), cy = GetSystemMetrics(SM_CYSCREEN);
        t_note("adapter 0 %ux%u vs primary screen %dx%d (DPI virtualisation can differ)", m.Width, m.Height, cx, cy);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_displaymode_repeatable) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DDISPLAYMODE a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    if (FAILED(IDirect3D9_GetAdapterDisplayMode(d, 0, &a))) { IDirect3D9_Release(d); t_skip("no mode"); return; }
    IDirect3D9_GetAdapterDisplayMode(d, 0, &b);
    EXPECT_EQ(a.Width, b.Width); EXPECT_EQ(a.Height, b.Height); EXPECT_EQ(a.Format, b.Format); EXPECT_EQ(a.RefreshRate, b.RefreshRate);
    IDirect3D9_Release(d);
}

/* ---- GetAdapterModeCount / EnumAdapterModes ---- */
TEST(d3d9_core_enum_modecount_bad_adapter) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    UINT bad[] = { n, n + 1, 0xFFFFFFFF };
    for (unsigned i = 0; i < 3; i++)
        for (unsigned f = 0; f < NMODEFMTS; f++) {
            UINT c = IDirect3D9_GetAdapterModeCount(d, bad[i], k_modefmts[f]);
            CHECKF(c == 0, "adapter %u fmt %u -> %u", bad[i], k_modefmts[f], c);
        }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_modecount_nondisplay_formats_zero) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    for (UINT a = 0; a < n; a++)
        for (unsigned f = 0; f < NNONMODEFMTS; f++) {
            UINT c = IDirect3D9_GetAdapterModeCount(d, a, k_nonmodefmts[f]);
            CHECKF(c == 0, "adapter %u fmt 0x%x -> %u", a, k_nonmodefmts[f], c);
        }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_modecount_table) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    if (!n) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (unsigned f = 0; f < NMODEFMTS; f++) {
        UINT c = IDirect3D9_GetAdapterModeCount(d, 0, k_modefmts[f]);
        t_note("mode count fmt %u: %u", k_modefmts[f], c);
        CHECK(c < 5000);
        UINT c2 = IDirect3D9_GetAdapterModeCount(d, 0, k_modefmts[f]);
        EXPECT_EQ(c, c2);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_modecount_x8r8g8b8_nonzero_note) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    UINT c = IDirect3D9_GetAdapterModeCount(d, 0, D3DFMT_X8R8G8B8);
    if (!c) t_note("X8R8G8B8 mode count is zero (headless / no monitor)");
    else CHECK(c >= 1);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_modes_fields_valid) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (unsigned f = 0; f < NMODEFMTS; f++) {
        UINT c = IDirect3D9_GetAdapterModeCount(d, 0, k_modefmts[f]);
        if (c > 400) c = 400;
        for (UINT i = 0; i < c; i++) {
            D3DDISPLAYMODE m; memset(&m, 0xEE, sizeof m);
            HRESULT hr = IDirect3D9_EnumAdapterModes(d, 0, k_modefmts[f], i, &m);
            CHECKF(hr == S_OK, "fmt %u idx %u -> %s", k_modefmts[f], i, d3dh_hr_name(hr));
            if (FAILED(hr)) break;
            CHECKF(m.Format == k_modefmts[f], "fmt %u idx %u returned fmt %u", k_modefmts[f], i, m.Format);
            CHECKF(m.Width >= 1 && m.Height >= 1, "idx %u %ux%u", i, m.Width, m.Height);
            CHECKF(m.RefreshRate < 1000, "idx %u refresh %u", i, m.RefreshRate);
        }
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_modes_index_out_of_range) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (unsigned f = 0; f < NMODEFMTS; f++) {
        UINT c = IDirect3D9_GetAdapterModeCount(d, 0, k_modefmts[f]);
        UINT idx[] = { c, c + 1, c + 1000, 0xFFFFFFFF };
        for (unsigned i = 0; i < 4; i++) {
            D3DDISPLAYMODE m; memset(&m, 0, sizeof m);
            HRESULT hr = IDirect3D9_EnumAdapterModes(d, 0, k_modefmts[f], idx[i], &m);
            CHECKF(hr == D9_INVALIDCALL, "fmt %u count %u idx %u -> %s", k_modefmts[f], c, idx[i], d3dh_hr_name(hr));
        }
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_modes_nondisplay_format_invalidcall) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (unsigned f = 0; f < NNONMODEFMTS; f++) {
        D3DDISPLAYMODE m; memset(&m, 0, sizeof m);
        HRESULT hr = IDirect3D9_EnumAdapterModes(d, 0, k_nonmodefmts[f], 0, &m);
        CHECKF(hr == D9_INVALIDCALL, "fmt 0x%x -> %s", k_nonmodefmts[f], d3dh_hr_name(hr));
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_modes_bad_adapter) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    UINT bad[] = { n, n + 1, 0xFFFFFFFF };
    for (unsigned i = 0; i < 3; i++) {
        D3DDISPLAYMODE m; memset(&m, 0, sizeof m);
        HRESULT hr = IDirect3D9_EnumAdapterModes(d, bad[i], D3DFMT_X8R8G8B8, 0, &m);
        CHECKF(hr == D9_INVALIDCALL, "adapter %u -> %s", bad[i], d3dh_hr_name(hr));
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_modes_unique_entries) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    if (t_is_wine()) { IDirect3D9_Release(d); t_skip("emulator"); return; }
    UINT c = IDirect3D9_GetAdapterModeCount(d, 0, D3DFMT_X8R8G8B8);
    if (c > 300) c = 300;
    D3DDISPLAYMODE *ms = calloc(c ? c : 1, sizeof *ms);
    for (UINT i = 0; i < c; i++) IDirect3D9_EnumAdapterModes(d, 0, D3DFMT_X8R8G8B8, i, &ms[i]);
    for (UINT i = 0; i < c; i++) for (UINT j = i + 1; j < c; j++)
        CHECKF(!(ms[i].Width == ms[j].Width && ms[i].Height == ms[j].Height && ms[i].RefreshRate == ms[j].RefreshRate),
               "duplicate mode %ux%u@%u at %u and %u", ms[i].Width, ms[i].Height, ms[i].RefreshRate, i, j);
    free(ms);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_modes_contains_current_note) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DDISPLAYMODE cur; memset(&cur, 0, sizeof cur);
    if (FAILED(IDirect3D9_GetAdapterDisplayMode(d, 0, &cur))) { IDirect3D9_Release(d); t_skip("no current mode"); return; }
    UINT c = IDirect3D9_GetAdapterModeCount(d, 0, cur.Format);
    int found = 0;
    for (UINT i = 0; i < c && i < 1000; i++) {
        D3DDISPLAYMODE m; memset(&m, 0, sizeof m);
        if (SUCCEEDED(IDirect3D9_EnumAdapterModes(d, 0, cur.Format, i, &m)) && m.Width == cur.Width && m.Height == cur.Height) found = 1;
    }
    t_note("current mode %ux%u listed in its own format table: %d", cur.Width, cur.Height, found);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_modes_plain_vs_ex_count) {
    D9_GET(a); D9_GET_EX(b);
    if (!IDirect3D9_GetAdapterCount(a)) { IDirect3D9_Release(a); IDirect3D9Ex_Release(b); t_skip("no adapters"); return; }
    UINT ca = IDirect3D9_GetAdapterModeCount(a, 0, D3DFMT_X8R8G8B8);
    UINT cb = IDirect3D9Ex_GetAdapterModeCount(b, 0, D3DFMT_X8R8G8B8);
    t_note("mode count plain %u / Ex %u", ca, cb);
    CHECK(cb >= ca);
    IDirect3D9_Release(a); IDirect3D9Ex_Release(b);
}

/* ---- GetAdapterMonitor ---- */
TEST(d3d9_core_enum_monitor_bad_adapter) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    UINT bad[] = { n, n + 1, 0xFFFFFFFF };
    for (unsigned i = 0; i < 3; i++) CHECKF(IDirect3D9_GetAdapterMonitor(d, bad[i]) == NULL, "adapter %u", bad[i]);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_monitor_valid_handles) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    if (!n) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (UINT a = 0; a < n; a++) {
        HMONITOR m = IDirect3D9_GetAdapterMonitor(d, a);
        if (!m) { t_note("adapter %u: NULL monitor", a); continue; }
        MONITORINFOEXA mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
        CHECKF(GetMonitorInfoA(m, (MONITORINFO *)&mi), "adapter %u monitor %p", a, (void *)m);
        D3DADAPTER_IDENTIFIER9 id; memset(&id, 0, sizeof id);
        IDirect3D9_GetAdapterIdentifier(d, a, 0, &id);
        t_note("adapter %u monitor device '%s' vs adapter device '%s'", a, mi.szDevice, id.DeviceName);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_monitor_stable) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    HMONITOR a = IDirect3D9_GetAdapterMonitor(d, 0), b = IDirect3D9_GetAdapterMonitor(d, 0);
    CHECK(a == b);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_monitor_primary_is_adapter0_note) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    HMONITOR m = IDirect3D9_GetAdapterMonitor(d, 0);
    POINT pt = { 0, 0 };
    HMONITOR p = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    t_note("adapter0 monitor %p primary %p", (void *)m, (void *)p);
    IDirect3D9_Release(d);
}

/* ---- RegisterSoftwareDevice ---- */
TEST(d3d9_core_enum_register_software_device_null) {
    D9_GET(d);
    EXPECT_HR(IDirect3D9_RegisterSoftwareDevice(d, NULL), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_RegisterSoftwareDevice(d, NULL), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_enum_register_software_device_null_ex) {
    D9_GET_EX(d);
    EXPECT_HR(IDirect3D9Ex_RegisterSoftwareDevice(d, NULL), D9_INVALIDCALL);
    IDirect3D9Ex_Release(d);
}

/* ---- IDirect3D9Ex enumeration ---- */
static void mkfilter(D3DDISPLAYMODEFILTER *f, D3DFORMAT fmt, D3DSCANLINEORDERING so) { f->Size = sizeof *f; f->Format = fmt; f->ScanLineOrdering = so; }
TEST(d3d9_core_enum_filter_struct_size) { EXPECT_EQ(sizeof(D3DDISPLAYMODEFILTER), 12); EXPECT_EQ(sizeof(D3DDISPLAYMODEEX), 24); }
TEST(d3d9_core_enum_ex_modecount_invalid_filters) {
    D9_GET_EX(d);
    EXPECT_EQ(IDirect3D9Ex_GetAdapterModeCountEx(d, 0, NULL), 0);
    D3DDISPLAYMODEFILTER f;
    mkfilter(&f, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_UNKNOWN); f.Size = 0;  EXPECT_EQ(IDirect3D9Ex_GetAdapterModeCountEx(d, 0, &f), 0);
    mkfilter(&f, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_UNKNOWN); f.Size = 8;  EXPECT_EQ(IDirect3D9Ex_GetAdapterModeCountEx(d, 0, &f), 0);
    mkfilter(&f, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_UNKNOWN); f.Size = 16; EXPECT_EQ(IDirect3D9Ex_GetAdapterModeCountEx(d, 0, &f), 0);
    mkfilter(&f, D3DFMT_UNKNOWN,  D3DSCANLINEORDERING_UNKNOWN);              EXPECT_EQ(IDirect3D9Ex_GetAdapterModeCountEx(d, 0, &f), 0);
    mkfilter(&f, D3DFMT_X8R8G8B8, (D3DSCANLINEORDERING)3);                   EXPECT_EQ(IDirect3D9Ex_GetAdapterModeCountEx(d, 0, &f), 0);
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_enum_ex_modecount_bad_adapter) {
    D9_GET_EX(d);
    UINT n = IDirect3D9Ex_GetAdapterCount(d);
    D3DDISPLAYMODEFILTER f; mkfilter(&f, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_UNKNOWN);
    EXPECT_EQ(IDirect3D9Ex_GetAdapterModeCountEx(d, n, &f), 0);
    EXPECT_EQ(IDirect3D9Ex_GetAdapterModeCountEx(d, 0xFFFFFFFF, &f), 0);
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_enum_ex_modecount_nondisplay_zero) {
    D9_GET_EX(d);
    UINT n = IDirect3D9Ex_GetAdapterCount(d);
    for (UINT a = 0; a < n; a++)
        for (unsigned i = 1; i < NNONMODEFMTS; i++) {
            D3DDISPLAYMODEFILTER f; mkfilter(&f, k_nonmodefmts[i], D3DSCANLINEORDERING_UNKNOWN);
            UINT c = IDirect3D9Ex_GetAdapterModeCountEx(d, a, &f);
            CHECKF(c == 0, "fmt 0x%x -> %u", k_nonmodefmts[i], c);
        }
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_enum_ex_modecount_scanline_partition) {
    D9_GET_EX(d);
    if (!IDirect3D9Ex_GetAdapterCount(d)) { IDirect3D9Ex_Release(d); t_skip("no adapters"); return; }
    D3DDISPLAYMODEFILTER f;
    mkfilter(&f, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_UNKNOWN);     UINT all = IDirect3D9Ex_GetAdapterModeCountEx(d, 0, &f);
    mkfilter(&f, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_PROGRESSIVE); UINT prog = IDirect3D9Ex_GetAdapterModeCountEx(d, 0, &f);
    mkfilter(&f, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_INTERLACED);  UINT inter = IDirect3D9Ex_GetAdapterModeCountEx(d, 0, &f);
    t_note("Ex mode counts: any %u progressive %u interlaced %u", all, prog, inter);
    CHECK(prog <= all); CHECK(inter <= all);
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_enum_ex_enum_invalid_filters) {
    D9_GET_EX(d);
    D3DDISPLAYMODEEX m; memset(&m, 0, sizeof m); m.Size = sizeof m;
    EXPECT_HR(IDirect3D9Ex_EnumAdapterModesEx(d, 0, NULL, 0, &m), D9_INVALIDCALL);
    D3DDISPLAYMODEFILTER f;
    mkfilter(&f, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_UNKNOWN); f.Size = 0;
    EXPECT_HR(IDirect3D9Ex_EnumAdapterModesEx(d, 0, &f, 0, &m), D9_INVALIDCALL);
    mkfilter(&f, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_UNKNOWN); f.Size = 24;
    EXPECT_HR(IDirect3D9Ex_EnumAdapterModesEx(d, 0, &f, 0, &m), D9_INVALIDCALL);
    mkfilter(&f, D3DFMT_UNKNOWN, D3DSCANLINEORDERING_UNKNOWN);
    EXPECT_HR(IDirect3D9Ex_EnumAdapterModesEx(d, 0, &f, 0, &m), D9_INVALIDCALL);
    mkfilter(&f, D3DFMT_X8R8G8B8, (D3DSCANLINEORDERING)3);
    EXPECT_HR(IDirect3D9Ex_EnumAdapterModesEx(d, 0, &f, 0, &m), D9_INVALIDCALL);
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_enum_ex_enum_bad_adapter_and_index) {
    D9_GET_EX(d);
    UINT n = IDirect3D9Ex_GetAdapterCount(d);
    D3DDISPLAYMODEFILTER f; mkfilter(&f, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_UNKNOWN);
    D3DDISPLAYMODEEX m; memset(&m, 0, sizeof m); m.Size = sizeof m;
    EXPECT_HR(IDirect3D9Ex_EnumAdapterModesEx(d, n, &f, 0, &m), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9Ex_EnumAdapterModesEx(d, 0xFFFFFFFF, &f, 0, &m), D9_INVALIDCALL);
    if (n) {
        UINT c = IDirect3D9Ex_GetAdapterModeCountEx(d, 0, &f);
        EXPECT_HR(IDirect3D9Ex_EnumAdapterModesEx(d, 0, &f, c, &m), D9_INVALIDCALL);
        EXPECT_HR(IDirect3D9Ex_EnumAdapterModesEx(d, 0, &f, 0xFFFFFFFF, &m), D9_INVALIDCALL);
    }
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_enum_ex_enum_nondisplay_format) {
    D9_GET_EX(d);
    if (!IDirect3D9Ex_GetAdapterCount(d)) { IDirect3D9Ex_Release(d); t_skip("no adapters"); return; }
    for (unsigned i = 1; i < NNONMODEFMTS; i++) {
        D3DDISPLAYMODEFILTER f; mkfilter(&f, k_nonmodefmts[i], D3DSCANLINEORDERING_UNKNOWN);
        D3DDISPLAYMODEEX m; memset(&m, 0, sizeof m); m.Size = sizeof m;
        HRESULT hr = IDirect3D9Ex_EnumAdapterModesEx(d, 0, &f, 0, &m);
        CHECKF(hr == D9_INVALIDCALL, "fmt 0x%x -> %s", k_nonmodefmts[i], d3dh_hr_name(hr));
    }
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_enum_ex_enum_fields) {
    D9_GET_EX(d);
    if (!IDirect3D9Ex_GetAdapterCount(d)) { IDirect3D9Ex_Release(d); t_skip("no adapters"); return; }
    D3DDISPLAYMODEFILTER f; mkfilter(&f, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_UNKNOWN);
    UINT c = IDirect3D9Ex_GetAdapterModeCountEx(d, 0, &f);
    if (c > 300) c = 300;
    for (UINT i = 0; i < c; i++) {
        D3DDISPLAYMODEEX m; memset(&m, 0xEE, sizeof m); m.Size = sizeof m;
        HRESULT hr = IDirect3D9Ex_EnumAdapterModesEx(d, 0, &f, i, &m);
        CHECKF(hr == S_OK, "idx %u -> %s", i, d3dh_hr_name(hr));
        if (FAILED(hr)) break;
        CHECK(m.Size == sizeof m);
        CHECKF(m.Format == D3DFMT_X8R8G8B8, "idx %u fmt %u", i, m.Format);
        CHECK(m.Width > 0 && m.Height > 0);
        CHECKF((UINT)m.ScanLineOrdering <= D3DSCANLINEORDERING_INTERLACED, "idx %u scanline %u", i, m.ScanLineOrdering);
    }
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_enum_ex_enum_progressive_filter) {
    D9_GET_EX(d);
    if (!IDirect3D9Ex_GetAdapterCount(d)) { IDirect3D9Ex_Release(d); t_skip("no adapters"); return; }
    D3DDISPLAYMODEFILTER f; mkfilter(&f, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_PROGRESSIVE);
    UINT c = IDirect3D9Ex_GetAdapterModeCountEx(d, 0, &f);
    if (c > 300) c = 300;
    for (UINT i = 0; i < c; i++) {
        D3DDISPLAYMODEEX m; memset(&m, 0, sizeof m); m.Size = sizeof m;
        if (FAILED(IDirect3D9Ex_EnumAdapterModesEx(d, 0, &f, i, &m))) { CHECKF(0, "idx %u failed", i); break; }
        CHECKF(m.ScanLineOrdering == D3DSCANLINEORDERING_PROGRESSIVE, "idx %u scanline %u", i, m.ScanLineOrdering);
    }
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_enum_ex_displaymode_bad_args) {
    D9_GET_EX(d);
    UINT n = IDirect3D9Ex_GetAdapterCount(d);
    D3DDISPLAYMODEEX m; D3DDISPLAYROTATION rot = 0;
    memset(&m, 0, sizeof m); m.Size = sizeof m;
    EXPECT_HR(IDirect3D9Ex_GetAdapterDisplayModeEx(d, n, &m, &rot), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9Ex_GetAdapterDisplayModeEx(d, 0xFFFFFFFF, &m, &rot), D9_INVALIDCALL);
    if (n) {
        memset(&m, 0, sizeof m); m.Size = 0;
        EXPECT_HR(IDirect3D9Ex_GetAdapterDisplayModeEx(d, 0, &m, &rot), D9_INVALIDCALL);
        m.Size = 12;
        EXPECT_HR(IDirect3D9Ex_GetAdapterDisplayModeEx(d, 0, &m, &rot), D9_INVALIDCALL);
        m.Size = 25;
        EXPECT_HR(IDirect3D9Ex_GetAdapterDisplayModeEx(d, 0, &m, &rot), D9_INVALIDCALL);
    }
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_enum_ex_displaymode_current) {
    D9_GET_EX(d);
    if (!IDirect3D9Ex_GetAdapterCount(d)) { IDirect3D9Ex_Release(d); t_skip("no adapters"); return; }
    D3DDISPLAYMODEEX m; D3DDISPLAYROTATION rot = (D3DDISPLAYROTATION)0xFF;
    memset(&m, 0, sizeof m); m.Size = sizeof m;
    HRESULT hr = IDirect3D9Ex_GetAdapterDisplayModeEx(d, 0, &m, &rot);
    if (hr == D9_NOTAVAILABLE) { IDirect3D9Ex_Release(d); t_skip("no monitor"); return; }
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) {
        CHECK(m.Size == sizeof m);
        CHECK(m.Width > 0 && m.Height > 0);
        CHECK(is_display_fmt(m.Format));
        CHECKF(rot >= D3DDISPLAYROTATION_IDENTITY && rot <= D3DDISPLAYROTATION_270, "rotation %u", rot);
        t_note("Ex current mode %ux%u@%u fmt %u scanline %u rotation %u", m.Width, m.Height, m.RefreshRate, m.Format, m.ScanLineOrdering, rot);
    }
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_enum_ex_displaymode_matches_plain) {
    D9_GET_EX(d);
    if (!IDirect3D9Ex_GetAdapterCount(d)) { IDirect3D9Ex_Release(d); t_skip("no adapters"); return; }
    D3DDISPLAYMODEEX x; D3DDISPLAYMODE p; D3DDISPLAYROTATION rot;
    memset(&x, 0, sizeof x); x.Size = sizeof x; memset(&p, 0, sizeof p);
    if (FAILED(IDirect3D9Ex_GetAdapterDisplayModeEx(d, 0, &x, &rot)) || FAILED(IDirect3D9Ex_GetAdapterDisplayMode(d, 0, &p))) {
        IDirect3D9Ex_Release(d); t_skip("no mode"); return;
    }
    EXPECT_EQ(x.Width, p.Width); EXPECT_EQ(x.Height, p.Height); EXPECT_EQ(x.Format, p.Format);
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_enum_ex_luid_bad_adapter) {
    D9_GET_EX(d);
    UINT n = IDirect3D9Ex_GetAdapterCount(d);
    LUID l;
    EXPECT_HR(IDirect3D9Ex_GetAdapterLUID(d, n, &l), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9Ex_GetAdapterLUID(d, 0xFFFFFFFF, &l), D9_INVALIDCALL);
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_enum_ex_luid_valid) {
    D9_GET_EX(d);
    UINT n = IDirect3D9Ex_GetAdapterCount(d);
    if (!n) { IDirect3D9Ex_Release(d); t_skip("no adapters"); return; }
    for (UINT a = 0; a < n; a++) {
        LUID l1, l2; memset(&l1, 0xAA, sizeof l1); memset(&l2, 0xBB, sizeof l2);
        EXPECT_HR(IDirect3D9Ex_GetAdapterLUID(d, a, &l1), S_OK);
        EXPECT_HR(IDirect3D9Ex_GetAdapterLUID(d, a, &l2), S_OK);
        CHECK(!memcmp(&l1, &l2, sizeof l1));
        t_note("adapter %u LUID %08lx:%08lx", a, (unsigned long)l1.HighPart, (unsigned long)l1.LowPart);
    }
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_enum_ex_luid_distinct_per_adapter_note) {
    D9_GET_EX(d);
    UINT n = IDirect3D9Ex_GetAdapterCount(d);
    if (n < 2) { IDirect3D9Ex_Release(d); t_skip("fewer than two adapters"); return; }
    LUID l0, l1;
    IDirect3D9Ex_GetAdapterLUID(d, 0, &l0); IDirect3D9Ex_GetAdapterLUID(d, 1, &l1);
    t_note("adapters 0/1 LUID equal (clone/multi-head on one GPU): %d", !memcmp(&l0, &l1, sizeof l0));
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_enum_ex_luid_matches_across_objects) {
    D9_GET_EX(a); D9_GET_EX(b);
    if (!IDirect3D9Ex_GetAdapterCount(a)) { IDirect3D9Ex_Release(a); IDirect3D9Ex_Release(b); t_skip("no adapters"); return; }
    LUID x, y;
    IDirect3D9Ex_GetAdapterLUID(a, 0, &x); IDirect3D9Ex_GetAdapterLUID(b, 0, &y);
    CHECK(!memcmp(&x, &y, sizeof x));
    IDirect3D9Ex_Release(a); IDirect3D9Ex_Release(b);
}
