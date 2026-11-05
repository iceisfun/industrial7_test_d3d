# d3d11.dll conformance corpus

Black-box tests for Windows 10 x64 `d3d11.dll`. The DLL is never linked: each test loads it with `LoadLibrary`,
resolves entry points with `GetProcAddress` (`U(Name)` gives a pointer typed from the SDK header;
`UIN("module.dll", Name)` pins one module, which matters for forwarders) and calls through the pointer.

Modules searched by `U()`: d3d11.dll d3d9.dll d3d10.dll d3d10_1.dll d3d10core.dll d3d10_1core.dll d3d10warp.dll d3d10level9.dll d3d11on12.dll d3d12.dll d3d12core.dll d3d9on12.dll dxgi.dll dxcore.dll d3dcompiler_47.dll d3dscache.dll d3d8thk.dll ddraw.dll dxilconv.dll dxva2.dll ntdll.dll kernel32.dll.

    make                         # cross-compile build/<area>.exe (mingw-w64)
    build\<area>.exe [substr]    # on Windows; optional test-name filter
    tools\run_all.bat            # everything, results\<area>.txt (add /destructive for dx_* areas)
    python tools\summarize.py results [--failures]

Output: `RUN/OK/FAIL/SKIP` per test, `  FAIL file:line expr (detail)` per failed check, `SUMMARY` last;
exit code 1 if any test failed. `NOTE` lines record observed, version-dependent behaviour.

Tests that need a second process re-launch the exe (`--child <name>`). Scratch files live in
`%TEMP%\d3dt_<pid>\` and are removed on exit. `tools/*_exports.txt` list the named exports.

See `HANDOFF.md` (when present) for how to run and triage the Windows passes.

## Writing D3D tests

Each area is `tests/<area>/*.c` and builds to `build/<area>.exe`; `make list-areas` lists them. Every exe is compiled with all of
`common/*.c`, `-Icommon -Iassets`, and linked with `-ldxguid -luuid -lole32 -lgdi32 -luser32 -loleaut32` (IID/CLSID data only).
No D3D/DXGI/compiler/DDraw/DXVA DLL is ever linked: reach them through `LoadLibrary -> GetProcAddress -> call`.
Writers own only their `tests/<area>/`; `common/`, `assets/` and the Makefile are shared and not edited per area.

    #include "d3dh.h"            // also pulls t.h, SDK D3D/DXGI/D3DCompiler headers in C (COBJMACROS, CINTERFACE) form
    #include "shaders.h"         // SH_<NAME> HLSL source strings (entry point main)
    #include "meshes.h"          // mesh_{cube,quad,tetra}_{positions,normals,uvs,indices}, MESH_*_NVERTS/NINDICES
    #include "textures.h"        // tex_checker8, tex_gradient16, tex_ramp1d, tex_bc1_4x4, tex_bc1_8x8

### Conventions
- Test names are `<area dir name>_<what>` (`TEST(d3d11core_create_buffer)`), unique project-wide. Many small tests beat few big ones.
- Skip rules: a missing DLL/export or a device that cannot be created is `t_skip()` (never a failure). Version, driver, GPU or
  locale dependent behaviour is `t_note()`; hard `CHECK`/`EXPECT_*` only what is documented or verified on Windows 10.
- WARP first: helpers try WARP, then REFERENCE, then NULL, then HARDWARE (D3D9: NULLREF, HAL, REF). Do not depend on hardware.
- No window is ever shown (`d3dh_create_hidden_window` is WS_POPUP, never visible; under headless Wine it falls back to a
  message-only window). Destroy what you create; scratch files go in `t_tmpdir()`.
- Windows 10 is the authority; Wine (`tools/wine_run.sh build/<area>.exe`, headless, usually no devices) only validates that tests build, run to completion and skip cleanly.
- Do not mention external origins of test ideas anywhere (code, docs, commit messages): no third-party attribution.
- Prefer `d3dh_*` loaders and `EXPECT_HR(hr, want)` (prints both HRESULT names) over hand-rolled equivalents.
- Regenerate assets with `python3 tools/gen_assets.py` (edit `assets/shaders/*.hlsl`, never the generated `assets/*.h`).

### d3dh API (common/d3dh.h; each helper has a one-line comment there)
- Loading: `d3dh_getproc(dll,name)`, `d3dh_have_dll`, `D3DH_FN(dll,Name)`; typed loaders `d3dh_D3D11CreateDevice()`, `d3dh_CreateDXGIFactory{,1,2}()`,
  `d3dh_D3D12CreateDevice()`, `d3dh_D3D12SerializeRootSignature()`, `d3dh_Direct3DCreate9{,Ex}()`, `d3dh_D3D10CreateDevice{,1}()`, `d3dh_D3DCompile()`,
  `d3dh_D3DReflect()`, `d3dh_D3DCreateBlob()`, `d3dh_D3DDisassemble()`, `d3dh_DirectDrawCreate{,Ex}()` ... (NULL if absent: `REQUIRE(d3dh_X())`).
- Windows: `d3dh_create_hidden_window()`, `d3dh_create_hidden_window_sized(w,h)`, `d3dh_destroy_window(h)`.
- Devices (return HRESULT, caller skips on failure): `d3dh_make_device11[_fl]`, `d3dh_make_device10`, `d3dh_make_device10_1`, `d3dh_make_device12`
  (WARP via `IDXGIFactory4::EnumWarpAdapter`), `d3dh_make_device9`, `d3dh_make_device9ex`, `d3dh_make_factory1/2`.
- Compile: `d3dh_compile(src,entry,target,flags,&blob)`, `d3dh_compile_ex(...)`, `d3dh_compile_errors()`, `d3dh_blob_ptr/size`.
- Readback: `d3dh_readback_texture2d11/10/12`, `d3dh_readback_buffer11/10/12` (images are tightly packed `d3dh_image`; free with `d3dh_image_free`), `d3dh_d12_flush`,
  `d3dh_d12_resource_desc`, `d3dh_d12_heap_cpu_start/gpu_start` (C wrappers for struct-returning D3D12 methods).
- Misc: `SAFE_RELEASE(p)`, `d3dh_refcount(p)`, `D3DH_RELEASE_RC(p)`, `D3DH_QI(p,iid,&out)`, `d3dh_hr_name(hr)`, `EXPECT_HR/EXPECT_HR_OK/EXPECT_HR_FAILED`,
  `d3dh_guid_str`, `d3dh_rng_*` (splitmix64), `d3dh_fnv1a`, `d3dh_crc32`, `d3dh_image_hash`, `d3dh_image_pixel32`, `d3dh_format_bytes`.
