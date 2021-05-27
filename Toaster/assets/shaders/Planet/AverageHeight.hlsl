#inputlayout
#type compute

Texture2D inputTexture : register(t0);
RWTexture2DArray<float4> outputTexture : register(u0);

SamplerState defaultSampler : register(s0);

[numthreads(32, 32, 1)]
void main( uint3 ThreadID : SV_DispatchThreadID )
{
	float inputWidth, inputHeight;
	inputTexture.GetDimensions(inputWidth, inputHeight);
	
	int numberOfSamples = 0;
	float4 combinedHeight = float4(0.0f, 0.0f, 0.0f, 0.0f);

	const int RANGE = 20;
	if (ThreadID.x < (inputWidth - RANGE) && ThreadID.x > RANGE && ThreadID.y < (inputHeight - RANGE) && ThreadID.y > RANGE)
	{
		for (float x = ThreadID.x - RANGE; x < ThreadID.x + RANGE; x++)
		{
			for (float y = ThreadID.y - RANGE; y < ThreadID.y + RANGE; y++)
			{
				combinedHeight += inputTexture.SampleLevel(defaultSampler, float2(x / inputWidth, y / inputHeight), 0);

				numberOfSamples++;
			}
		}
	}

	float4 averageHeight = combinedHeight / numberOfSamples;

	outputTexture[ThreadID] = averageHeight;
}