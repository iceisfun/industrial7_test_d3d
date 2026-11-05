// Writes the flattened dispatch thread id to a typed UAV buffer u0. Entry: main
RWBuffer<uint> outbuf : register(u0);
[numthreads(8, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    outbuf[dtid.x] = dtid.x;
}
