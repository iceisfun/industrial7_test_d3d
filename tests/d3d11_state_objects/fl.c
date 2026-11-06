/* Feature-level matrix: which state descriptions each feature level accepts (WARP can create devices from 9_1 up). */
#include "so_common.h"

typedef struct { const char *name; D3D_FEATURE_LEVEL fl; } flcase;
static const flcase k_fl[] = { { "9_1", D3D_FEATURE_LEVEL_9_1 }, { "9_2", D3D_FEATURE_LEVEL_9_2 }, { "9_3", D3D_FEATURE_LEVEL_9_3 }, { "10_0", D3D_FEATURE_LEVEL_10_0 }, { "10_1", D3D_FEATURE_LEVEL_10_1 }, { "11_0", D3D_FEATURE_LEVEL_11_0 } };

static ID3D11Device *mk_dev(D3D_FEATURE_LEVEL fl, ID3D11DeviceContext **ctx, D3D_FEATURE_LEVEL *got) {
    ID3D11Device *d = NULL;
    HRESULT hr = U(D3D11CreateDevice)(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, &fl, 1, D3D11_SDK_VERSION, &d, got, ctx);
    return SUCCEEDED(hr) ? d : NULL;
}

/* Expected result per probe, 'A' = accepted (validate-only call gives S_FALSE), 'I' = E_INVALIDARG, 'M' = E_OUTOFMEMORY.
 * Probe order: independent blend, alpha-to-coverage, dual-source factor, anisotropic x16, anisotropic x2, comparison filter, MIRROR_ONCE, BORDER addressing,
 * MinLOD/MaxLOD/MipLODBias, DepthClipEnable FALSE, DepthBiasClamp + slope bias, wireframe, antialiased line, stencil. Then the queries EVENT, OCCLUSION, TIMESTAMP,
 * TIMESTAMP_DISJOINT, PIPELINE_STATISTICS, OCCLUSION_PREDICATE, SO_STATISTICS, SO_OVERFLOW_PREDICATE, SO_STATISTICS_STREAM0, SO_OVERFLOW_PREDICATE_STREAM3. */
static const char *k_exp[] = {
    "IIIIAIIIIMMAAA" "AIAAIIIIII",   /* 9_1 */
    "IIIAAIAIIMMAAA" "AAAAIIIIII",   /* 9_2 */
    "IIIAAIAAIMMAAA" "AAAAIIIIII",   /* 9_3 */
    "AAAAAAAAAAAAAA" "AAAAAAAAII",   /* 10_0 */
    "AAAAAAAAAAAAAA" "AAAAAAAAII",   /* 10_1 */
    "AAAAAAAAAAAAAA" "AAAAAAAAAA",   /* 11_0 */
};
static char code(HRESULT h) { return h == S_FALSE ? 'A' : h == E_INVALIDARG ? 'I' : h == E_OUTOFMEMORY ? 'M' : '?'; }

ST(fl_state_description_matrix) {
    unsigned k;
    SO_NEED();
    for (k = 0; k < sizeof k_fl / sizeof k_fl[0]; k++) {
        ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL got = 0; ID3D11Device *dev = mk_dev(k_fl[k].fl, &ctx, &got);
        char r[40]; int p = 0; HRESULT h;
        if (!dev) { CHECKF(0, "WARP device at feature level %s not created", k_fl[k].name); continue; }
        EXPECT_EQ(got, k_fl[k].fl);
        { D3D11_BLEND_DESC d = so_blend_default(); d.IndependentBlendEnable = TRUE; d.RenderTarget[1].BlendEnable = TRUE; r[p++] = code(ID3D11Device_CreateBlendState(dev, &d, NULL)); }
        { D3D11_BLEND_DESC d = so_blend_default(); d.AlphaToCoverageEnable = TRUE; r[p++] = code(ID3D11Device_CreateBlendState(dev, &d, NULL)); }
        { D3D11_BLEND_DESC d = so_blend_default(); d.RenderTarget[0].BlendEnable = TRUE; d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC1_COLOR; r[p++] = code(ID3D11Device_CreateBlendState(dev, &d, NULL)); }
        { D3D11_SAMPLER_DESC d = so_samp_default(); d.Filter = D3D11_FILTER_ANISOTROPIC; d.MaxAnisotropy = 16; r[p++] = code(ID3D11Device_CreateSamplerState(dev, &d, NULL)); }
        { D3D11_SAMPLER_DESC d = so_samp_default(); d.Filter = D3D11_FILTER_ANISOTROPIC; d.MaxAnisotropy = 2; r[p++] = code(ID3D11Device_CreateSamplerState(dev, &d, NULL)); }
        { D3D11_SAMPLER_DESC d = so_samp_default(); d.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; d.ComparisonFunc = D3D11_COMPARISON_LESS; r[p++] = code(ID3D11Device_CreateSamplerState(dev, &d, NULL)); }
        { D3D11_SAMPLER_DESC d = so_samp_default(); d.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR_ONCE; r[p++] = code(ID3D11Device_CreateSamplerState(dev, &d, NULL)); }
        { D3D11_SAMPLER_DESC d = so_samp_default(); d.AddressU = D3D11_TEXTURE_ADDRESS_BORDER; d.BorderColor[0] = .5f; r[p++] = code(ID3D11Device_CreateSamplerState(dev, &d, NULL)); }
        { D3D11_SAMPLER_DESC d = so_samp_default(); d.MinLOD = 1; d.MaxLOD = 5; d.MipLODBias = 2; r[p++] = code(ID3D11Device_CreateSamplerState(dev, &d, NULL)); }
        { D3D11_RASTERIZER_DESC d = so_rast_default(); d.DepthClipEnable = FALSE; r[p++] = code(ID3D11Device_CreateRasterizerState(dev, &d, NULL)); }
        { D3D11_RASTERIZER_DESC d = so_rast_default(); d.SlopeScaledDepthBias = 1; d.DepthBiasClamp = 0.5f; r[p++] = code(ID3D11Device_CreateRasterizerState(dev, &d, NULL)); }
        { D3D11_RASTERIZER_DESC d = so_rast_default(); d.FillMode = D3D11_FILL_WIREFRAME; r[p++] = code(ID3D11Device_CreateRasterizerState(dev, &d, NULL)); }
        { D3D11_RASTERIZER_DESC d = so_rast_default(); d.AntialiasedLineEnable = TRUE; r[p++] = code(ID3D11Device_CreateRasterizerState(dev, &d, NULL)); }
        { D3D11_DEPTH_STENCIL_DESC d = so_ds_default(); d.StencilEnable = TRUE; r[p++] = code(ID3D11Device_CreateDepthStencilState(dev, &d, NULL)); }
        { static const D3D11_QUERY qs[] = { D3D11_QUERY_EVENT, D3D11_QUERY_OCCLUSION, D3D11_QUERY_TIMESTAMP, D3D11_QUERY_TIMESTAMP_DISJOINT, D3D11_QUERY_PIPELINE_STATISTICS, D3D11_QUERY_OCCLUSION_PREDICATE,
                                         D3D11_QUERY_SO_STATISTICS, D3D11_QUERY_SO_OVERFLOW_PREDICATE, D3D11_QUERY_SO_STATISTICS_STREAM0, D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3 };
          unsigned q; for (q = 0; q < 10; q++) { D3D11_QUERY_DESC d; d.Query = qs[q]; d.MiscFlags = 0; h = ID3D11Device_CreateQuery(dev, &d, NULL); r[p++] = code(h); } }
        r[p] = 0;
        t_check(!strcmp(r, k_exp[k]), __FILE__, __LINE__, "feature level matrix", "FL %s: got %s want %s", k_fl[k].name, r, k_exp[k]);
        ID3D11DeviceContext_Release(ctx); ID3D11Device_Release(dev);
    }
}
