/* Shared helpers for the d3d11_buffers area. */
#ifndef BUFH_H
#define BUFH_H
#include "d3dh.h"
#include <d3d11_1.h>

/* Lazily created device shared by every test of this exe (released at exit). Returns 0 and skips when unavailable. */
int bufh_init(void);
extern ID3D11Device *g_dev;
extern ID3D11DeviceContext *g_ctx;
extern D3D_FEATURE_LEVEL g_fl;

#define NEED_DEV() do { if (!bufh_init()) { t_skip("no D3D11 device"); return; } } while (0)
#define NEED_FL11() do { NEED_DEV(); if (g_fl < D3D_FEATURE_LEVEL_11_0) { t_skip("feature level < 11_0"); return; } } while (0)

/* Build a desc. */
D3D11_BUFFER_DESC bufh_desc(UINT bw, D3D11_USAGE usage, UINT bind, UINT cpu, UINT misc, UINT stride);
/* Create with a byte pattern (pat[i] = (seed + i*7) & 0xff when init != 0). */
HRESULT bufh_create(const D3D11_BUFFER_DESC *d, int init, unsigned seed, ID3D11Buffer **out);
void bufh_pattern(unsigned char *p, UINT n, unsigned seed);
/* Read back whole buffer through a private staging copy (malloc'd, free() it). NULL on failure. */
unsigned char *bufh_read(ID3D11Buffer *b, UINT *size);
/* Compare the buffer contents with a pattern; returns 1 when equal. */
int bufh_matches(ID3D11Buffer *b, const unsigned char *want, UINT n);
int bufh_matches_pattern(ID3D11Buffer *b, unsigned seed);
UINT bufh_bytes(ID3D11Buffer *b);
const char *bufh_hrn(HRESULT hr);
/* A private device (own lifetime, may be destroyed/removed by the test). Returns the HRESULT of creation. */
HRESULT bufh_priv_device(UINT flags, ID3D11Device **dev, ID3D11DeviceContext **ctx);
/* 1 when the adapter behind dev is the Microsoft Basic Render Driver (vendor 0x1414, WARP-backed: the oracle's only adapter). */
int bufh_is_brd(ID3D11Device *dev);
#endif
