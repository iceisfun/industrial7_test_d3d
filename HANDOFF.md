# HANDOFF: Windows 10 pass of the d3d11 conformance corpus

## Purpose
The corpus asserts the behaviour of Windows 10 x64 `d3d11.dll` and its companion modules (d3d9, d3d10, d3d10_1, d3d10core, d3d10_1core, d3d10warp, d3d10level9, d3d11on12, d3d12, d3d12core, d3d9on12, dxgi, dxcore, d3dcompiler_47, d3dscache, d3d8thk, ddraw, dxilconv, dxva2) for someone implementing d3d11.dll. Every test loads the DLL, resolves exports with `GetProcAddress` and calls through the pointer; nothing is linked. Windows 10 results are the authority. So far the corpus was only validated headless under an emulator without a GPU (see `COVERAGE.md`: about 11.7k tests, roughly 9k skipped there because no device exists). Expect wrong expectations in the first real run, mostly in the device-based areas that never executed.

## Prerequisites
- Disposable Windows 10 x64 VM (1809 to 22H2) with a snapshot. A basic display adapter with WARP is enough; a physical GPU is not needed. Session 0, RDP or console are all fine; no window is ever shown.
- Optional Windows features: "Graphics Tools" (debug layers) and D3D12 support. Without them the affected tests skip.
- A standard user account and an elevated administrator account (run both).
- Build host: Linux with `x86_64-w64-mingw32-gcc`, or a prebuilt `build/` copy (binaries are not committed).
- Optional: Python 3 on the VM for `tools\summarize.py`.

## Build
    make clean && make -j8        # build/<area>.exe, static, no DLL under test linked

Copy `build\*.exe` and `tools\run_all.bat`, `tools\summarize.py` to the VM, keeping `build\` and `tools\` as sibling folders.

## Run (revert the snapshot between passes)
1. Standard user: `tools\run_all.bat`
2. Elevated admin: `tools\run_all.bat`
3. Optional, state-changing areas: `tools\run_all.bat /destructive` (`dx_gap_*`). These are dormant on purpose: monitor DDC/CI writes only happen with `set TP_ALLOW_MONITOR_WRITE=1`; leave it unset on VMs.
Single area or test: `build\<area>.exe <name-substring>`. Total runtime on WARP is expected to be a few minutes; `d3d12_rootsig_cpu` is the longest (about 100 s under the emulator). Each exe exits 1 if any test failed.
Collect `build\results\*.txt` (zip them) and run `python tools\summarize.py build\results --failures`.

## Triage failures
For each failing check decide:
1. Wrong expectation: Windows differs from the test. Check the decompilation (`http://docker1.dev.moonlightcompanies.com:8765/api/function/<module>/<Name>/decomp`) and fix the test.
2. Environment/SKU/hardware dependence (WARP vs GPU, locale, privilege, missing feature): turn the check into `t_note` or `t_skip`.
3. Real divergence from the reference behaviour: record as a finding; this is what the corpus is for.
Fix, rebuild, re-run only that test, commit with a short message using specific `git add` paths.

## Expected skips
- No D3D12 device or feature level: d3d12_objects, d3d12_entry_points (device parts), d3d_on12_dxcore_misc (11on12, 9on12).
- No debug layers: debug-layer parts of d3d11_debug_refcount and d3d10_device.
- No physical monitor or DDC/CI: dxva2_monitor_video live parts, dx_gap_02.
- Unelevated: any admin-only probes skip with a message; compare the two passes.
- Missing optional redistributables (d3dx*): d3dx_redist_optional skips.
- Session 0 / no desktop: tests needing a desktop window fall back to skip.

## Known weak spots
- Headless Wine failures that need a Windows verdict: d3d12_rootsig_cpu (many serializer/HLSL root-signature expectations), d3dcompiler_reflect_blob and d3d10_utils_effects (compiler/disassembler text, blob validation), ddraw_legacy, dxva2_monitor_video, gap_00, d3d11_device_create (module-level undocumented exports). Most are probably Wine gaps, but they are the highest-risk expectations.
- Device-based areas (d3d11_*, d3d10_device, d3d9_*, dxgi_*, d3d12_objects, hlsl_semantics_exec, cross_com_threads_stress) were almost entirely skipped during validation, so their hard checks have never run anywhere.
- Undocumented-signature exports (`D3D11CoreCreateDevice` etc.): argument layouts were inferred from the decompilation.

## Cleanup of leftovers
Scratch files live in `%TEMP%\d3dt_<pid>\` and are removed on exit. After crashes: `rmdir /s /q %TEMP%\d3dt_*` and `taskkill /f /im core_smoke.exe` (repeat for any stray `<area>.exe`, e.g. `for %f in (build\*.exe) do taskkill /f /im %~nxf`). Reverting the snapshot removes everything else.

## Report back
Per-area totals (tests, failed, skipped), the list of wrong expectations fixed, the list of real divergences with check lines and HRESULT/NTSTATUS values, areas that could not run and why, the Windows build number, GPU or WARP in use, and whether Graphics Tools and D3D12 were present.
