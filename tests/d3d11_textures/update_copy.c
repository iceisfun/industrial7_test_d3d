/* UpdateSubresource (boxes, pitches, block alignment, mips/arrays) and CopySubresourceRegion/CopyResource
 * (offsets, boxes, same-family reinterpretation, block-compressed <-> uncompressed). */
#include "txh.h"

/* ---- unit geometry (texel or block) and patching ---------------------------------------------------------------- */
static UINT tx_bw(DXGI_FORMAT f) { tx_finfo_t i; return tx_finfo(f, &i) ? i.bw : 1; }
static UINT tx_bh(DXGI_FORMAT f) { tx_finfo_t i; return tx_finfo(f, &i) ? i.bh : 1; }
static UINT tx_bpb(DXGI_FORMAT f) { tx_finfo_t i; return tx_finfo(f, &i) ? i.bpb : 0; }
static UINT tx_units(UINT v, UINT b) { return (v + b - 1) / b; }

/* tight pattern image of w*h*d units of u bytes */
static unsigned char *tx_patbuf(size_t n, UINT seed) { unsigned char *p = malloc(n ? n : 1); for (size_t i = 0; i < n; i++) p[i] = tx_pat(seed, i); return p; }

/* copy a cw*ch*cd region (unit coordinates) */
static void tx_patch(unsigned char *dst, UINT dwu, UINT dhu, const unsigned char *src, UINT swu, UINT shu, UINT u,
                     UINT sx, UINT sy, UINT sz, UINT cw, UINT ch, UINT cd, UINT dx, UINT dy, UINT dz) {
    for (UINT z = 0; z < cd; z++) for (UINT y = 0; y < ch; y++)
        memcpy(dst + (((size_t)(dz + z) * dhu + dy + y) * dwu + dx) * u, src + (((size_t)(sz + z) * shu + sy + y) * swu + sx) * u, (size_t)cw * u);
}

static void tx_cmp_img(const tx_img *im, const unsigned char *exp, UINT sub, const char *what, const char *file, int line) {
    size_t n = (size_t)im->rowbytes * im->rows * im->d, i;
    for (i = 0; i < n; i++) if (im->data[i] != exp[i]) break;
    t_check(i == n, file, line, what, "sub %u (%ux%ux%u) first mismatch at byte %u (got %02x want %02x)", sub, im->w, im->h, im->d, (unsigned)i, i < n ? im->data[i] : 0, i < n ? exp[i] : 0);
}

static HRESULT tx_mk_dim(tx_ctx *c, int dim, UINT w, UINT h, UINT d, UINT mips, UINT arr, DXGI_FORMAT f, D3D11_USAGE u, UINT bind, UINT cpu, UINT misc, const D3D11_SUBRESOURCE_DATA *sd, ID3D11Resource **out) {
    *out = NULL;
    if (dim == 1) return tx_mk1d(c, w, mips, arr, f, u, bind, cpu, misc, sd, (ID3D11Texture1D **)out);
    if (dim == 2) return tx_mk2d(c, w, h, mips, arr, f, u, bind, cpu, misc, sd, (ID3D11Texture2D **)out);
    return tx_mk3d(c, w, h, d, mips, f, u, bind, cpu, misc, sd, (ID3D11Texture3D **)out);
}

/* ---- UpdateSubresource ---------------------------------------------------------------------------------------------- */
typedef struct { int dim; UINT w, h, d, mips, arr; DXGI_FORMAT f; UINT sub; int use_box; UINT bx, by, bz, cw, ch, cd, rowpad, slicepad, bind; } tx_upd;
static void tx_upd_run(tx_ctx *c, const tx_upd *p, const char *file, int line) {
    tx_init base, up; ID3D11Resource *res = NULL; HRESULT hr; D3D11_BOX box; UINT m, n, s;
    UINT mip, tw, th, td, bwx = tx_bw(p->f), bhx = tx_bh(p->f), u = tx_bpb(p->f), cw, ch, cd, bx, by, bz;
    m = p->mips ? p->mips : tx_full_mips(p->w > p->h ? (p->w > p->d ? p->w : p->d) : (p->h > p->d ? p->h : p->d));
    n = m * p->arr;
    if (!tx_init_make(&base, p->w, p->h, p->d, p->mips, p->arr, p->f, 0, 0, 200)) { t_check(0, file, line, "init", "oom"); return; }
    hr = tx_mk_dim(c, p->dim, p->w, p->h, p->d, p->mips, p->arr, p->f, U_DEF, p->bind, 0, 0, base.sd, &res);
    t_check(hr == S_OK, file, line, "create", "%s", d3dh_hr_name(hr));
    if (FAILED(hr)) { tx_init_free(&base); return; }
    mip = p->sub % m; tw = tx_dim(p->w, mip); th = p->dim >= 2 ? tx_dim(p->h, mip) : 1; td = p->dim == 3 ? tx_dim(p->d, mip) : 1;
    if (p->use_box) { bx = p->bx; by = p->by; bz = p->bz; cw = p->cw; ch = p->ch; cd = p->cd; }
    else { bx = by = bz = 0; cw = tw; ch = th; cd = td; }
    if (!tx_init_make(&up, cw, ch, cd, 1, 1, p->f, p->rowpad, p->slicepad, 300)) { t_check(0, file, line, "init", "oom"); return; }
    box.left = bx; box.right = bx + cw; box.top = by; box.bottom = by + ch; box.front = bz; box.back = bz + cd;
    ID3D11DeviceContext_UpdateSubresource(c->ctx, res, p->sub, p->use_box ? &box : NULL, up.sd[0].pSysMem, up.sd[0].SysMemPitch, up.sd[0].SysMemSlicePitch);
    for (s = 0; s < n; s++) {
        UINT sm = s % m, sw = tx_dim(p->w, sm), sh = p->dim >= 2 ? tx_dim(p->h, sm) : 1, sdp = p->dim == 3 ? tx_dim(p->d, sm) : 1;
        UINT swu = tx_units(sw, bwx), shu = tx_units(sh, bhx); size_t sz = (size_t)swu * shu * sdp * u;
        unsigned char *exp = tx_patbuf(sz, 200 + s); tx_img im;
        if (s == p->sub) {
            unsigned char *src = tx_patbuf((size_t)tx_units(cw, bwx) * tx_units(ch, bhx) * cd * u, 300);
            tx_patch(exp, swu, shu, src, tx_units(cw, bwx), tx_units(ch, bhx), u, 0, 0, 0, tx_units(cw, bwx), tx_units(ch, bhx), cd, bx / bwx, by / bhx, bz);
            free(src);
        }
        hr = tx_fetch(c, res, s, &im);
        t_check(hr == S_OK, file, line, "fetch", "sub %u: %s", s, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) { tx_cmp_img(&im, exp, s, s == p->sub ? "updated subresource" : "untouched subresource", file, line); tx_img_free(&im); }
        free(exp);
    }
    ID3D11Resource_Release(res);
    tx_init_free(&base); tx_init_free(&up);
}
#define UPD(nm, ...) DEVTEST(d3d11_textures_update_##nm) { tx_upd p = { __VA_ARGS__ }; tx_upd_run(c, &p, __FILE__, __LINE__); }
/* dim w h d mips arr fmt sub use_box bx by bz cw ch cd rowpad slicepad bind */
UPD(2d_full_null_box, 2, 8, 8, 1, 1, 1, F_RGBA8, 0, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(2d_full_padded_pitch, 2, 8, 8, 1, 1, 1, F_RGBA8, 0, 0, 0,0,0, 0,0,0, 20, 0, 0)
UPD(2d_full_rt_bind, 2, 8, 8, 1, 1, 1, F_RGBA8, 0, 0, 0,0,0, 0,0,0, 0, 0, B_RT | B_SRV)
UPD(2d_full_odd_dims, 2, 7, 5, 1, 1, 1, F_RGBA8, 0, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(2d_box_middle, 2, 8, 8, 1, 1, 1, F_RGBA8, 0, 1, 2,1,0, 4,3,1, 0, 0, 0)
UPD(2d_box_middle_padded, 2, 8, 8, 1, 1, 1, F_RGBA8, 0, 1, 2,1,0, 4,3,1, 12, 0, 0)
UPD(2d_box_single_texel, 2, 8, 8, 1, 1, 1, F_RGBA8, 0, 1, 3,5,0, 1,1,1, 0, 0, 0)
UPD(2d_box_corner_origin, 2, 8, 8, 1, 1, 1, F_RGBA8, 0, 1, 0,0,0, 2,2,1, 0, 0, 0)
UPD(2d_box_corner_last, 2, 8, 8, 1, 1, 1, F_RGBA8, 0, 1, 6,6,0, 2,2,1, 0, 0, 0)
UPD(2d_box_row, 2, 8, 8, 1, 1, 1, F_RGBA8, 0, 1, 0,4,0, 8,1,1, 0, 0, 0)
UPD(2d_box_column, 2, 8, 8, 1, 1, 1, F_RGBA8, 0, 1, 5,0,0, 1,8,1, 0, 0, 0)
UPD(2d_box_full_extent, 2, 8, 8, 1, 1, 1, F_RGBA8, 0, 1, 0,0,0, 8,8,1, 0, 0, 0)
UPD(2d_box_unaligned_texels, 2, 13, 11, 1, 1, 1, F_RGBA8, 0, 1, 3,2,0, 7,5,1, 1, 0, 0)
UPD(2d_r8_box, 2, 16, 16, 1, 1, 1, F_R8, 0, 1, 3,3,0, 5,7,1, 0, 0, 0)
UPD(2d_r8_box_padded, 2, 16, 16, 1, 1, 1, F_R8, 0, 1, 1,2,0, 3,3,1, 1, 0, 0)
UPD(2d_r16g16_box, 2, 9, 9, 1, 1, 1, DXGI_FORMAT_R16G16_UNORM, 0, 1, 2,2,0, 3,3,1, 0, 0, 0)
UPD(2d_r32g32b32a32_box, 2, 6, 6, 1, 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 1, 1,1,0, 3,2,1, 0, 0, 0)
UPD(2d_r32g32b32_box, 2, 6, 6, 1, 1, 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, 1, 1,1,0, 3,2,1, 4, 0, 0)
UPD(2d_r10g10b10a2_box, 2, 6, 6, 1, 1, 1, DXGI_FORMAT_R10G10B10A2_UNORM, 0, 1, 2,0,0, 3,3,1, 0, 0, 0)
UPD(2d_mip1_full, 2, 16, 16, 1, 4, 1, F_RGBA8, 1, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(2d_mip2_box, 2, 16, 16, 1, 4, 1, F_RGBA8, 2, 1, 1,1,0, 2,2,1, 0, 0, 0)
UPD(2d_mip3_full_1x2, 2, 16, 16, 1, 5, 1, F_RGBA8, 3, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(2d_last_mip_1x1, 2, 8, 8, 1, 4, 1, F_RGBA8, 3, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(2d_array_slice1_mip0, 2, 8, 8, 1, 3, 3, F_RGBA8, 3, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(2d_array_slice2_mip1_box, 2, 8, 8, 1, 3, 3, F_RGBA8, 7, 1, 1,1,0, 2,2,1, 0, 0, 0)
UPD(2d_array_slice2_mip2, 2, 8, 8, 1, 3, 3, F_RGBA8, 8, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(2d_cube_face3, 2, 8, 8, 1, 1, 6, F_RGBA8, 3, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(2d_typeless_box, 2, 8, 8, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, 0, 1, 2,2,0, 4,4,1, 0, 0, 0)
UPD(2d_bgra_box, 2, 8, 8, 1, 1, 1, F_BGRA8, 0, 1, 2,2,0, 4,4,1, 0, 0, 0)
UPD(2d_a8_box, 2, 8, 8, 1, 1, 1, DXGI_FORMAT_A8_UNORM, 0, 1, 1,1,0, 6,6,1, 0, 0, 0)
UPD(2d_bc1_full, 2, 16, 16, 1, 1, 1, DXGI_FORMAT_BC1_UNORM, 0, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(2d_bc1_full_padded, 2, 16, 16, 1, 1, 1, DXGI_FORMAT_BC1_UNORM, 0, 0, 0,0,0, 0,0,0, 16, 0, 0)
UPD(2d_bc1_box_one_block, 2, 16, 16, 1, 1, 1, DXGI_FORMAT_BC1_UNORM, 0, 1, 4,8,0, 4,4,1, 0, 0, 0)
UPD(2d_bc1_box_blocks, 2, 16, 16, 1, 1, 1, DXGI_FORMAT_BC1_UNORM, 0, 1, 4,4,0, 8,4,1, 0, 0, 0)
UPD(2d_bc1_box_blocks_padded, 2, 16, 16, 1, 1, 1, DXGI_FORMAT_BC1_UNORM, 0, 1, 8,0,0, 8,12,1, 8, 0, 0)
UPD(2d_bc1_box_origin, 2, 16, 16, 1, 1, 1, DXGI_FORMAT_BC1_UNORM, 0, 1, 0,0,0, 4,4,1, 0, 0, 0)
UPD(2d_bc1_box_last, 2, 16, 16, 1, 1, 1, DXGI_FORMAT_BC1_UNORM, 0, 1, 12,12,0, 4,4,1, 0, 0, 0)
UPD(2d_bc1_mip1_full, 2, 16, 16, 1, 5, 1, DXGI_FORMAT_BC1_UNORM, 1, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(2d_bc1_mip1_box, 2, 16, 16, 1, 5, 1, DXGI_FORMAT_BC1_UNORM, 1, 1, 4,0,0, 4,8,1, 0, 0, 0)
UPD(2d_bc1_mip2_one_block, 2, 16, 16, 1, 5, 1, DXGI_FORMAT_BC1_UNORM, 2, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(2d_bc1_mip3_2x2, 2, 16, 16, 1, 5, 1, DXGI_FORMAT_BC1_UNORM, 3, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(2d_bc1_mip4_1x1, 2, 16, 16, 1, 5, 1, DXGI_FORMAT_BC1_UNORM, 4, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(2d_bc1_array_slice, 2, 8, 8, 1, 2, 3, DXGI_FORMAT_BC1_UNORM, 4, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(2d_bc2_box, 2, 16, 8, 1, 1, 1, DXGI_FORMAT_BC2_UNORM, 0, 1, 4,0,0, 8,8,1, 0, 0, 0)
UPD(2d_bc3_box, 2, 16, 16, 1, 1, 1, DXGI_FORMAT_BC3_UNORM, 0, 1, 8,4,0, 4,8,1, 0, 0, 0)
UPD(2d_bc3_full_padded, 2, 12, 12, 1, 1, 1, DXGI_FORMAT_BC3_UNORM, 0, 0, 0,0,0, 0,0,0, 32, 0, 0)
UPD(2d_bc4_box, 2, 16, 16, 1, 1, 1, DXGI_FORMAT_BC4_UNORM, 0, 1, 4,4,0, 8,8,1, 0, 0, 0)
UPD(2d_bc5_box, 2, 16, 16, 1, 1, 1, DXGI_FORMAT_BC5_UNORM, 0, 1, 12,0,0, 4,16,1, 0, 0, 0)
UPD(2d_bc7_box, 2, 16, 16, 1, 1, 1, DXGI_FORMAT_BC7_UNORM, 0, 1, 4,4,0, 8,4,1, 0, 0, 0)
UPD(2d_bc7_full, 2, 8, 8, 1, 1, 1, DXGI_FORMAT_BC7_UNORM, 0, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(2d_bc6h_box, 2, 8, 8, 1, 1, 1, DXGI_FORMAT_BC6H_UF16, 0, 1, 4,0,0, 4,8,1, 0, 0, 0)
UPD(1d_full, 1, 16, 1, 1, 1, 1, F_RGBA8, 0, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(1d_box_left, 1, 16, 1, 1, 1, 1, F_RGBA8, 0, 1, 0,0,0, 5,1,1, 0, 0, 0)
UPD(1d_box_middle, 1, 16, 1, 1, 1, 1, F_RGBA8, 0, 1, 4,0,0, 5,1,1, 0, 0, 0)
UPD(1d_box_last, 1, 16, 1, 1, 1, 1, F_RGBA8, 0, 1, 15,0,0, 1,1,1, 0, 0, 0)
UPD(1d_mip2, 1, 16, 1, 1, 5, 1, F_RGBA8, 2, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(1d_array_slice1, 1, 16, 1, 1, 3, 3, F_RGBA8, 4, 1, 2,0,0, 4,1,1, 0, 0, 0)
UPD(1d_r8_box, 1, 33, 1, 1, 1, 1, F_R8, 0, 1, 7,0,0, 20,1,1, 0, 0, 0)
UPD(3d_full, 3, 4, 4, 4, 1, 1, F_RGBA8, 0, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(3d_full_padded, 3, 4, 4, 4, 1, 1, F_RGBA8, 0, 0, 0,0,0, 0,0,0, 8, 16, 0)
UPD(3d_box_inner, 3, 4, 4, 4, 1, 1, F_RGBA8, 0, 1, 1,1,1, 2,2,2, 0, 0, 0)
UPD(3d_box_inner_padded, 3, 4, 4, 4, 1, 1, F_RGBA8, 0, 1, 1,1,1, 2,2,2, 8, 8, 0)
UPD(3d_box_one_slice, 3, 4, 4, 4, 1, 1, F_RGBA8, 0, 1, 0,0,2, 4,4,1, 0, 0, 0)
UPD(3d_box_column_depth, 3, 4, 4, 4, 1, 1, F_RGBA8, 0, 1, 2,3,0, 1,1,4, 0, 0, 0)
UPD(3d_box_row_slices, 3, 5, 3, 4, 1, 1, F_RGBA8, 0, 1, 1,1,0, 3,1,4, 0, 0, 0)
UPD(3d_box_last_texel, 3, 4, 4, 4, 1, 1, F_RGBA8, 0, 1, 3,3,3, 1,1,1, 0, 0, 0)
UPD(3d_odd_full, 3, 5, 3, 7, 1, 1, F_RGBA8, 0, 0, 0,0,0, 0,0,0, 4, 4, 0)
UPD(3d_mip1, 3, 8, 8, 8, 4, 1, F_RGBA8, 1, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(3d_mip2_box, 3, 8, 8, 8, 4, 1, F_RGBA8, 2, 1, 1,0,1, 1,2,1, 0, 0, 0)
UPD(3d_mip3_1x1x1, 3, 8, 8, 8, 4, 1, F_RGBA8, 3, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(3d_r8_box, 3, 8, 4, 4, 1, 1, F_R8, 0, 1, 2,1,1, 4,2,2, 3, 1, 0)
UPD(3d_r32f_full, 3, 3, 3, 3, 1, 1, DXGI_FORMAT_R32_FLOAT, 0, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(3d_bc1_full, 3, 8, 8, 2, 1, 1, DXGI_FORMAT_BC1_UNORM, 0, 0, 0,0,0, 0,0,0, 0, 0, 0)
UPD(3d_bc1_box, 3, 8, 8, 4, 1, 1, DXGI_FORMAT_BC1_UNORM, 0, 1, 4,0,1, 4,8,2, 0, 0, 0)
UPD(3d_bc1_box_padded, 3, 8, 8, 4, 1, 1, DXGI_FORMAT_BC1_UNORM, 0, 1, 0,4,2, 8,4,2, 8, 8, 0)
UPD(3d_bc3_full_padded, 3, 8, 4, 2, 1, 1, DXGI_FORMAT_BC3_UNORM, 0, 0, 0,0,0, 0,0,0, 16, 16, 0)
UPD(3d_bc7_box, 3, 8, 8, 2, 1, 1, DXGI_FORMAT_BC7_UNORM, 0, 1, 4,4,0, 4,4,2, 0, 0, 0)

/* An empty box is a no-op (documented), whichever extent is degenerate. */
static void tx_empty_box(tx_ctx *c, int axis, const char *file, int line) {
    tx_init base, up; ID3D11Resource *res = NULL; D3D11_BOX box; tx_img im; unsigned char *exp;
    CHECK(tx_init_make(&base, 4, 4, 4, 1, 1, F_RGBA8, 0, 0, 200));
    CHECK(tx_init_make(&up, 4, 4, 4, 1, 1, F_RGBA8, 0, 0, 300));
    EXPECT_HR(tx_mk3d(c, 4, 4, 4, 1, F_RGBA8, U_DEF, 0, 0, 0, base.sd, (ID3D11Texture3D **)&res), S_OK);
    if (res) {
        box.left = 0; box.right = 4; box.top = 0; box.bottom = 4; box.front = 0; box.back = 4;
        if (axis == 0) box.right = box.left;
        if (axis == 1) box.bottom = box.top;
        if (axis == 2) box.back = box.front;
        if (axis == 3) { box.left = 3; box.right = 2; }
        if (axis == 4) { box.top = 3; box.bottom = 1; }
        if (axis == 5) { box.front = 3; box.back = 0; }
        ID3D11DeviceContext_UpdateSubresource(c->ctx, res, 0, &box, up.sd[0].pSysMem, up.sd[0].SysMemPitch, up.sd[0].SysMemSlicePitch);
        exp = tx_patbuf(4 * 4 * 4 * 4, 200);
        if (SUCCEEDED(tx_fetch(c, res, 0, &im))) { tx_cmp_img(&im, exp, 0, "empty box leaves texture unchanged", file, line); tx_img_free(&im); }
        free(exp);
        ID3D11Resource_Release(res);
    }
    tx_init_free(&base); tx_init_free(&up);
}
#define EMPTYBOX(nm, AX) DEVTEST(d3d11_textures_update_empty_box_##nm) { tx_empty_box(c, AX, __FILE__, __LINE__); }
EMPTYBOX(left_eq_right, 0)
EMPTYBOX(top_eq_bottom, 1)
EMPTYBOX(front_eq_back, 2)
EMPTYBOX(left_gt_right, 3)
EMPTYBOX(top_gt_bottom, 4)
EMPTYBOX(front_gt_back, 5)

DEVTEST(d3d11_textures_update_source_copied_immediately) {
    tx_init base, up; ID3D11Texture2D *t = NULL; tx_img im; unsigned char *exp;
    CHECK(tx_init_make(&base, 4, 4, 1, 1, 1, F_RGBA8, 0, 0, 200));
    CHECK(tx_init_make(&up, 4, 4, 1, 1, 1, F_RGBA8, 0, 0, 300));
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, F_RGBA8, U_DEF, 0, 0, 0, base.sd, &t), S_OK);
    if (t) {
        ID3D11DeviceContext_UpdateSubresource(c->ctx, (ID3D11Resource *)t, 0, NULL, up.sd[0].pSysMem, 16, 0);
        memset(up.blocks[0], 0xFF, 64);   /* caller may reuse its memory once UpdateSubresource returns */
        exp = tx_patbuf(64, 300);
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 0, &im))) { tx_cmp_img(&im, exp, 0, "update data snapshot", __FILE__, __LINE__); tx_img_free(&im); }
        free(exp);
        ID3D11Texture2D_Release(t);
    }
    tx_init_free(&base); tx_init_free(&up);
}
DEVTEST(d3d11_textures_update_twice_last_wins) {
    tx_init base, a, b; ID3D11Texture2D *t = NULL; tx_img im; unsigned char *exp;
    CHECK(tx_init_make(&base, 4, 4, 1, 1, 1, F_RGBA8, 0, 0, 200));
    CHECK(tx_init_make(&a, 4, 4, 1, 1, 1, F_RGBA8, 0, 0, 301));
    CHECK(tx_init_make(&b, 4, 4, 1, 1, 1, F_RGBA8, 0, 0, 302));
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, F_RGBA8, U_DEF, 0, 0, 0, base.sd, &t), S_OK);
    if (t) {
        ID3D11DeviceContext_UpdateSubresource(c->ctx, (ID3D11Resource *)t, 0, NULL, a.sd[0].pSysMem, 16, 0);
        ID3D11DeviceContext_UpdateSubresource(c->ctx, (ID3D11Resource *)t, 0, NULL, b.sd[0].pSysMem, 16, 0);
        exp = tx_patbuf(64, 302);
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 0, &im))) { tx_cmp_img(&im, exp, 0, "last update wins", __FILE__, __LINE__); tx_img_free(&im); }
        free(exp);
        ID3D11Texture2D_Release(t);
    }
    tx_init_free(&base); tx_init_free(&a); tx_init_free(&b);
}

/* ---- CopySubresourceRegion -------------------------------------------------------------------------------------------- */
typedef struct {
    int dim; UINT w, h, d, mips, arr; DXGI_FORMAT sf;          /* source */
    UINT dw, dh, dd, dmips, darr; DXGI_FORMAT df;               /* destination (texels of df) */
    UINT ssub, dsub; int use_box; UINT sx, sy, sz, cw, ch, cd;  /* box in source units (texels of sf) */
    UINT dx, dy, dz;                                            /* destination offset in texels of df */
    int hard;                                                   /* 0: only note a mismatch */
} tx_cp;
static void tx_cp_run(tx_ctx *c, const tx_cp *p, const char *file, int line) {
    tx_init si, di; ID3D11Resource *src = NULL, *dst = NULL; HRESULT hr; D3D11_BOX box;
    UINT sm, dm, sn, dn, s, su = tx_bpb(p->sf), du = tx_bpb(p->df);
    UINT sbw = tx_bw(p->sf), sbh = tx_bh(p->sf), dbw = tx_bw(p->df), dbh = tx_bh(p->df);
    UINT mm = p->w > p->h ? p->w : p->h; if (p->d > mm) mm = p->d;
    UINT dmm = p->dw > p->dh ? p->dw : p->dh; if (p->dd > dmm) dmm = p->dd;
    sm = p->mips ? p->mips : tx_full_mips(mm); dm = p->dmips ? p->dmips : tx_full_mips(dmm); sn = sm * p->arr; dn = dm * p->darr;
    CHECK(tx_init_make(&si, p->w, p->h, p->d, p->mips, p->arr, p->sf, 0, 0, 400));
    CHECK(tx_init_make(&di, p->dw, p->dh, p->dd, p->dmips, p->darr, p->df, 0, 0, 500));
    hr = tx_mk_dim(c, p->dim, p->w, p->h, p->d, p->mips, p->arr, p->sf, U_DEF, 0, 0, 0, si.sd, &src);
    t_check(hr == S_OK, file, line, "create source", "%s", d3dh_hr_name(hr));
    hr = tx_mk_dim(c, p->dim, p->dw, p->dh, p->dd, p->dmips, p->darr, p->df, U_DEF, 0, 0, 0, di.sd, &dst);
    t_check(hr == S_OK, file, line, "create destination", "%s", d3dh_hr_name(hr));
    if (src && dst) {
        UINT mip = p->ssub % sm, tw = tx_dim(p->w, mip), th = p->dim >= 2 ? tx_dim(p->h, mip) : 1, td = p->dim == 3 ? tx_dim(p->d, mip) : 1;
        UINT cw, ch, cd, sx, sy, sz;
        if (p->use_box) { sx = p->sx; sy = p->sy; sz = p->sz; cw = p->cw; ch = p->ch; cd = p->cd; } else { sx = sy = sz = 0; cw = tw; ch = th; cd = td; }
        box.left = sx; box.right = sx + cw; box.top = sy; box.bottom = sy + ch; box.front = sz; box.back = sz + cd;
        ID3D11DeviceContext_CopySubresourceRegion(c->ctx, dst, p->dsub, p->dx, p->dy, p->dz, src, p->ssub, p->use_box ? &box : NULL);
        for (s = 0; s < dn; s++) {
            UINT m2 = s % dm, ew = tx_dim(p->dw, m2), eh = p->dim >= 2 ? tx_dim(p->dh, m2) : 1, ed = p->dim == 3 ? tx_dim(p->dd, m2) : 1;
            UINT ewu = tx_units(ew, dbw), ehu = tx_units(eh, dbh); tx_img im;
            unsigned char *exp = tx_patbuf((size_t)ewu * ehu * ed * du, 500 + s);
            if (s == p->dsub) {
                UINT swu = tx_units(tw, sbw), shu = tx_units(th, sbh);
                unsigned char *sb = tx_patbuf((size_t)swu * shu * td * su, 400 + p->ssub);
                tx_patch(exp, ewu, ehu, sb, swu, shu, su, sx / sbw, sy / sbh, sz, tx_units(cw, sbw), tx_units(ch, sbh), cd, p->dx / dbw, p->dy / dbh, p->dz);
                free(sb);
            }
            hr = tx_fetch(c, dst, s, &im);
            t_check(hr == S_OK, file, line, "fetch", "sub %u: %s", s, d3dh_hr_name(hr));
            if (SUCCEEDED(hr)) {
                size_t n = (size_t)im.rowbytes * im.rows * im.d, i;
                for (i = 0; i < n; i++) if (im.data[i] != exp[i]) break;
                if (p->hard || s != p->dsub) t_check(i == n, file, line, s == p->dsub ? "copied subresource" : "untouched subresource", "sub %u first mismatch at byte %u", s, (unsigned)i);
                else if (i != n) t_note("%s:%d copy result differs at byte %u of sub %u", file, line, (unsigned)i, s);
                tx_img_free(&im);
            }
            free(exp);
        }
    }
    SAFE_RELEASE(src); SAFE_RELEASE(dst);
    tx_init_free(&si); tx_init_free(&di);
}
#define CPY(nm, ...) DEVTEST(d3d11_textures_copy_##nm) { tx_cp p = { __VA_ARGS__ }; tx_cp_run(c, &p, __FILE__, __LINE__); }
/* dim w h d mips arr sf | dw dh dd dmips darr df | ssub dsub use_box sx sy sz cw ch cd | dx dy dz | hard */
CPY(2d_whole, 2, 8,8,1,1,1,F_RGBA8, 8,8,1,1,1,F_RGBA8, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(2d_whole_1x1, 2, 1,1,1,1,1,F_RGBA8, 1,1,1,1,1,F_RGBA8, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(2d_whole_odd, 2, 7,5,1,1,1,F_RGBA8, 7,5,1,1,1,F_RGBA8, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(2d_box_to_origin, 2, 8,8,1,1,1,F_RGBA8, 8,8,1,1,1,F_RGBA8, 0,0,1, 2,2,0,3,3,1, 0,0,0, 1)
CPY(2d_box_to_offset, 2, 8,8,1,1,1,F_RGBA8, 8,8,1,1,1,F_RGBA8, 0,0,1, 1,1,0,4,3,1, 3,4,0, 1)
CPY(2d_box_same_offset, 2, 8,8,1,1,1,F_RGBA8, 8,8,1,1,1,F_RGBA8, 0,0,1, 2,2,0,4,4,1, 2,2,0, 1)
CPY(2d_box_single_texel, 2, 8,8,1,1,1,F_RGBA8, 8,8,1,1,1,F_RGBA8, 0,0,1, 7,7,0,1,1,1, 0,0,0, 1)
CPY(2d_box_into_corner, 2, 8,8,1,1,1,F_RGBA8, 8,8,1,1,1,F_RGBA8, 0,0,1, 0,0,0,2,2,1, 6,6,0, 1)
CPY(2d_offset_no_box, 2, 4,4,1,1,1,F_RGBA8, 8,8,1,1,1,F_RGBA8, 0,0,0, 0,0,0,0,0,0, 2,3,0, 1)
CPY(2d_small_into_large, 2, 3,2,1,1,1,F_R8, 16,16,1,1,1,F_R8, 0,0,0, 0,0,0,0,0,0, 13,14,0, 1)
CPY(2d_r8_box, 2, 16,16,1,1,1,F_R8, 16,16,1,1,1,F_R8, 0,0,1, 3,5,0,9,2,1, 1,0,0, 1)
CPY(2d_r32g32b32a32_box, 2, 6,6,1,1,1,DXGI_FORMAT_R32G32B32A32_FLOAT, 6,6,1,1,1,DXGI_FORMAT_R32G32B32A32_FLOAT, 0,0,1, 1,1,0,3,3,1, 2,2,0, 1)
CPY(2d_mip_to_mip, 2, 16,16,1,4,1,F_RGBA8, 16,16,1,4,1,F_RGBA8, 2,2,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(2d_mip1_to_mip0_of_smaller, 2, 16,16,1,3,1,F_RGBA8, 8,8,1,1,1,F_RGBA8, 1,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(2d_mip0_to_mip1_of_larger, 2, 8,8,1,1,1,F_RGBA8, 16,16,1,3,1,F_RGBA8, 0,1,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(2d_slice_to_slice, 2, 8,8,1,2,3,F_RGBA8, 8,8,1,2,3,F_RGBA8, 4,2,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(2d_slice_mip_box, 2, 8,8,1,3,2,F_RGBA8, 8,8,1,3,2,F_RGBA8, 1,4,1, 1,0,0,2,2,1, 0,1,0, 1)
CPY(2d_array_to_non_array_slot, 2, 8,8,1,1,4,F_RGBA8, 8,8,1,1,2,F_RGBA8, 3,1,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(2d_cube_face, 2, 4,4,1,1,6,F_RGBA8, 4,4,1,1,6,F_RGBA8, 5,0,0, 0,0,0,0,0,0, 0,0,0, 1)
/* same type group, different interpretation: the copy is a raw byte copy */
CPY(fam_rgba8_typeless_to_unorm, 2, 8,8,1,1,1,DXGI_FORMAT_R8G8B8A8_TYPELESS, 8,8,1,1,1,F_RGBA8, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_rgba8_unorm_to_typeless, 2, 8,8,1,1,1,F_RGBA8, 8,8,1,1,1,DXGI_FORMAT_R8G8B8A8_TYPELESS, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_rgba8_unorm_to_uint, 2, 8,8,1,1,1,F_RGBA8, 8,8,1,1,1,DXGI_FORMAT_R8G8B8A8_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_rgba8_unorm_to_snorm, 2, 8,8,1,1,1,F_RGBA8, 8,8,1,1,1,DXGI_FORMAT_R8G8B8A8_SNORM, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_rgba8_unorm_to_sint, 2, 8,8,1,1,1,F_RGBA8, 8,8,1,1,1,DXGI_FORMAT_R8G8B8A8_SINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_rgba8_unorm_to_srgb, 2, 8,8,1,1,1,F_RGBA8, 8,8,1,1,1,DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_rgba8_srgb_to_uint_box, 2, 8,8,1,1,1,DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 8,8,1,1,1,DXGI_FORMAT_R8G8B8A8_UINT, 0,0,1, 1,1,0,5,5,1, 2,2,0, 1)
CPY(fam_bgra8_typeless, 2, 8,8,1,1,1,DXGI_FORMAT_B8G8R8A8_TYPELESS, 8,8,1,1,1,F_BGRA8, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_bgra8_srgb, 2, 8,8,1,1,1,F_BGRA8, 8,8,1,1,1,DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_r32_float_to_uint, 2, 8,8,1,1,1,DXGI_FORMAT_R32_FLOAT, 8,8,1,1,1,DXGI_FORMAT_R32_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_r32_typeless_to_sint, 2, 8,8,1,1,1,DXGI_FORMAT_R32_TYPELESS, 8,8,1,1,1,DXGI_FORMAT_R32_SINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_r16_unorm_to_uint, 2, 8,8,1,1,1,DXGI_FORMAT_R16_UNORM, 8,8,1,1,1,DXGI_FORMAT_R16_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_r16_float_to_snorm, 2, 8,8,1,1,1,DXGI_FORMAT_R16_FLOAT, 8,8,1,1,1,DXGI_FORMAT_R16_SNORM, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_r16g16_float_to_uint, 2, 8,8,1,1,1,DXGI_FORMAT_R16G16_FLOAT, 8,8,1,1,1,DXGI_FORMAT_R16G16_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_r16g16b16a16_float_to_uint, 2, 8,8,1,1,1,DXGI_FORMAT_R16G16B16A16_FLOAT, 8,8,1,1,1,DXGI_FORMAT_R16G16B16A16_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_r32g32_float_to_sint, 2, 8,8,1,1,1,DXGI_FORMAT_R32G32_FLOAT, 8,8,1,1,1,DXGI_FORMAT_R32G32_SINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_r32g32b32a32_float_to_uint, 2, 4,4,1,1,1,DXGI_FORMAT_R32G32B32A32_FLOAT, 4,4,1,1,1,DXGI_FORMAT_R32G32B32A32_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_r32g32b32_float_to_uint, 2, 4,4,1,1,1,DXGI_FORMAT_R32G32B32_FLOAT, 4,4,1,1,1,DXGI_FORMAT_R32G32B32_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_r10g10b10a2_unorm_to_uint, 2, 8,8,1,1,1,DXGI_FORMAT_R10G10B10A2_UNORM, 8,8,1,1,1,DXGI_FORMAT_R10G10B10A2_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_r8g8_unorm_to_sint, 2, 8,8,1,1,1,DXGI_FORMAT_R8G8_UNORM, 8,8,1,1,1,DXGI_FORMAT_R8G8_SINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_r8_unorm_to_uint, 2, 8,8,1,1,1,DXGI_FORMAT_R8_UNORM, 8,8,1,1,1,DXGI_FORMAT_R8_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_bc1_unorm_to_srgb, 2, 8,8,1,1,1,DXGI_FORMAT_BC1_UNORM, 8,8,1,1,1,DXGI_FORMAT_BC1_UNORM_SRGB, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_bc1_typeless_to_unorm, 2, 8,8,1,1,1,DXGI_FORMAT_BC1_TYPELESS, 8,8,1,1,1,DXGI_FORMAT_BC1_UNORM, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_bc3_unorm_to_typeless, 2, 8,8,1,1,1,DXGI_FORMAT_BC3_UNORM, 8,8,1,1,1,DXGI_FORMAT_BC3_TYPELESS, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_bc4_unorm_to_snorm, 2, 8,8,1,1,1,DXGI_FORMAT_BC4_UNORM, 8,8,1,1,1,DXGI_FORMAT_BC4_SNORM, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_bc5_unorm_to_snorm, 2, 8,8,1,1,1,DXGI_FORMAT_BC5_UNORM, 8,8,1,1,1,DXGI_FORMAT_BC5_SNORM, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_bc6h_uf16_to_sf16, 2, 8,8,1,1,1,DXGI_FORMAT_BC6H_UF16, 8,8,1,1,1,DXGI_FORMAT_BC6H_SF16, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(fam_bc7_unorm_to_srgb, 2, 8,8,1,1,1,DXGI_FORMAT_BC7_UNORM, 8,8,1,1,1,DXGI_FORMAT_BC7_UNORM_SRGB, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
/* block-compressed <-> uncompressed with equal bytes per block: destination extent is counted in blocks */
CPY(bc1_to_r32g32_uint, 2, 16,16,1,1,1,DXGI_FORMAT_BC1_UNORM, 4,4,1,1,1,DXGI_FORMAT_R32G32_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(bc1_to_r32g32_uint_chain_mip1, 2, 16,16,1,5,1,DXGI_FORMAT_BC1_UNORM, 2,2,1,1,1,DXGI_FORMAT_R32G32_UINT, 1,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(bc1_to_r16g16b16a16_unorm, 2, 8,8,1,1,1,DXGI_FORMAT_BC1_UNORM, 2,2,1,1,1,DXGI_FORMAT_R16G16B16A16_UNORM, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(bc1_to_r16g16b16a16_uint_box, 2, 16,16,1,1,1,DXGI_FORMAT_BC1_UNORM, 8,8,1,1,1,DXGI_FORMAT_R16G16B16A16_UINT, 0,0,1, 4,4,0,8,4,1, 1,2,0, 1)
CPY(bc1_to_r32g32_float, 2, 8,8,1,1,1,DXGI_FORMAT_BC1_UNORM, 2,2,1,1,1,DXGI_FORMAT_R32G32_FLOAT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(bc4_to_r32g32_uint, 2, 16,8,1,1,1,DXGI_FORMAT_BC4_UNORM, 4,2,1,1,1,DXGI_FORMAT_R32G32_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(bc2_to_r32g32b32a32_uint, 2, 16,16,1,1,1,DXGI_FORMAT_BC2_UNORM, 4,4,1,1,1,DXGI_FORMAT_R32G32B32A32_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(bc3_to_r32g32b32a32_uint, 2, 16,16,1,1,1,DXGI_FORMAT_BC3_UNORM, 4,4,1,1,1,DXGI_FORMAT_R32G32B32A32_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(bc3_to_r32g32b32a32_float_box, 2, 16,16,1,1,1,DXGI_FORMAT_BC3_UNORM, 8,8,1,1,1,DXGI_FORMAT_R32G32B32A32_FLOAT, 0,0,1, 4,8,0,8,4,1, 1,1,0, 1)
CPY(bc5_to_r32g32b32a32_uint, 2, 8,8,1,1,1,DXGI_FORMAT_BC5_UNORM, 2,2,1,1,1,DXGI_FORMAT_R32G32B32A32_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(bc6h_to_r32g32b32a32_uint, 2, 8,8,1,1,1,DXGI_FORMAT_BC6H_UF16, 2,2,1,1,1,DXGI_FORMAT_R32G32B32A32_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(bc7_to_r32g32b32a32_uint, 2, 12,8,1,1,1,DXGI_FORMAT_BC7_UNORM, 3,2,1,1,1,DXGI_FORMAT_R32G32B32A32_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(bc7_to_r32g32b32a32_uint_array, 2, 8,8,1,1,3,DXGI_FORMAT_BC7_UNORM, 2,2,1,1,3,DXGI_FORMAT_R32G32B32A32_UINT, 2,1,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(r32g32_uint_to_bc1, 2, 4,4,1,1,1,DXGI_FORMAT_R32G32_UINT, 16,16,1,1,1,DXGI_FORMAT_BC1_UNORM, 0,0,0, 0,0,0,0,0,0, 0,0,0, 0)
CPY(r32g32b32a32_uint_to_bc3, 2, 4,4,1,1,1,DXGI_FORMAT_R32G32B32A32_UINT, 16,16,1,1,1,DXGI_FORMAT_BC3_UNORM, 0,0,0, 0,0,0,0,0,0, 0,0,0, 0)
CPY(r32g32b32a32_uint_to_bc7, 2, 2,2,1,1,1,DXGI_FORMAT_R32G32B32A32_UINT, 8,8,1,1,1,DXGI_FORMAT_BC7_UNORM, 0,0,0, 0,0,0,0,0,0, 0,0,0, 0)
CPY(bc1_whole, 2, 16,16,1,1,1,DXGI_FORMAT_BC1_UNORM, 16,16,1,1,1,DXGI_FORMAT_BC1_UNORM, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(bc1_box_block_aligned, 2, 16,16,1,1,1,DXGI_FORMAT_BC1_UNORM, 16,16,1,1,1,DXGI_FORMAT_BC1_UNORM, 0,0,1, 4,8,0,8,4,1, 8,0,0, 1)
CPY(bc1_chain_mip2, 2, 16,16,1,5,1,DXGI_FORMAT_BC1_UNORM, 16,16,1,5,1,DXGI_FORMAT_BC1_UNORM, 2,2,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(bc1_small_mips, 2, 16,16,1,5,1,DXGI_FORMAT_BC1_UNORM, 16,16,1,5,1,DXGI_FORMAT_BC1_UNORM, 4,4,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(bc3_box, 2, 16,16,1,1,1,DXGI_FORMAT_BC3_UNORM, 16,16,1,1,1,DXGI_FORMAT_BC3_UNORM, 0,0,1, 0,0,0,12,4,1, 4,12,0, 1)
CPY(bc7_whole_array, 2, 8,8,1,1,2,DXGI_FORMAT_BC7_UNORM, 8,8,1,1,2,DXGI_FORMAT_BC7_UNORM, 1,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(1d_whole, 1, 16,1,1,1,1,F_RGBA8, 16,1,1,1,1,F_RGBA8, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(1d_box, 1, 16,1,1,1,1,F_RGBA8, 16,1,1,1,1,F_RGBA8, 0,0,1, 3,0,0,5,1,1, 8,0,0, 1)
CPY(1d_offset_no_box, 1, 4,1,1,1,1,F_RGBA8, 16,1,1,1,1,F_RGBA8, 0,0,0, 0,0,0,0,0,0, 9,0,0, 1)
CPY(1d_mip_slice, 1, 16,1,1,3,2,F_RGBA8, 16,1,1,3,2,F_RGBA8, 4,1,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(1d_family, 1, 8,1,1,1,1,DXGI_FORMAT_R32_FLOAT, 8,1,1,1,1,DXGI_FORMAT_R32_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(3d_whole, 3, 4,4,4,1,1,F_RGBA8, 4,4,4,1,1,F_RGBA8, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(3d_box_inner, 3, 8,8,8,1,1,F_RGBA8, 8,8,8,1,1,F_RGBA8, 0,0,1, 1,2,3,3,3,3, 0,0,0, 1)
CPY(3d_box_with_offset, 3, 8,8,8,1,1,F_RGBA8, 8,8,8,1,1,F_RGBA8, 0,0,1, 0,0,0,2,3,4, 5,4,3, 1)
CPY(3d_slice_to_slice, 3, 4,4,4,1,1,F_RGBA8, 4,4,4,1,1,F_RGBA8, 0,0,1, 0,0,2,4,4,1, 0,0,0, 1)
CPY(3d_slice_into_other, 3, 4,4,4,1,1,F_RGBA8, 4,4,4,1,1,F_RGBA8, 0,0,1, 0,0,0,4,4,1, 0,0,3, 1)
CPY(3d_mip_to_mip, 3, 8,8,8,4,1,F_RGBA8, 8,8,8,4,1,F_RGBA8, 1,1,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(3d_mip_to_smaller_volume, 3, 8,8,8,3,1,F_RGBA8, 4,4,4,1,1,F_RGBA8, 1,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(3d_small_volume_into_mip, 3, 2,2,2,1,1,F_RGBA8, 8,8,8,3,1,F_RGBA8, 0,2,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(3d_family, 3, 4,4,4,1,1,DXGI_FORMAT_R8G8B8A8_UNORM, 4,4,4,1,1,DXGI_FORMAT_R8G8B8A8_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
CPY(3d_bc1_whole, 3, 8,8,4,1,1,DXGI_FORMAT_BC1_UNORM, 8,8,4,1,1,DXGI_FORMAT_BC1_UNORM, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)
/* (the destination box must stay inside the 8x8x4 volume: an out-of-range copy removes the WARP device) */
CPY(3d_bc1_box, 3, 8,8,4,1,1,DXGI_FORMAT_BC1_UNORM, 8,8,4,1,1,DXGI_FORMAT_BC1_UNORM, 0,0,1, 4,0,1,4,4,2, 0,4,2, 1)
CPY(3d_bc1_to_r32g32, 3, 8,8,4,1,1,DXGI_FORMAT_BC1_UNORM, 2,2,4,1,1,DXGI_FORMAT_R32G32_UINT, 0,0,0, 0,0,0,0,0,0, 0,0,0, 1)

/* Empty box: nothing is copied. */
DEVTEST(d3d11_textures_copy_empty_box_noop) {
    tx_init si, di; ID3D11Texture2D *s = NULL, *d = NULL; D3D11_BOX box; tx_img im; unsigned char *exp;
    CHECK(tx_init_make(&si, 4, 4, 1, 1, 1, F_RGBA8, 0, 0, 400));
    CHECK(tx_init_make(&di, 4, 4, 1, 1, 1, F_RGBA8, 0, 0, 500));
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, F_RGBA8, U_DEF, 0, 0, 0, si.sd, &s), S_OK);
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, F_RGBA8, U_DEF, 0, 0, 0, di.sd, &d), S_OK);
    if (s && d) {
        box.left = 2; box.right = 2; box.top = 0; box.bottom = 4; box.front = 0; box.back = 1;
        ID3D11DeviceContext_CopySubresourceRegion(c->ctx, (ID3D11Resource *)d, 0, 0, 0, 0, (ID3D11Resource *)s, 0, &box);
        box.left = 0; box.right = 4; box.top = 3; box.bottom = 3;
        ID3D11DeviceContext_CopySubresourceRegion(c->ctx, (ID3D11Resource *)d, 0, 0, 0, 0, (ID3D11Resource *)s, 0, &box);
        box.top = 0; box.bottom = 4; box.front = 1; box.back = 1;
        ID3D11DeviceContext_CopySubresourceRegion(c->ctx, (ID3D11Resource *)d, 0, 0, 0, 0, (ID3D11Resource *)s, 0, &box);
        exp = tx_patbuf(64, 500);
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)d, 0, &im))) { tx_cmp_img(&im, exp, 0, "empty source box copies nothing", __FILE__, __LINE__); tx_img_free(&im); }
        free(exp);
    }
    SAFE_RELEASE(s); SAFE_RELEASE(d);
    tx_init_free(&si); tx_init_free(&di);
}

/* CopyResource of whole resources, all subresources. */
static void tx_copyres_case(tx_ctx *c, int dim, UINT w, UINT h, UINT d, UINT mips, UINT arr, DXGI_FORMAT f, D3D11_USAGE su, UINT sbind, UINT scpu, const char *file, int line) {
    tx_init si; ID3D11Resource *s = NULL, *dst = NULL; HRESULT hr; UINT n, mm = w > h ? w : h;
    if (d > mm) mm = d;
    n = (mips ? mips : tx_full_mips(mm)) * arr;
    CHECK(tx_init_make(&si, w, h, d, mips, arr, f, 0, 0, 600));
    hr = tx_mk_dim(c, dim, w, h, d, mips, arr, f, su, sbind, scpu, 0, si.sd, &s);
    t_check(hr == S_OK, file, line, "create source", "%s", d3dh_hr_name(hr));
    hr = tx_mk_dim(c, dim, w, h, d, mips, arr, f, U_DEF, 0, 0, 0, NULL, &dst);
    t_check(hr == S_OK, file, line, "create destination", "%s", d3dh_hr_name(hr));
    if (s && dst) {
        ID3D11DeviceContext_CopyResource(c->ctx, dst, s);
        tx_img im; size_t bad;
        for (UINT i = 0; i < n; i++) {
            hr = tx_fetch(c, dst, i, &im);
            t_check(hr == S_OK, file, line, "fetch", "sub %u: %s", i, d3dh_hr_name(hr));
            if (SUCCEEDED(hr)) { t_check(tx_img_is_pat(&im, 600 + i, &bad), file, line, "CopyResource content", "sub %u mismatch at %u", i, (unsigned)bad); tx_img_free(&im); }
        }
    }
    SAFE_RELEASE(s); SAFE_RELEASE(dst);
    tx_init_free(&si);
}
#define CPYRES(nm, DIM, W, H, D, M, A, F, U, B, CP) DEVTEST(d3d11_textures_copyresource_##nm) { tx_copyres_case(c, DIM, W, H, D, M, A, F, U, B, CP, __FILE__, __LINE__); }
CPYRES(2d_default, 2, 8, 8, 1, 1, 1, F_RGBA8, U_DEF, 0, 0)
CPYRES(2d_chain_arr, 2, 8, 8, 1, 0, 3, F_RGBA8, U_DEF, 0, 0)
CPYRES(2d_from_immutable, 2, 8, 8, 1, 0, 2, F_RGBA8, U_IMM, B_SRV, 0)
CPYRES(2d_from_staging, 2, 8, 8, 1, 0, 2, F_RGBA8, U_STG, 0, CPU_R)
CPYRES(2d_from_dynamic, 2, 8, 8, 1, 1, 1, F_RGBA8, U_DYN, B_SRV, CPU_W)
CPYRES(2d_bc1_chain, 2, 16, 16, 1, 0, 1, DXGI_FORMAT_BC1_UNORM, U_DEF, 0, 0)
CPYRES(2d_bc7_chain_arr, 2, 8, 8, 1, 0, 2, DXGI_FORMAT_BC7_UNORM, U_IMM, B_SRV, 0)
CPYRES(2d_r32g32b32a32, 2, 5, 5, 1, 0, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, U_DEF, 0, 0)
CPYRES(1d_chain_arr, 1, 16, 1, 1, 0, 3, F_RGBA8, U_DEF, 0, 0)
CPYRES(3d_chain, 3, 8, 4, 2, 0, 1, F_RGBA8, U_DEF, 0, 0)
CPYRES(3d_bc1, 3, 8, 8, 4, 0, 1, DXGI_FORMAT_BC1_UNORM, U_IMM, B_SRV, 0)
