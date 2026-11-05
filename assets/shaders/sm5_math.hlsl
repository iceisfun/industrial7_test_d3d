// SM5 math intrinsics showcase. Reads 16 floats from t0 (raw), writes 16 results per thread to u0 (raw). Entry: main
ByteAddressBuffer inbuf : register(t0);
RWByteAddressBuffer outbuf : register(u0);
[numthreads(1, 1, 1)]
void main()
{
    float a = asfloat(inbuf.Load(0)), b = asfloat(inbuf.Load(4)), c = asfloat(inbuf.Load(8));
    float r[16];
    r[0] = sin(a) + cos(b);
    r[1] = sqrt(abs(a)) + rsqrt(abs(b) + 1);
    r[2] = pow(abs(a) + 1, b);
    r[3] = exp2(c) + log2(abs(c) + 1);
    r[4] = frac(a) + floor(b) + ceil(c) + round(a);
    r[5] = saturate(a) + clamp(b, -1, 1) + lerp(a, b, 0.25);
    r[6] = mad(a, b, c) + fmod(a, 2.5) + trunc(b);
    r[7] = smoothstep(0, 1, saturate(a)) + step(a, b);
    r[8] = sign(a) + min(a, b) + max(b, c);
    r[9] = dot(float3(a, b, c), float3(1, 2, 3));
    r[10] = length(float3(a, b, c)) + distance(float2(a, b), float2(c, a));
    r[11] = normalize(float3(a + 2, b, c)).x;
    r[12] = cross(float3(a, b, c), float3(c, a, b)).y;
    r[13] = (float)f16tof32(f32tof16(a));
    r[14] = tanh(a) + atan2(a, b) + sinh(c) + cosh(c);
    r[15] = rcp(abs(a) + 1) + degrees(a) + radians(b);
    [unroll] for (uint i = 0; i < 16; i++)
        outbuf.Store(i * 4, asuint(r[i]));
}
