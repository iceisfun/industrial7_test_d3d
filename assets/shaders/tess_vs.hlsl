// Control-point passthrough vertex shader for tessellation. Entry: main
struct CP { float3 pos : POSITION; };
CP main(CP i) { return i; }
