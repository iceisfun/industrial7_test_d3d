/* shared helpers for the optional redistributable D3DX / legacy area */
#ifndef DX_H
#define DX_H
#include "d3dh.h"
#include <math.h>
#include <d3dx9.h>
#include "dx_assets.h"

#define X9  "d3dx9_43.dll"
#define X10 "d3dx10_43.dll"
#define X11 "d3dx11_43.dll"

/* A function from a redistributable DLL (typed from the SDK header); skips the test if DLL or export is absent. */
#define NEED_DLL(dll) do { if (!LoadLibraryA(dll)) { t_skip(dll " not installed"); return; } } while (0)
#define F9(name)  UIN(X9, name)

static int dx_near(float a, float b, float eps) { return fabsf(a - b) <= eps; }
#define EPS 1e-5f
#define CHECKN(a, b) t_check(dx_near((a), (b), EPS), __FILE__, __LINE__, #a " ~= " #b, "got %.9g want %.9g", (double)(a), (double)(b))
#define CHECKNE(a, b, e) t_check(dx_near((a), (b), (e)), __FILE__, __LINE__, #a " ~= " #b, "got %.9g want %.9g", (double)(a), (double)(b))

static int dx_mat_near(const D3DXMATRIX *m, const float *w, float eps) {
    const float *f = (const float *)m;
    for (int i = 0; i < 16; i++) if (!dx_near(f[i], w[i], eps)) return 0;
    return 1;
}
static void dx_mat_dump(const char *what, const D3DXMATRIX *m) {
    const float *f = (const float *)m;
    t_note("%s: [%g %g %g %g | %g %g %g %g | %g %g %g %g | %g %g %g %g]", what, f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7],f[8],f[9],f[10],f[11],f[12],f[13],f[14],f[15]);
}
#define CHECK_MAT(m, ...) do { static const float w_[16] = { __VA_ARGS__ }; \
    int ok_ = dx_mat_near(&(m), w_, 1e-5f); t_check(ok_, __FILE__, __LINE__, "matrix " #m " == " #__VA_ARGS__, NULL); if (!ok_) dx_mat_dump(#m, &(m)); } while (0)

static const float DX_PI = 3.14159265358979f;
static D3DXMATRIX dx_ident(void) { D3DXMATRIX m; memset(&m, 0, sizeof m); m._11 = m._22 = m._33 = m._44 = 1; return m; }
#endif
