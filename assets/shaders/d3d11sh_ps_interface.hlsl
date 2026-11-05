// d3d11_shaders corpus: target ps_5_0, entry main
interface iColor { float4 Get(); };
class cRed : iColor { float4 Get() { return float4(1, 0, 0, 1); } };
class cBlue : iColor { float4 Get() { return float4(0, 0, 1, 1); } };
iColor g_color;
cRed g_red;
cBlue g_blue;
float4 main(float4 p : SV_Position) : SV_Target { return g_color.Get(); }
