// Atomic operations on UAV u0 (raw) and groupshared memory. 64 threads each add 1 and OR their bit. Entry: main
RWByteAddressBuffer outbuf : register(u0);
groupshared uint gs_counter;
[numthreads(64, 1, 1)]
void main(uint gi : SV_GroupIndex)
{
    if (gi == 0) gs_counter = 0;
    GroupMemoryBarrierWithGroupSync();
    uint old;
    InterlockedAdd(gs_counter, 1, old);
    GroupMemoryBarrierWithGroupSync();
    uint dummy;
    outbuf.InterlockedAdd(0, 1, dummy);
    outbuf.InterlockedOr(4, 1u << (gi & 31), dummy);
    outbuf.InterlockedMax(8, gi, dummy);
    outbuf.InterlockedMin(12, gi + 1, dummy);
    if (gi == 0) outbuf.Store(16, gs_counter);
}
