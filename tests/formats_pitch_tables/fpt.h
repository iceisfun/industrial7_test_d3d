/* Local helpers for the formats_pitch_tables area: format layout table and cached devices. */
#ifndef FPT_H
#define FPT_H
#include "d3dh.h"

/* Layout groups. */
enum {
    G_NONE = 0,   /* id not assigned in DXGI_FORMAT (116..129) */
    G_UNKNOWN,
    G_B8, G_B16, G_B32, G_B64, G_B96, G_B128,
    G_BC1, G_BC2, G_BC3, G_BC4, G_BC5, G_BC6, G_BC7,
    G_422,        /* R8G8_B8G8 / G8R8_G8B8: 2 pixels per 4 bytes */
    G_R1,         /* 1 bit per pixel */
    G_VPACK,      /* packed video: AYUV Y410 Y416 YUY2 Y210 Y216 */
    G_PLANAR,     /* planar video: NV12 P010 P016 420_OPAQUE NV11 P208 V208 V408 */
    G_PAL         /* palettized: AI44 IA44 P8 A8P8 */
};

typedef struct { UINT id; const char *name; UINT group; UINT bpb; UINT bw, bh; } fpt_fmt;

/* id, name, group, bytes per block (luma bytes per pixel for planar), block width, block height */
#define FPT_TABLE(X) \
 X(0,UNKNOWN,G_UNKNOWN,0,1,1) \
 X(1,R32G32B32A32_TYPELESS,G_B128,16,1,1) X(2,R32G32B32A32_FLOAT,G_B128,16,1,1) X(3,R32G32B32A32_UINT,G_B128,16,1,1) X(4,R32G32B32A32_SINT,G_B128,16,1,1) \
 X(5,R32G32B32_TYPELESS,G_B96,12,1,1) X(6,R32G32B32_FLOAT,G_B96,12,1,1) X(7,R32G32B32_UINT,G_B96,12,1,1) X(8,R32G32B32_SINT,G_B96,12,1,1) \
 X(9,R16G16B16A16_TYPELESS,G_B64,8,1,1) X(10,R16G16B16A16_FLOAT,G_B64,8,1,1) X(11,R16G16B16A16_UNORM,G_B64,8,1,1) X(12,R16G16B16A16_UINT,G_B64,8,1,1) X(13,R16G16B16A16_SNORM,G_B64,8,1,1) X(14,R16G16B16A16_SINT,G_B64,8,1,1) \
 X(15,R32G32_TYPELESS,G_B64,8,1,1) X(16,R32G32_FLOAT,G_B64,8,1,1) X(17,R32G32_UINT,G_B64,8,1,1) X(18,R32G32_SINT,G_B64,8,1,1) \
 X(19,R32G8X24_TYPELESS,G_B64,8,1,1) X(20,D32_FLOAT_S8X24_UINT,G_B64,8,1,1) X(21,R32_FLOAT_X8X24_TYPELESS,G_B64,8,1,1) X(22,X32_TYPELESS_G8X24_UINT,G_B64,8,1,1) \
 X(23,R10G10B10A2_TYPELESS,G_B32,4,1,1) X(24,R10G10B10A2_UNORM,G_B32,4,1,1) X(25,R10G10B10A2_UINT,G_B32,4,1,1) X(26,R11G11B10_FLOAT,G_B32,4,1,1) \
 X(27,R8G8B8A8_TYPELESS,G_B32,4,1,1) X(28,R8G8B8A8_UNORM,G_B32,4,1,1) X(29,R8G8B8A8_UNORM_SRGB,G_B32,4,1,1) X(30,R8G8B8A8_UINT,G_B32,4,1,1) X(31,R8G8B8A8_SNORM,G_B32,4,1,1) X(32,R8G8B8A8_SINT,G_B32,4,1,1) \
 X(33,R16G16_TYPELESS,G_B32,4,1,1) X(34,R16G16_FLOAT,G_B32,4,1,1) X(35,R16G16_UNORM,G_B32,4,1,1) X(36,R16G16_UINT,G_B32,4,1,1) X(37,R16G16_SNORM,G_B32,4,1,1) X(38,R16G16_SINT,G_B32,4,1,1) \
 X(39,R32_TYPELESS,G_B32,4,1,1) X(40,D32_FLOAT,G_B32,4,1,1) X(41,R32_FLOAT,G_B32,4,1,1) X(42,R32_UINT,G_B32,4,1,1) X(43,R32_SINT,G_B32,4,1,1) \
 X(44,R24G8_TYPELESS,G_B32,4,1,1) X(45,D24_UNORM_S8_UINT,G_B32,4,1,1) X(46,R24_UNORM_X8_TYPELESS,G_B32,4,1,1) X(47,X24_TYPELESS_G8_UINT,G_B32,4,1,1) \
 X(48,R8G8_TYPELESS,G_B16,2,1,1) X(49,R8G8_UNORM,G_B16,2,1,1) X(50,R8G8_UINT,G_B16,2,1,1) X(51,R8G8_SNORM,G_B16,2,1,1) X(52,R8G8_SINT,G_B16,2,1,1) \
 X(53,R16_TYPELESS,G_B16,2,1,1) X(54,R16_FLOAT,G_B16,2,1,1) X(55,D16_UNORM,G_B16,2,1,1) X(56,R16_UNORM,G_B16,2,1,1) X(57,R16_UINT,G_B16,2,1,1) X(58,R16_SNORM,G_B16,2,1,1) X(59,R16_SINT,G_B16,2,1,1) \
 X(60,R8_TYPELESS,G_B8,1,1,1) X(61,R8_UNORM,G_B8,1,1,1) X(62,R8_UINT,G_B8,1,1,1) X(63,R8_SNORM,G_B8,1,1,1) X(64,R8_SINT,G_B8,1,1,1) X(65,A8_UNORM,G_B8,1,1,1) \
 X(66,R1_UNORM,G_R1,1,8,1) X(67,R9G9B9E5_SHAREDEXP,G_B32,4,1,1) \
 X(68,R8G8_B8G8_UNORM,G_422,4,2,1) X(69,G8R8_G8B8_UNORM,G_422,4,2,1) \
 X(70,BC1_TYPELESS,G_BC1,8,4,4) X(71,BC1_UNORM,G_BC1,8,4,4) X(72,BC1_UNORM_SRGB,G_BC1,8,4,4) \
 X(73,BC2_TYPELESS,G_BC2,16,4,4) X(74,BC2_UNORM,G_BC2,16,4,4) X(75,BC2_UNORM_SRGB,G_BC2,16,4,4) \
 X(76,BC3_TYPELESS,G_BC3,16,4,4) X(77,BC3_UNORM,G_BC3,16,4,4) X(78,BC3_UNORM_SRGB,G_BC3,16,4,4) \
 X(79,BC4_TYPELESS,G_BC4,8,4,4) X(80,BC4_UNORM,G_BC4,8,4,4) X(81,BC4_SNORM,G_BC4,8,4,4) \
 X(82,BC5_TYPELESS,G_BC5,16,4,4) X(83,BC5_UNORM,G_BC5,16,4,4) X(84,BC5_SNORM,G_BC5,16,4,4) \
 X(85,B5G6R5_UNORM,G_B16,2,1,1) X(86,B5G5R5A1_UNORM,G_B16,2,1,1) X(87,B8G8R8A8_UNORM,G_B32,4,1,1) X(88,B8G8R8X8_UNORM,G_B32,4,1,1) X(89,R10G10B10_XR_BIAS_A2_UNORM,G_B32,4,1,1) \
 X(90,B8G8R8A8_TYPELESS,G_B32,4,1,1) X(91,B8G8R8A8_UNORM_SRGB,G_B32,4,1,1) X(92,B8G8R8X8_TYPELESS,G_B32,4,1,1) X(93,B8G8R8X8_UNORM_SRGB,G_B32,4,1,1) \
 X(94,BC6H_TYPELESS,G_BC6,16,4,4) X(95,BC6H_UF16,G_BC6,16,4,4) X(96,BC6H_SF16,G_BC6,16,4,4) \
 X(97,BC7_TYPELESS,G_BC7,16,4,4) X(98,BC7_UNORM,G_BC7,16,4,4) X(99,BC7_UNORM_SRGB,G_BC7,16,4,4) \
 X(100,AYUV,G_VPACK,4,1,1) X(101,Y410,G_VPACK,4,1,1) X(102,Y416,G_VPACK,8,1,1) \
 X(103,NV12,G_PLANAR,1,2,2) X(104,P010,G_PLANAR,2,2,2) X(105,P016,G_PLANAR,2,2,2) X(106,420_OPAQUE,G_PLANAR,1,2,2) \
 X(107,YUY2,G_VPACK,4,2,1) X(108,Y210,G_VPACK,8,2,1) X(109,Y216,G_VPACK,8,2,1) \
 X(110,NV11,G_PLANAR,1,4,1) X(111,AI44,G_PAL,1,1,1) X(112,IA44,G_PAL,1,1,1) X(113,P8,G_PAL,1,1,1) X(114,A8P8,G_PAL,2,1,1) X(115,B4G4R4A4_UNORM,G_B16,2,1,1) \
 X(130,P208,G_PLANAR,1,2,1) X(131,V208,G_PLANAR,1,1,2) X(132,V408,G_PLANAR,1,1,1)

#define FPT_NFORMATS 133
extern const fpt_fmt fpt_formats[FPT_NFORMATS];   /* indexed by id; unassigned ids have group G_NONE */

static inline UINT fpt_ceil_div(UINT a, UINT b) { return (a + b - 1) / b; }
static inline UINT fpt_align(UINT v, UINT a) { return (v + a - 1) / a * a; }
static inline UINT fpt_mip(UINT v, UINT m) { v >>= m; return v ? v : 1; }
/* Minimum unpadded bytes of one row of the (first plane of the) image, width w. */
static inline UINT fpt_row_bytes(const fpt_fmt *f, UINT w)
{
    if (f->group == G_PLANAR) return w * f->bpb;
    return fpt_ceil_div(w, f->bw) * f->bpb;
}
static inline UINT fpt_rows(const fpt_fmt *f, UINT h)
{
    if (f->group == G_PLANAR) return h;
    return fpt_ceil_div(h, f->bh);
}
static inline int fpt_is_bc(const fpt_fmt *f) { return f->group >= G_BC1 && f->group <= G_BC7; }
static inline UINT fpt_calc_sub(UINT mip, UINT arr, UINT mips) { return mip + arr * mips; }
static inline UINT fpt_calc_sub12(UINT mip, UINT arr, UINT plane, UINT mips, UINT arrsize) { return mip + arr * mips + plane * mips * arrsize; }

/* The widths of the table. */
#define FPT_NW 12
extern const UINT fpt_widths[FPT_NW];

/* Cached devices (created on first use, released at exit). Return NULL when unavailable. */
ID3D11Device *fpt_dev11(ID3D11DeviceContext **ctx, D3D_FEATURE_LEVEL *fl);
ID3D10Device *fpt_dev10(void);
ID3D12Device *fpt_dev12(void);
IDirect3DDevice9 *fpt_dev9(IDirect3D9 **d3d);

/* Compare a measured table entry: golden key -> printf-formatted value. Enforced on WARP (the reference adapter), only noted elsewhere. */
void fpt_golden(const char *file, int line, const char *key, const char *fmt, ...);
#define FPT_GOLDEN(key, ...) fpt_golden(__FILE__, __LINE__, key, __VA_ARGS__)
/* Golden HRESULT by formatted description (fpt_pins.inc, measured on the WARP reference). */
void fpt_pinf(const char *file, int line, HRESULT hr, const char *fmt, ...);
#define PINF(hr, ...) fpt_pinf(__FILE__, __LINE__, (hr), __VA_ARGS__)
int fpt_is_warp(void);   /* default D3D11 adapter is the Microsoft Basic Render Driver */

/* D3D11: CheckFormatSupport word, 0 on failure (hr in *hr_out when non-NULL). */
UINT fpt_support11(ID3D11Device *dev, UINT fmt, HRESULT *hr_out);
#endif
