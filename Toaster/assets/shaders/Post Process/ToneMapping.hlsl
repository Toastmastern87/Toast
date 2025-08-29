#type pixel
Texture2D BaseTexture : register(t10);
SamplerState DefaultSampler : register(s1);

#define PI 3.141592653589793
#pragma pack_matrix(row_major)

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

cbuffer DirectionalLight : register(b3)
{
    matrix lightViewProj;
    float4 direction;
    float4 radiance;
    float SunIntensity;
};


// === ACES helpers (unchanged) ==============================================
static const float3x3 ACESInputMat =
{
    { 0.59719, 0.35458, 0.04823 },
    { 0.07600, 0.90834, 0.01566 },
    { 0.02840, 0.13383, 0.83777 }
};

static const float3x3 ACESOutputMat =
{
    { 1.60475, -0.53108, -0.07367 },
    { -0.10208, 1.10813, -0.00605 },
    { -0.00327, -0.07276, 1.07602 }
};

static const float3x3 D65_to_D60 =
{
    { 0.987224, 0.007648, -0.014872 },
    { -0.006113, 1.001864, 0.004249 },
    { 0.015953, -0.019591, 1.003640 }
};

static const float3x3 D60_to_D65 =
{
    { 1.012780, -0.007597, 0.016690 },
    { 0.006019, 0.998132, -0.004117 },
    { -0.016787, 0.019661, 0.996995 }
};

float3 RRTAndODTFit(float3 v)
{
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

// === Pixel shader ===========================================================
float4 main(PixelInputType input) : SV_TARGET
{
    // HDR scene color (linear)
    float4 colorHDR = BaseTexture.Sample(DefaultSampler, input.texCoord);

    // Constant exposure in EV (stops). 2.5 EV -> ~5.657x
    const float ExposureEV = 2.5f - log2(SunIntensity);
    const float exposureMul = exp2(ExposureEV);
    float3 color = colorHDR.rgb * exposureMul;

    // Chromatic adapt sRGB (D65) -> ACES (D60), apply ACES filmic, then adapt back.
    color = mul(D65_to_D60, color);
    color = mul(ACESInputMat, color);
    color = RRTAndODTFit(color);
    color = mul(ACESOutputMat, color);
    color = mul(D60_to_D65, color);

    // Clamp to [0,1] (still linear). No manual sRGB — backbuffer is sRGB.
    color = saturate(max(color, 0.0f));

    return float4(color, colorHDR.a);
}