#inputlayout
#type compute

Texture2D heightMap						: register(t0);
Texture2D averateHeightMap				: register(t1);
RWTexture2DArray<float4> outputTexture	: register(u0);

SamplerState defaultSampler : register(s0);

[numthreads(32, 32, 1)]
void main( uint3 ThreadID : SV_DispatchThreadID )
{
	float inputWidth, inputHeight, height, averateHeight;
	heightMap.GetDimensions(inputWidth, inputHeight);

	float2 uv = float2(ThreadID.xy) / float2(inputWidth, inputHeight);

	height = heightMap.SampleLevel(defaultSampler, float2(uv.x, uv.y), 0);
	averateHeight = averateHeightMap.SampleLevel(defaultSampler, float2(uv.x, uv.y), 0);

	if(height < averateHeight)
		outputTexture[ThreadID] = float4(0.0f, 0.0f, 0.0f, 1.0f); 
	else
		outputTexture[ThreadID] = float4(1.0f, 1.0f, 1.0f, 1.0f);
}