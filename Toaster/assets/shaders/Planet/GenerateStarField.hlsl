#inputlayout
#type compute

static const float PI = 3.14159265359f;

struct Star
{
    float3 Dir;
    float Lum;
    float3 RGB;
    float _pad;
};

RWStructuredBuffer<Star> StarBuf : register(u0);

cbuffer StarFieldSettingsCB : register(b0)
{
    uint StarCount;
    float BrightnessMin;
    float BrightnessMax;
    float TemperatureMin;
    float TemperatureMax;
    float Seed;
};

uint pcg_hash(uint v, uint seed)
{
    uint state = v + seed * 747796405u; // pcg set-seq step
    state ^= state >> 17;
    state *= 0xed5ad4bbu;
    state ^= state >> 11;
    state *= 0xac4c1b51u;
    state ^= state >> 15;
    state *= 0x31848babu;
    state ^= state >> 14;
    return state;
}

float rand(inout uint state)
{
    state = pcg_hash(state, 0u);
    return (state & 0x00FFFFFFu) * (1.0f / 16777216.0f); // 24-bit mantissa
}

float2 rand2(inout uint state)
{
    return float2(rand(state), rand(state));
}

float3 KelvinToRGB(float K)
{
    K = clamp(K, 1000.0f, 40000.0f);

    // polynomials are pre-scaled so  D65 (6500 K) ≈ 1,1,1
    float3 c;
    float t = K;

    // ---- Red ---------------------------------------------------------
    if (K < 6600.0f)
        c.r = 1.0f;
    else
        c.r = 1.292936186f * pow(t / 100.0f - 60.0f, -0.1332047592f);

    // ---- Green -------------------------------------------------------
    if (K < 6600.0f)
        c.g = 0.390081578f * log(t / 100.0f) - 0.631841444f;
    else
        c.g = 1.129890860f * pow(t / 100.0f - 60.0f, -0.0755148492f);

    // ---- Blue --------------------------------------------------------
    if (K > 6600.0f)
        c.b = 1.0f;
    else if (K < 1900.0f)
        c.b = 0.0f;
    else
        c.b = 0.543206789f * log(t / 100.0f - 10.0f) - 1.196254089f;

    return saturate(c); // keep values in 0-1
}

float TemperatureSample(float xi)
{
    float k = pow(xi, 1.8f); // 0.35-0.4 ≈ Salpeter IMF slope 
    return lerp(TemperatureMin, TemperatureMax, k);
}

[numthreads(256, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
    if (id >= StarCount)
        return;

    uint state = pcg_hash(id, Seed);

    // --- Position on unit sphere (uniform) --------------------
    float2 u = rand2(state); // -> [0,1)²
    float z = 1 - 2 * u.x; // cosθ
    float a = 2 * PI * u.y; // φ
    float r = sqrt(max(0.0, 1 - z * z));
    float3 dir = float3(r * cos(a), r * sin(a), z);

    // --- Brightness (log-uniform) -----------------------------
    float L = exp(lerp(log(BrightnessMin), log(BrightnessMax), rand(state)));

    // --- Temperature -----------------------------------------
    float T = TemperatureSample(rand(state)); // 2 500–9 500 K
    float3 rgb = KelvinToRGB(T) * L;
    
    Star v;
    v.Dir = dir;
    v.Lum = L;
    v.RGB = rgb;
    v._pad = 0.0f;
    StarBuf[id] = v; 
}