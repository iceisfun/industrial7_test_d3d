/* ID3D11Device3 *View1 creators (plane slices) and video views. Missing interfaces yield skips. */
#include "views.h"

static ID3D11Device3 *get_dev3(ID3D11Device *dev)
{
    ID3D11Device3 *d3 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(dev, &IID_ID3D11Device3, (void **)&d3))) return NULL;
    return d3;
}
#define NEED_DEV3() ID3D11Device3 *d3 = get_dev3(dev); if (!d3) { DONE(); t_skip("ID3D11Device3 unavailable"); return; }

TEST(d3d11_views_srv1_null_desc_matches_srv)
{
    NEED_DEV(); NEED_DEV3();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    ID3D11ShaderResourceView1 *v1 = NULL; ID3D11ShaderResourceView *v0 = NULL;
    EXPECT_HR(ID3D11Device3_CreateShaderResourceView1(d3, R(t), NULL, &v1), S_OK);
    EXPECT_HR(MK_SRV(dev, t, NULL, &v0), S_OK);
    REQUIRE(v1); REQUIRE(v0);
    D3D11_SHADER_RESOURCE_VIEW_DESC1 d1; D3D11_SHADER_RESOURCE_VIEW_DESC d0;
    memset(&d1, 0xCD, sizeof d1);
    ID3D11ShaderResourceView1_GetDesc1(v1, &d1);
    ID3D11ShaderResourceView_GetDesc(v0, &d0);
    EXPECT_EQ(d1.Format, d0.Format);
    EXPECT_EQ(d1.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE2DARRAY);
    EXPECT_EQ(d1.Texture2DArray.MostDetailedMip, 0);
    EXPECT_EQ(d1.Texture2DArray.MipLevels, d0.Texture2DArray.MipLevels);
    EXPECT_EQ(d1.Texture2DArray.FirstArraySlice, 0);
    EXPECT_EQ(d1.Texture2DArray.ArraySize, 2);
    EXPECT_EQ(d1.Texture2DArray.PlaneSlice, 0);
    SAFE_RELEASE(v0); SAFE_RELEASE(v1); SAFE_RELEASE(t); SAFE_RELEASE(d3); DONE();
}
TEST(d3d11_views_srv1_plane_slice_on_non_planar_fails)
{
    NEED_DEV(); NEED_DEV3();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC1 d; memset(&d, 0, sizeof d);
    d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    d.Texture2D.MipLevels = 1; d.Texture2D.PlaneSlice = 0;
    ID3D11ShaderResourceView1 *v = NULL;
    EXPECT_HR(ID3D11Device3_CreateShaderResourceView1(d3, R(t), &d, &v), S_OK);
    SAFE_RELEASE(v);
    d.Texture2D.PlaneSlice = 1;
    EXPECT_HR(ID3D11Device3_CreateShaderResourceView1(d3, R(t), &d, &v), E_INV);
    CHECK(v == NULL);
    EXPECT_HR(ID3D11Device3_CreateShaderResourceView1(d3, R(t), &d, NULL), E_INV);
    d.Texture2D.PlaneSlice = 0;
    EXPECT_HR(ID3D11Device3_CreateShaderResourceView1(d3, R(t), &d, NULL), S_FALSE);
    SAFE_RELEASE(t); SAFE_RELEASE(d3); DONE();
}
TEST(d3d11_views_srv1_requires_bind_flag)
{
    NEED_DEV(); NEED_DEV3();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    ID3D11ShaderResourceView1 *v = NULL;
    EXPECT_HR(ID3D11Device3_CreateShaderResourceView1(d3, R(t), NULL, &v), E_INV);
    CHECK(v == NULL);
    SAFE_RELEASE(t); SAFE_RELEASE(d3); DONE();
}
TEST(d3d11_views_srv1_qi_to_srv_same_object)
{
    NEED_DEV(); NEED_DEV3();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    ID3D11ShaderResourceView1 *v1 = NULL; ID3D11ShaderResourceView *v0 = NULL;
    EXPECT_HR(ID3D11Device3_CreateShaderResourceView1(d3, R(t), NULL, &v1), S_OK);
    REQUIRE(v1);
    EXPECT_HR(ID3D11ShaderResourceView1_QueryInterface(v1, &IID_ID3D11ShaderResourceView, (void **)&v0), S_OK);
    EXPECT_EQ(v0, v1);
    SAFE_RELEASE(v0); SAFE_RELEASE(v1); SAFE_RELEASE(t); SAFE_RELEASE(d3); DONE();
}
TEST(d3d11_views_srv_legacy_view_supports_srv1_interface)
{
    NEED_DEV(); NEED_DEV3();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    ID3D11ShaderResourceView *v0 = NULL; ID3D11ShaderResourceView1 *v1 = NULL;
    EXPECT_HR(MK_SRV(dev, t, NULL, &v0), S_OK);
    REQUIRE(v0);
    HRESULT hr = ID3D11ShaderResourceView_QueryInterface(v0, &IID_ID3D11ShaderResourceView1, (void **)&v1);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC1 d; memset(&d, 0xCD, sizeof d);
        ID3D11ShaderResourceView1_GetDesc1(v1, &d);
        EXPECT_EQ(d.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE2D);
        EXPECT_EQ(d.Texture2D.PlaneSlice, 0);
        SAFE_RELEASE(v1);
    }
    SAFE_RELEASE(v0); SAFE_RELEASE(t); SAFE_RELEASE(d3); DONE();
}
TEST(d3d11_views_rtv1_plane_slice_rules)
{
    NEED_DEV(); NEED_DEV3();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 2, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    ID3D11RenderTargetView1 *v = NULL;
    D3D11_RENDER_TARGET_VIEW_DESC1 d; memset(&d, 0, sizeof d);
    d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; d.Texture2D.MipSlice = 1; d.Texture2D.PlaneSlice = 0;
    EXPECT_HR(ID3D11Device3_CreateRenderTargetView1(d3, R(t), &d, &v), S_OK);
    if (v) {
        D3D11_RENDER_TARGET_VIEW_DESC1 o; memset(&o, 0xCD, sizeof o);
        ID3D11RenderTargetView1_GetDesc1(v, &o);
        EXPECT_EQ(o.Texture2D.MipSlice, 1); EXPECT_EQ(o.Texture2D.PlaneSlice, 0); EXPECT_EQ(o.ViewDimension, D3D11_RTV_DIMENSION_TEXTURE2D);
        SAFE_RELEASE(v);
    }
    d.Texture2D.PlaneSlice = 1;
    EXPECT_HR(ID3D11Device3_CreateRenderTargetView1(d3, R(t), &d, &v), E_INV);
    CHECK(v == NULL);
    EXPECT_HR(ID3D11Device3_CreateRenderTargetView1(d3, R(t), NULL, NULL), S_FALSE);
    SAFE_RELEASE(t); SAFE_RELEASE(d3); DONE();
}
TEST(d3d11_views_rtv1_null_desc)
{
    NEED_DEV(); NEED_DEV3();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 3, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    ID3D11RenderTargetView1 *v = NULL;
    EXPECT_HR(ID3D11Device3_CreateRenderTargetView1(d3, R(t), NULL, &v), S_OK);
    REQUIRE(v);
    D3D11_RENDER_TARGET_VIEW_DESC1 o; memset(&o, 0xCD, sizeof o);
    ID3D11RenderTargetView1_GetDesc1(v, &o);
    EXPECT_EQ(o.ViewDimension, D3D11_RTV_DIMENSION_TEXTURE2DARRAY);
    EXPECT_EQ(o.Texture2DArray.ArraySize, 3);
    EXPECT_EQ(o.Texture2DArray.PlaneSlice, 0);
    SAFE_RELEASE(v); SAFE_RELEASE(t); SAFE_RELEASE(d3); DONE();
}
TEST(d3d11_views_uav1_plane_slice_rules)
{
    NEED_DEV(); NEED_DEV3(); NEED_FL11();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 2, 1, DXGI_FORMAT_R32_UINT, D3D11_BIND_UNORDERED_ACCESS, 0, 1);
    REQUIRE(t);
    ID3D11UnorderedAccessView1 *v = NULL;
    D3D11_UNORDERED_ACCESS_VIEW_DESC1 d; memset(&d, 0, sizeof d);
    d.Format = DXGI_FORMAT_R32_UINT; d.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D; d.Texture2D.MipSlice = 1;
    EXPECT_HR(ID3D11Device3_CreateUnorderedAccessView1(d3, R(t), &d, &v), S_OK);
    if (v) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC1 o; memset(&o, 0xCD, sizeof o);
        ID3D11UnorderedAccessView1_GetDesc1(v, &o);
        EXPECT_EQ(o.Texture2D.MipSlice, 1); EXPECT_EQ(o.Texture2D.PlaneSlice, 0);
        SAFE_RELEASE(v);
    }
    d.Texture2D.PlaneSlice = 1;
    EXPECT_HR(ID3D11Device3_CreateUnorderedAccessView1(d3, R(t), &d, &v), E_INV);
    CHECK(v == NULL);
    d.Texture2D.PlaneSlice = 0; d.Texture2D.MipSlice = 2;
    EXPECT_HR(ID3D11Device3_CreateUnorderedAccessView1(d3, R(t), &d, &v), E_INV);
    SAFE_RELEASE(t); SAFE_RELEASE(d3); DONE();
}
TEST(d3d11_views_uav1_buffer_roundtrip)
{
    NEED_DEV(); NEED_DEV3(); NEED_FL11();
    ID3D11Buffer *b = mkbuf(dev, 256, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 16);
    REQUIRE(b);
    D3D11_UNORDERED_ACCESS_VIEW_DESC1 d; memset(&d, 0, sizeof d);
    d.Format = DXGI_FORMAT_UNKNOWN; d.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; d.Buffer.FirstElement = 2; d.Buffer.NumElements = 10; d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
    ID3D11UnorderedAccessView1 *v = NULL;
    EXPECT_HR(ID3D11Device3_CreateUnorderedAccessView1(d3, R(b), &d, &v), S_OK);
    REQUIRE(v);
    D3D11_UNORDERED_ACCESS_VIEW_DESC1 o; memset(&o, 0xCD, sizeof o);
    ID3D11UnorderedAccessView1_GetDesc1(v, &o);
    EXPECT_EQ(o.ViewDimension, D3D11_UAV_DIMENSION_BUFFER);
    EXPECT_EQ(o.Buffer.FirstElement, 2); EXPECT_EQ(o.Buffer.NumElements, 10); EXPECT_EQ(o.Buffer.Flags, D3D11_BUFFER_UAV_FLAG_APPEND);
    SAFE_RELEASE(v); SAFE_RELEASE(b); SAFE_RELEASE(d3); DONE();
}
TEST(d3d11_views_srv1_nv12_planes)
{
    NEED_DEV(); NEED_DEV3();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_NV12, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    if (!t) { SAFE_RELEASE(d3); DONE(); t_skip("NV12 shader-resource texture not supported here"); return; }
    D3D11_SHADER_RESOURCE_VIEW_DESC1 d; memset(&d, 0, sizeof d);
    d.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; d.Texture2D.MipLevels = 1;
    ID3D11ShaderResourceView1 *v = NULL; HRESULT hr;
    d.Format = DXGI_FORMAT_R8_UNORM; d.Texture2D.PlaneSlice = 0;
    hr = ID3D11Device3_CreateShaderResourceView1(d3, R(t), &d, &v);
    EXPECT_HR(hr, S_OK);
    SAFE_RELEASE(v);
    d.Format = DXGI_FORMAT_R8G8_UNORM; d.Texture2D.PlaneSlice = 1;
    hr = ID3D11Device3_CreateShaderResourceView1(d3, R(t), &d, &v);
    EXPECT_HR(hr, S_OK);
    SAFE_RELEASE(v);
    d.Format = DXGI_FORMAT_R8G8_UNORM; d.Texture2D.PlaneSlice = 2;
    hr = ID3D11Device3_CreateShaderResourceView1(d3, R(t), &d, &v);
    EXPECT_HR(hr, E_INV);     /* NV12 has two planes */
    SAFE_RELEASE(v); SAFE_RELEASE(t); SAFE_RELEASE(d3); DONE();
}

/* ---- video views: existence + argument validation -------------------------------------- */
static ID3D11VideoDevice *get_vdev(ID3D11Device *dev)
{
    ID3D11VideoDevice *vd = NULL;
    if (FAILED(ID3D11Device_QueryInterface(dev, &IID_ID3D11VideoDevice, (void **)&vd))) return NULL;
    return vd;
}
#define NEED_VDEV() ID3D11VideoDevice *vd = get_vdev(dev); if (!vd) { DONE(); t_skip("ID3D11VideoDevice: QueryInterface on the device returns E_NOINTERFACE on both WARP and the hardware-type Basic Render Driver (Win10 22H2 VM)"); return; }

TEST(d3d11_views_video_device_view_methods_exist)
{
    NEED_DEV(); NEED_VDEV();
    CHECK(vd->lpVtbl->CreateVideoDecoderOutputView != NULL);
    CHECK(vd->lpVtbl->CreateVideoProcessorInputView != NULL);
    CHECK(vd->lpVtbl->CreateVideoProcessorOutputView != NULL);
    SAFE_RELEASE(vd); DONE();
}
TEST(d3d11_views_video_decoder_output_view_null_args)
{
    NEED_DEV(); NEED_VDEV();
    ID3D11VideoDecoderOutputView *v = NULL;
    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC d; memset(&d, 0, sizeof d);
    d.DecodeProfile = D3D11_DECODER_PROFILE_H264_VLD_NOFGT; d.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
    HRESULT hr = ID3D11VideoDevice_CreateVideoDecoderOutputView(vd, NULL, &d, &v);
    EXPECT_HR_FAILED(hr);
    CHECK(v == NULL);
    hr = ID3D11VideoDevice_CreateVideoDecoderOutputView(vd, NULL, NULL, NULL);
    EXPECT_HR_FAILED(hr);
    SAFE_RELEASE(vd); DONE();
}
TEST(d3d11_views_video_decoder_output_view_bad_texture)
{
    NEED_DEV(); NEED_VDEV();
    /* A plain render-target texture lacks BIND_DECODER: no decoder output view can be made from it. */
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC d; memset(&d, 0, sizeof d);
    d.DecodeProfile = D3D11_DECODER_PROFILE_H264_VLD_NOFGT; d.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
    ID3D11VideoDecoderOutputView *v = NULL;
    HRESULT hr = ID3D11VideoDevice_CreateVideoDecoderOutputView(vd, R(t), &d, &v);
    EXPECT_HR_FAILED(hr);
    CHECK(v == NULL);
    SAFE_RELEASE(t); SAFE_RELEASE(vd); DONE();
}
TEST(d3d11_views_video_processor_views_null_and_unknown_dimension)
{
    NEED_DEV(); NEED_VDEV();
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC cd; memset(&cd, 0, sizeof cd);
    cd.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE; cd.InputWidth = cd.OutputWidth = 64; cd.InputHeight = cd.OutputHeight = 64;
    cd.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
    ID3D11VideoProcessorEnumerator *en = NULL; ID3D11VideoProcessor *vp = NULL;
    HRESULT hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(vd, &cd, &en);
    if (FAILED(hr) || !en) { SAFE_RELEASE(vd); DONE(); t_skip("no video processor enumerator (%s)", d3dh_hr_name(hr)); return; }
    ID3D11Texture2D *t = mk2d(dev, 64, 64, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    ID3D11VideoProcessorInputView *iv = NULL; ID3D11VideoProcessorOutputView *ov = NULL;
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC id; memset(&id, 0, sizeof id);
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC od; memset(&od, 0, sizeof od);
    id.ViewDimension = D3D11_VPIV_DIMENSION_UNKNOWN; od.ViewDimension = D3D11_VPOV_DIMENSION_UNKNOWN;
    hr = ID3D11VideoDevice_CreateVideoProcessorInputView(vd, R(t), en, &id, &iv);
    EXPECT_HR_FAILED(hr); CHECK(iv == NULL);
    hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(vd, R(t), en, &od, &ov);
    EXPECT_HR_FAILED(hr); CHECK(ov == NULL);
    hr = ID3D11VideoDevice_CreateVideoProcessorInputView(vd, NULL, en, &id, &iv);
    EXPECT_HR_FAILED(hr);
    hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(vd, NULL, en, &od, &ov);
    EXPECT_HR_FAILED(hr);
    SAFE_RELEASE(t); SAFE_RELEASE(en); SAFE_RELEASE(vd); DONE();
}
TEST(d3d11_views_video_processor_views_roundtrip_when_supported)
{
    NEED_DEV(); NEED_VDEV();
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC cd; memset(&cd, 0, sizeof cd);
    cd.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE; cd.InputWidth = cd.OutputWidth = 64; cd.InputHeight = cd.OutputHeight = 64;
    cd.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
    ID3D11VideoProcessorEnumerator *en = NULL;
    HRESULT hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(vd, &cd, &en);
    if (FAILED(hr) || !en) { SAFE_RELEASE(vd); DONE(); t_skip("no video processor enumerator (%s)", d3dh_hr_name(hr)); return; }
    ID3D11Texture2D *t = mk2d(dev, 64, 64, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    ID3D11VideoProcessorInputView *iv = NULL; ID3D11VideoProcessorOutputView *ov = NULL;
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC id; memset(&id, 0, sizeof id);
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC od; memset(&od, 0, sizeof od);
    id.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D; od.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    hr = ID3D11VideoDevice_CreateVideoProcessorInputView(vd, R(t), en, &id, &iv);
    t_note("video processor input view on R8G8B8A8 -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && iv) {
        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC o; memset(&o, 0xCD, sizeof o);
        ID3D11VideoProcessorInputView_GetDesc(iv, &o);
        EXPECT_EQ(o.ViewDimension, D3D11_VPIV_DIMENSION_TEXTURE2D);
        EXPECT_EQ(o.Texture2D.MipSlice, 0); EXPECT_EQ(o.Texture2D.ArraySlice, 0);
        ID3D11Resource *g = NULL; ID3D11VideoProcessorInputView_GetResource(iv, &g);
        EXPECT_EQ(g, t); SAFE_RELEASE(g);
    }
    SAFE_RELEASE(iv);
    hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(vd, R(t), en, &od, &ov);
    t_note("video processor output view on R8G8B8A8 -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && ov) {
        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC o; memset(&o, 0xCD, sizeof o);
        ID3D11VideoProcessorOutputView_GetDesc(ov, &o);
        EXPECT_EQ(o.ViewDimension, D3D11_VPOV_DIMENSION_TEXTURE2D);
        EXPECT_EQ(o.Texture2D.MipSlice, 0);
        ID3D11Resource *g = NULL; ID3D11VideoProcessorOutputView_GetResource(ov, &g);
        EXPECT_EQ(g, t); SAFE_RELEASE(g);
    }
    SAFE_RELEASE(ov);
    SAFE_RELEASE(t); SAFE_RELEASE(en); SAFE_RELEASE(vd); DONE();
}
