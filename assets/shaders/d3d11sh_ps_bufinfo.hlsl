// d3d11_shaders corpus: target ps_5_0, entry main
Texture2D<float4> tex : register(t0);
float4 main(float4 p : SV_Position) : SV_Target { uint w, h, m; tex.GetDimensions(0, w, h, m); return float4((float)w, (float)h, (float)m, 1); }
