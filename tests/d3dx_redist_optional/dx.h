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

/* d3dx9_43.dll does not export the header-inline helpers (MatrixIdentity, Vec3Length/Cross/Dot/...): use local equivalents */
static D3DXMATRIX *dx_matrix_identity(D3DXMATRIX *m) { memset(m, 0, sizeof *m); m->_11 = m->_22 = m->_33 = m->_44 = 1; return m; }
static float dx_vec3_length(const D3DXVECTOR3 *v) { return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z); }
static float dx_vec3_dot(const D3DXVECTOR3 *a, const D3DXVECTOR3 *b) { return a->x * b->x + a->y * b->y + a->z * b->z; }
static D3DXVECTOR3 *dx_vec3_cross(D3DXVECTOR3 *o, const D3DXVECTOR3 *a, const D3DXVECTOR3 *b) {
    D3DXVECTOR3 t = { a->y * b->z - a->z * b->y, a->z * b->x - a->x * b->z, a->x * b->y - a->y * b->x }; *o = t; return o; }
static const float DX_PI = 3.14159265358979f;
static D3DXMATRIX dx_ident(void) { D3DXMATRIX m; memset(&m, 0, sizeof m); m._11 = m._22 = m._33 = m._44 = 1; return m; }

/* ID3DXMesh / ID3DXEffect C call wrappers (the SDK header only provides some) */
#define ID3DXEffect_Begin(p, ...) ((p)->lpVtbl->Begin((p), ##__VA_ARGS__))
#define ID3DXEffect_BeginPass(p, ...) ((p)->lpVtbl->BeginPass((p), ##__VA_ARGS__))
#define ID3DXEffect_End(p, ...) ((p)->lpVtbl->End((p), ##__VA_ARGS__))
#define ID3DXEffect_EndPass(p, ...) ((p)->lpVtbl->EndPass((p), ##__VA_ARGS__))
#define ID3DXEffect_GetDesc(p, ...) ((p)->lpVtbl->GetDesc((p), ##__VA_ARGS__))
#define ID3DXEffect_GetFloat(p, ...) ((p)->lpVtbl->GetFloat((p), ##__VA_ARGS__))
#define ID3DXEffect_GetInt(p, ...) ((p)->lpVtbl->GetInt((p), ##__VA_ARGS__))
#define ID3DXEffect_GetParameterByName(p, ...) ((p)->lpVtbl->GetParameterByName((p), ##__VA_ARGS__))
#define ID3DXEffect_GetParameterDesc(p, ...) ((p)->lpVtbl->GetParameterDesc((p), ##__VA_ARGS__))
#define ID3DXEffect_GetTechnique(p, ...) ((p)->lpVtbl->GetTechnique((p), ##__VA_ARGS__))
#define ID3DXEffect_GetTechniqueByName(p, ...) ((p)->lpVtbl->GetTechniqueByName((p), ##__VA_ARGS__))
#define ID3DXEffect_GetTechniqueDesc(p, ...) ((p)->lpVtbl->GetTechniqueDesc((p), ##__VA_ARGS__))
#define ID3DXEffect_GetVector(p, ...) ((p)->lpVtbl->GetVector((p), ##__VA_ARGS__))
#define ID3DXEffect_Release(p, ...) ((p)->lpVtbl->Release((p), ##__VA_ARGS__))
#define ID3DXEffect_SetFloat(p, ...) ((p)->lpVtbl->SetFloat((p), ##__VA_ARGS__))
#define ID3DXEffect_SetInt(p, ...) ((p)->lpVtbl->SetInt((p), ##__VA_ARGS__))
#define ID3DXEffect_SetTechnique(p, ...) ((p)->lpVtbl->SetTechnique((p), ##__VA_ARGS__))
#define ID3DXEffect_SetVector(p, ...) ((p)->lpVtbl->SetVector((p), ##__VA_ARGS__))
#define ID3DXMesh_CloneMeshFVF(p, ...) ((p)->lpVtbl->CloneMeshFVF((p), ##__VA_ARGS__))
#define ID3DXMesh_GenerateAdjacency(p, ...) ((p)->lpVtbl->GenerateAdjacency((p), ##__VA_ARGS__))
#define ID3DXMesh_GetAttributeTable(p, ...) ((p)->lpVtbl->GetAttributeTable((p), ##__VA_ARGS__))
#define ID3DXMesh_GetDeclaration(p, ...) ((p)->lpVtbl->GetDeclaration((p), ##__VA_ARGS__))
#define ID3DXMesh_GetDevice(p, ...) ((p)->lpVtbl->GetDevice((p), ##__VA_ARGS__))
#define ID3DXMesh_GetFVF(p, ...) ((p)->lpVtbl->GetFVF((p), ##__VA_ARGS__))
#define ID3DXMesh_GetIndexBuffer(p, ...) ((p)->lpVtbl->GetIndexBuffer((p), ##__VA_ARGS__))
#define ID3DXMesh_GetNumBytesPerVertex(p, ...) ((p)->lpVtbl->GetNumBytesPerVertex((p), ##__VA_ARGS__))
#define ID3DXMesh_GetNumFaces(p, ...) ((p)->lpVtbl->GetNumFaces((p), ##__VA_ARGS__))
#define ID3DXMesh_GetNumVertices(p, ...) ((p)->lpVtbl->GetNumVertices((p), ##__VA_ARGS__))
#define ID3DXMesh_GetOptions(p, ...) ((p)->lpVtbl->GetOptions((p), ##__VA_ARGS__))
#define ID3DXMesh_LockAttributeBuffer(p, ...) ((p)->lpVtbl->LockAttributeBuffer((p), ##__VA_ARGS__))
#define ID3DXMesh_LockIndexBuffer(p, ...) ((p)->lpVtbl->LockIndexBuffer((p), ##__VA_ARGS__))
#define ID3DXMesh_LockVertexBuffer(p, ...) ((p)->lpVtbl->LockVertexBuffer((p), ##__VA_ARGS__))
#define ID3DXMesh_OptimizeInplace(p, ...) ((p)->lpVtbl->OptimizeInplace((p), ##__VA_ARGS__))
#define ID3DXMesh_Release(p, ...) ((p)->lpVtbl->Release((p), ##__VA_ARGS__))
#define ID3DXMesh_UnlockAttributeBuffer(p, ...) ((p)->lpVtbl->UnlockAttributeBuffer((p), ##__VA_ARGS__))
#define ID3DXMesh_UnlockIndexBuffer(p, ...) ((p)->lpVtbl->UnlockIndexBuffer((p), ##__VA_ARGS__))
#define ID3DXMesh_UnlockVertexBuffer(p, ...) ((p)->lpVtbl->UnlockVertexBuffer((p), ##__VA_ARGS__))
#endif
