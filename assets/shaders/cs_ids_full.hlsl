// Writes group id, group thread id, dispatch thread id and flattened index per thread to a raw UAV u0 (4 uints each). Entry: main
RWByteAddressBuffer outbuf : register(u0);
[numthreads(4, 2, 1)]
void main(uint3 gid : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint3 dtid : SV_DispatchThreadID, uint gidx : SV_GroupIndex)
{
    uint lin = dtid.y * 16 + dtid.x;
    outbuf.Store4(lin * 16, uint4(gid.x | (gid.y << 8), gtid.x | (gtid.y << 8), dtid.x | (dtid.y << 8), gidx));
}
