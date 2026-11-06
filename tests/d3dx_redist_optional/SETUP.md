# Running the d3dx_redist_optional area for real

Windows 10 does not ship the D3DX / d3dcompiler_43 redistributables, so on a stock image every test in this area skips
("d3dx9_43.dll not installed", ...), and the 14 legacy `d3d8.dll` tests skip because d3d8.dll is not part of Windows 10 22H2.

To exercise the area, put the DLLs of the DirectX End-User Runtimes (June 2010) next to the test executable
(application-local deployment, no installation or admin rights needed):

1. Download the public redistributable `directx_Jun2010_redist.exe` from Microsoft (do not commit it or its contents).
2. Extract it: `directx_Jun2010_redist.exe /Q /T:C:\somewhere`.
3. Expand the x64 cabinets into the directory that holds `d3dx_redist_optional.exe`:
   `expand C:\somewhere\Jun2010_d3dx9_43_x64.cab -F:* .` and the same for `Jun2010_d3dx10_43_x64.cab`, `Jun2010_d3dx11_43_x64.cab`,
   `Jun2010_D3DCompiler_43_x64.cab`.

With the four DLLs present the area runs 331 tests (about 2500 checks) with only the d3d8 tests skipped. The redistributable
can equally be installed system wide (`DXSETUP.exe /silent`, needs administrator); both ways give identical results.
