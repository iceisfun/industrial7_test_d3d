// Pixel shader using interpolation modifiers, SV_PrimitiveID, SV_IsFrontFace and clip(). Entry: main
struct PSIn {
    float4 pos : SV_Position;
    nointerpolation float4 flat_c : COLOR0;
    noperspective float2 np : TEXCOORD0;
    centroid float2 cen : TEXCOORD1;
};
float4 main(PSIn i, uint prim : SV_PrimitiveID, bool front : SV_IsFrontFace) : SV_Target
{
    clip(i.np.x + 1);
    return float4(i.flat_c.rgb * (front ? 1 : 0.5), (prim & 1) ? 1 : 0.5) + float4(i.cen * 0, 0, 0);
}
