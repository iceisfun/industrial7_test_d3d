# HANDOFF: Windows 10 pass of the d3d11 conformance corpus

## Purpose
The corpus asserts the behaviour of Windows 10 x64 `d3d11.dll` and its companion modules (d3d9, d3d10, d3d10_1, d3d10core, d3d10_1core, d3d10warp, d3d10level9, d3d11on12, d3d12, d3d12core, d3d9on12, dxgi, dxcore, d3dcompiler_47, d3dscache, d3d8thk, ddraw, dxilconv, dxva2) for someone implementing d3d11.dll. Every test loads the DLL, resolves exports with `GetProcAddress` and calls through the pointer; nothing is linked. Windows 10 results are the authority.

## Status
The whole corpus has been run on real Windows 10 22H2 (build 19045.x, "Microsoft Basic Display Adapter" with WARP, no GPU, 1024x768 virtual display, interactive desktop session) as a standard user and as an elevated administrator, with the Graphics Tools optional feature installed (debug layers present). Every expectation in the device-based areas has been measured there; wrong expectations were fixed and Windows-specific values are pinned. Per-area totals are in `COVERAGE.md`. Current state (final verification, 3 standard-user, 3 elevated and 2 destructive fresh-VM runs): 12459 tests (12516 with the destructive `dx_gap_*` areas), 0 failed, 415 skipped (measured, see below), every area reaches its SUMMARY, no timeouts or crashes, and the per-test outcomes are identical across all runs and modes. The two former WARP-divergence failures are pinned on the Microsoft adapter (see below).

## Prerequisites
- Disposable Windows 10 x64 VM (1809 to 22H2) with a snapshot. A basic display adapter with WARP is enough; a physical GPU is not needed. An interactive desktop session (console or RDP) is required for the areas that duplicate the desktop or create windows; no window is ever shown.
- Optional Windows features: "Graphics Tools" (debug layers; `dism /online /add-capability /capabilityname:Tools.Graphics.DirectX~~~~0.0.1.0`). Without it the debug-layer parts skip.
- A standard user account and an elevated administrator account (run both).
- Build host: Linux with `x86_64-w64-mingw32-gcc` (and `i686-w64-mingw32-gcc` for `ddraw_legacy_x86`), or a prebuilt `build/` copy (binaries are not committed).
- Optional: Python 3 on the VM for `tools\summarize.py`. Optional: the June 2010 D3DX redistributable DLLs placed next to the exes enable `d3dx_redist_optional` (see `tests/d3dx_redist_optional/SETUP.md`; never commit them).

## Build
    make clean && make -j8        # build/<area>.exe, static, no DLL under test linked

Copy `build\*.exe` and `tools\run_all.bat`, `tools\summarize.py` to the VM, keeping `build\` and `tools\` as sibling folders.

## Run (revert the snapshot between passes)
1. Standard user: `tools\run_all.bat`
2. Elevated admin: `tools\run_all.bat`
3. Optional, state-changing areas: `tools\run_all.bat /destructive` (`dx_gap_*`). These are dormant on purpose: monitor DDC/CI writes only happen with `set TP_ALLOW_MONITOR_WRITE=1`; leave it unset on VMs.
Single area or test: `build\<area>.exe <name-substring>`. Total runtime on WARP is a few minutes when areas run in parallel; the desktop, timing and device-exclusive areas (d3d9_*, dxgi_swapchain, dxgi_debug_ext, ddraw_legacy and ddraw_legacy_x86, dxva2_monitor_video, d3d12_*, d3d11_render_exec, d3d11_debug_refcount, cross_com_threads_stress) are best run one at a time. Each exe exits 1 if any test failed.
Collect `build\results\*.txt` (zip them) and run `python tools\summarize.py build\results --failures`.

## Triage failures
For each failing check decide:
1. Wrong expectation: Windows differs from the test. Re-probe the exact call on the VM (a small variant of the failing test, repeated at least 20 times if the result varies) and fix the test.
2. Environment/SKU/hardware dependence (WARP vs GPU, locale, privilege, missing feature): turn the check into `t_note` or `t_skip`. Values that WARP decides are pinned only when the device runs on the Basic Render Driver (vendor 0x1414) and logged as notes on other adapters.
3. Real divergence from the reference behaviour: record as a finding; this is what the corpus is for.
Fix, rebuild, re-run only that test, commit with a short message using specific `git add` paths.

## Known WARP divergences (pinned, no longer failing)
These two tests asserted the specified behaviour and failed on WARP. They now pin the measured Microsoft-adapter result (vendor 0x1414) and keep the specified result for every other adapter, so a real GPU still gets the specified check:
- `d3d11_render_exec_tess_quad_pow2_non_power_of_two_factor`: "pow2" partitioning does not round on WARP; quad factors 3, 5, 6, 7 give 18, 50, 72, 98 triangles (2*f*f, same as "integer"); specified are 32, 128, 128, 128.
- `d3d12_objects_dheap_copy_descriptors_spanning_ranges`: CopyDescriptors with destination ranges {1x3, 5x1} and source ranges {0x2, 4x2} restarts the offset inside the current source range at the second destination range (d5 gets s4, not s5); texture 4 reads white and texture 5 stays untouched.

## Expected skips (all measured on the reference VM)
- No video interfaces on the Basic Render Driver: `d3d11_interfaces_video` (72) and `d3d11_views` (5) video tests, `dxva2_monitor_video` (DXVA-HD device creation and decoder GUID enumeration, 10).
- 64-bit ddraw has no legacy Direct3D: 14 `ddraw_legacy` tests skip; the 32-bit build `ddraw_legacy_x86` runs them.
- Only one hardware-type adapter: 2 `d3d9_core` and 1 `d3d_on12_dxcore_misc` multi-adapter tests.
- Tests that need the debug layer to be ABSENT (`create_device_debug_flag_failure_nulls_outputs`, debug-layer-missing output clearing) skip when Graphics Tools is installed. On the plain image without the feature the same two tests fail as above, 93 further tests skip (debug-layer parts of d3d11_debug_refcount, d3d10_device, d3d12_cmdlist_ext, dxgi_debug_ext, d3d11_gapfill) and the debug-layer-missing contracts run: `D3D12GetDebugInterface` / `DXGIGetDebugInterface1` fail with DXGI_ERROR_SDK_COMPONENT_MISSING (0x887a002d) for unknown IIDs and `CreateDXGIFactory2(DEBUG)` fails with DXGI_ERROR_INVALID_CALL.
- D3D11 REFERENCE devices cannot be created without the SDK; `SwitchToRef` is not exposed.
- Missing optional redistributables (d3dx*): `d3dx_redist_optional` skips (306).
- Unelevated: any admin-only probes skip with a message; compare the two passes.

## Environment facts that matter
- Adapter 0 is the hardware-type "Microsoft Basic Render Driver" (1414:008c) that owns the output; adapter 1 is the WARP software adapter without outputs. `D3D11CreateDevice(HARDWARE)` works on adapter 0; helper devices prefer WARP.
- Output duplication works from the interactive session on adapter 0 (system-memory desktop image); a device on the other adapter is rejected with E_INVALIDARG.
- Several Windows teardown paths race (releasing a D3D11 device right after output-duplication objects, or after failed duplication attempts, intermittently raised an access violation). The affected tests pause before teardown or isolate the call in a child process.
- D3D12 runs on WARP12 with feature level 12_1; ray tracing, mesh shaders, video and enhanced barriers are not available there.

## Harness notes (what the final verification used)
- Each run is a fresh VM from the untouched image (copy-on-write overlay, discarded afterwards); never reuse a VM between passes. Repeat each mode at least 3 times and diff the per-test OK/FAIL/SKIP lines.
- Stage the exes on the VM's local disk, not on a network or virtual-FAT share (test I/O is far too slow there).
- Run independent areas in a parallel group (about 6 wide) and the desktop, timing and device-exclusive areas serially: d3d9_*, dxgi_swapchain, ddraw_legacy, cross_com_threads_stress, dxva2_monitor_video, d3d12_rootsig_cpu, d3d12_objects, d3d12_entry_points, d3d_on12_dxcore_misc, d3d11_render_exec, d3d11_debug_refcount. Run `dx_*` last and only in the destructive pass.
- Give every exe a hard limit (600 s). If an area ends without a SUMMARY line (crash or hang), `<area>.exe --list` prints the test names so each unfinished test can be re-run in its own process (`<area>.exe <name>`) with a short limit; none was needed in the final pass.
- Check the run environment first: session id, desktop (interactive session, 1024x768), adapter list (Basic Render Driver + WARP), and whether the Graphics Tools feature is present. The device areas run for real in this configuration: there is no "skip wall"; the remaining skips are the measured ones above.
- Adding the Graphics Tools feature needs Windows Update access on a throw-away overlay; keep the result as a separate base image so the plain image stays available for the "debug layer absent" expectations. Debug-layer-dependent HRESULT expectations accept both images.
- Test helpers evaluate macro arguments once (`EXPECT_EQ`, `EXPECT_NE`, `EXPECT_STR`); keep it that way, calls such as `EXPECT_EQ(Release(o), 0)` must not release twice.
- Windows raises access violations for some NULL/invalid-argument calls instead of returning an error code; such calls are wrapped (`t_try`) or isolated in a child process, never left to kill the area.

## Cleanup of leftovers
Scratch files live in `%TEMP%\d3dt_<pid>\` and are removed on exit. After crashes: `rmdir /s /q %TEMP%\d3dt_*` and `taskkill /f /im core_smoke.exe` (repeat for any stray `<area>.exe`, e.g. `for %f in (build\*.exe) do taskkill /f /im %~nxf`). Reverting the snapshot removes everything else.

## Report back
Per-area totals (tests, failed, skipped), the list of wrong expectations fixed, the list of real divergences with check lines and HRESULT/NTSTATUS values, areas that could not run and why, the Windows build number, GPU or WARP in use, and whether Graphics Tools and D3D12 were present.
