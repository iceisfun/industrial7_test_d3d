/* Shared declarations for the exports_modules area: per-module export tables (exports_data.c) and helpers. */
#ifndef XM_H
#define XM_H
#include "t.h"

typedef struct { const char *name; unsigned ord; unsigned rva; const char *fwd; } xexp;   /* name NULL: ordinal-only */
typedef struct {
    const char *dll;
    const xexp *e; int n;
    int subsystem;
    const char *sections;   /* comma list of section names of the reference image */
    const char *deps;       /* comma list of non-delay, non-apiset imported DLLs of the reference image */
    const char *ver;        /* reference FileVersion */
} xmod;

#define XM_COUNT 20
extern const xmod g_xm[XM_COUNT];

enum { XM_d3d11, XM_d3d9, XM_d3d10, XM_d3d10_1, XM_d3d10core, XM_d3d10_1core, XM_d3d10warp, XM_d3d10level9,
       XM_d3d11on12, XM_d3d12, XM_d3d12core, XM_d3d9on12, XM_dxgi, XM_dxcore, XM_d3dcompiler_47, XM_d3dscache,
       XM_d3d8thk, XM_ddraw, XM_dxilconv, XM_dxva2 };

/* One TEST per module: XM_FOR_ALL(prop, fn) defines exports_modules_<module>_<prop> calling fn(index). */
#define XM_DEFINE(i, m, prop, fn) TEST(exports_modules_##m##_##prop) { xm_begin(); fn(i); xm_end(); }
#define XM_FOR_ALL(prop, fn) \
    XM_DEFINE(0, d3d11, prop, fn) XM_DEFINE(1, d3d9, prop, fn) XM_DEFINE(2, d3d10, prop, fn) \
    XM_DEFINE(3, d3d10_1, prop, fn) XM_DEFINE(4, d3d10core, prop, fn) XM_DEFINE(5, d3d10_1core, prop, fn) \
    XM_DEFINE(6, d3d10warp, prop, fn) XM_DEFINE(7, d3d10level9, prop, fn) XM_DEFINE(8, d3d11on12, prop, fn) \
    XM_DEFINE(9, d3d12, prop, fn) XM_DEFINE(10, d3d12core, prop, fn) XM_DEFINE(11, d3d9on12, prop, fn) \
    XM_DEFINE(12, dxgi, prop, fn) XM_DEFINE(13, dxcore, prop, fn) XM_DEFINE(14, d3dcompiler_47, prop, fn) \
    XM_DEFINE(15, d3dscache, prop, fn) XM_DEFINE(16, d3d8thk, prop, fn) XM_DEFINE(17, ddraw, prop, fn) \
    XM_DEFINE(18, dxilconv, prop, fn) XM_DEFINE(19, dxva2, prop, fn)

/* XC: like CHECKF, but a failure under an emulator (incomplete DLL set) is only counted and summarised as a note. */
void xm_begin(void);
void xm_end(void);
void xm_check(int ok, const char *file, int line, const char *expr, const char *fmt, ...);
#define XC(c, ...) xm_check(!!(c), __FILE__, __LINE__, #c, __VA_ARGS__)

#define XCK(c) xm_check(!!(c), __FILE__, __LINE__, #c, NULL)
#define XERR(err) do { DWORD e_ = GetLastError(); xm_check(e_ == (DWORD)(err), __FILE__, __LINE__, #err, "GetLastError got %lu want %lu", e_, (DWORD)(err)); } while (0)

/* helpers (xm_util.c) */
HMODULE xm_load(int i);                          /* LoadLibraryA of the module, NULL if missing */
const IMAGE_NT_HEADERS64 *xm_nt(HMODULE m);      /* NULL if the image is malformed */
const IMAGE_EXPORT_DIRECTORY *xm_expdir(HMODULE m, DWORD *size);
const char *xm_name_of_ordinal(HMODULE m, unsigned ord); /* NULL if the ordinal has no name */
int  xm_in_dir(HMODULE m, const void *p);        /* p lies inside the export directory (forwarder string) */
int  xm_is_exec(HMODULE m, const void *p);       /* p lies in a section flagged executable */
int  xm_in_image(HMODULE m, const void *p);
int  xm_has_name(int mod, const char *name);
int  xm_wine(void);
#endif
