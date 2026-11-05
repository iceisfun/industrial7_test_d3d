// Colour from constant buffer b0. Entry: main
cbuffer Params : register(b0) { float4 color; };
float4 main(float4 pos : SV_Position) : SV_Target
{
    return color;
}
