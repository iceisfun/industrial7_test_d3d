// SM5 integer / bit-manipulation intrinsics. Reads 4 uints from t0 (raw), writes results to u0 (raw). Entry: main
ByteAddressBuffer inbuf : register(t0);
RWByteAddressBuffer outbuf : register(u0);
[numthreads(1, 1, 1)]
void main()
{
    uint x = inbuf.Load(0), y = inbuf.Load(4);
    int s = asint(inbuf.Load(8));
    uint r[12];
    r[0] = countbits(x);
    r[1] = firstbithigh(x);
    r[2] = firstbitlow(x);
    r[3] = reversebits(x);
    r[4] = asuint(firstbithigh(s));
    r[5] = x & y; r[6] = x | y; r[7] = x ^ y; r[8] = ~x;
    r[9] = (x << (y & 31)) + (x >> (y & 31));
    r[10] = asuint(s / 3) + asuint(s % 7);
    r[11] = (x * y) + mad(x, y, 1u);
    [unroll] for (uint i = 0; i < 12; i++)
        outbuf.Store(i * 4, r[i]);
}
