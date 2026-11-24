#inputlayout
#type compute

// Computes diffuse irradiance cubemap convolution for image-based lighting.
// Uses quasi Monte Carlo sampling with Hammersley sequence.

static const float PI = 3.141592;
static const float TwoPI = 2 * PI;
static const float Epsilon = 0.00001;

// Cosine-weighted + filtered importance sampling from the mipped source cube; 128 samples cover the hemisphere
static const uint NumSamples = 128;
static const float InvNumSamples = 1.0f / float(NumSamples);

cbuffer SpecularMapFilterSettings : register(b5)
{
	// Roughness value to pre-filter for.
    float roughness;
    uint faceIndex;
};

TextureCube inputTexture : register(t15);

RWTexture2DArray<float4> outputTexture : register(u0);

SamplerState defaultSampler : register(s0);

// Compute Van der Corput radical inverse
// See: http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Sample i-th point from Hammersley point set of NumSamples points total.
float2 SampleHammersley(uint i)
{
	return float2(i * InvNumSamples, RadicalInverse_VdC(i));
}

// Cosine-weighted direction on the hemisphere around +Z. pdf = cosTheta / PI.
float3 SampleCosineHemisphere(float u1, float u2)
{
    const float r = sqrt(u1);
    const float phi = TwoPI * u2;
    return float3(r * cos(phi), r * sin(phi), sqrt(max(0.0f, 1.0f - u1)));
}

// Calculate normalized sampling direction vector based on current fragment coordinates.
// This is essentially "inverse-sampling": we reconstruct what the sampling vector would be if we wanted it to "hit"
// this particular fragment in a cubemap.
float3 GetSamplingVector(uint3 ThreadID)
{
    float outputWidth, outputHeight, outputDepth;
    outputTexture.GetDimensions(outputWidth, outputHeight, outputDepth);

    float2 st = (ThreadID.xy + 0.5f) / float2(outputWidth, outputHeight);
    float2 uv = 2.0 * float2(st.x, 1.0 - st.y) - 1.0;

    // Select vector based on cubemap face index from the constant buffer.
    float3 ret;
    switch (faceIndex)
    {
        case 0: ret = float3(1.0, uv.y, -uv.x);  break;
        case 1: ret = float3(-1.0, uv.y, uv.x);  break;
        case 2: ret = float3(uv.x, 1.0, -uv.y);  break;
        case 3: ret = float3(uv.x, -1.0, uv.y);  break;
        case 4: ret = float3(uv.x, uv.y, 1.0);   break;
        case 5: ret = float3(-uv.x, uv.y, -1.0); break;
        default: ret = float3(0.0, 0.0, 1.0); break; // Default case
    }
    return normalize(ret);
}

// Compute orthonormal basis for converting from tanget/shading space to world space.
void ComputeBasisVectors(const float3 N, out float3 S, out float3 T)
{
	// Branchless select non-degenerate T.
	T = cross(N, float3(0.0, 1.0, 0.0));
	T = lerp(cross(N, float3(1.0, 0.0, 0.0)), T, step(Epsilon, dot(T, T)));

	T = normalize(T);
	S = normalize(cross(N, T));
}

// Convert point from tangent/shading space to world space.
float3 TangentToWorld(const float3 v, const float3 N, const float3 S, const float3 T)
{
	return S * v.x + T * v.y + N * v.z;
}

[numthreads(8, 8, 1)]
void main(uint3 ThreadID : SV_DispatchThreadID)
{
    uint outputWidth, outputHeight, outputDepth;
    outputTexture.GetDimensions(outputWidth, outputHeight, outputDepth);
    if (ThreadID.x >= outputWidth || ThreadID.y >= outputHeight)
        return;
	
    uint inputWidth, inputHeight, inputLevels;
    inputTexture.GetDimensions(0, inputWidth, inputHeight, inputLevels);
    const float wt = 4.0f * PI / (6.0f * inputWidth * inputHeight);
	
    float3 N = GetSamplingVector(ThreadID);

	float3 S, T;
	ComputeBasisVectors(N, S, T);

    // Lambert with albedo 1 and a cosine pdf: L * cosTheta / pdf / PI = L, so the estimate is the plain average.
    // (Same quantity as the old uniform 2 * L * cosTheta estimate; LightningPass needs no change.)
	float3 irradiance = 0.0f;
	for (uint i = 0; i < NumSamples; ++i) 
	{
		float2 u = SampleHammersley(i);
        float3 Lt = SampleCosineHemisphere(u.x, u.y);
        float3 Li = TangentToWorld(Lt, N, S, T);

        float pdf = max(Lt.x, Epsilon) / PI;
        float ws = 1.0f / (NumSamples * pdf);
        float mipLevel = max(0.5f * log2(ws / wt) + 1.0f, 0.0f);
        
		irradiance += inputTexture.SampleLevel(defaultSampler, Li, mipLevel).rgb; 
	}
	irradiance *= InvNumSamples;

    outputTexture[uint3(ThreadID.x, ThreadID.y, 0)] = float4(irradiance, 1.0f);
}