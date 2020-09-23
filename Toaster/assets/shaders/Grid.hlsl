#type vertex
#pragma pack_matrix( row_major )

cbuffer InverseMatrices : register(b0)
{
	matrix inverseViewMatrix;
	matrix inverseProjectionMatrix;
};

struct VertexInputType
{
	uint vertexID : SV_VertexID;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float3 nearPoint : NORMAL0;
	float3 farPoint : NORMAL1;
};

static float3 gridPlane[6] = {
	float3(-1.0f, -1.0f, 0.0f), float3(1.0f, 1.0f, 0.0f), float3(1.0f, -1.0f, 0.0f),
	float3(1.0f, 1.0f, 0.0f), float3(-1.0f, -1.0f, 0.0f), float3(-1.0f, 1.0f, 0.0f)
	};

float3 UnprojectPoint(float x, float y, float z, matrix viewInv, matrix projInv) {

	float4 unprojectedPoint = mul(mul(float4(x, y, z, 1.0f), projInv), viewInv);

	return unprojectedPoint.xyz / unprojectedPoint.w;
}

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	float3 p = gridPlane[input.vertexID];
	output.nearPoint = UnprojectPoint(p.x, p.y, 0.0f, inverseViewMatrix, inverseProjectionMatrix); // unprojecting on the near plane
	output.farPoint = UnprojectPoint(p.x, p.y, 1.0f, inverseViewMatrix, inverseProjectionMatrix); // unprojecting on the far plane

	output.position = float4(p, 1.0f); // using directly the clipped coordinates

	return output;
}

#type pixel
#pragma pack_matrix( row_major )

cbuffer GridData : register(b0)
{
	matrix viewMatrix;
	matrix projectionMatrix;
	float far;
	float near;
	float gradient;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float3 nearPoint : NORMAL0;
	float3 farPoint : NORMAL1;
};

struct PixelOutputType
{
	float4 color : SV_Target;
	float depth : SV_Depth;
};

float4 Grid(float3 R, float scale)
{
	float2 coord = R.xz * scale; // use the scale variable to set the distance between the lines
	float2 derivative = fwidth(coord);
	float2 grid = abs(frac(coord - 0.5f) - 0.5f) / derivative;
	float li = min(grid.x, grid.y);
	float minimumz = min(derivative.y, 10.0f);
	float minimumx = min(derivative.x, 10.0f);
	float4 color = float4(0.4f, 0.4f, 0.4f, 1.0f - min(li, 1.0f));
	// z axis
	if (R.x > (-1.0f * minimumx) && R.x < (1.0f * minimumx))
		color = float4(0.0f, 0.0f, 1.0f, 1.0f);
	// x axis
	if (R.z > (-1.0f * minimumz) && R.z < (1.0f * minimumz))
		color = float4(1.0f, 0.0f, 0.0f, 1.0f);

	return color;
}

float ComputeDepth(float3 pos) {
	float4 clipSpacePos = mul(mul(float4(pos, 1.0f), viewMatrix), projectionMatrix);

	return (clipSpacePos.z / clipSpacePos.w);
}

float ComputeLinearDepth(float3 pos)
{
	float4 clip = mul(float4(pos, 1.0f), viewMatrix);
	float depth = clip.z;
	return (depth - near) / (far - near);
}

PixelOutputType main(PixelInputType input)
{
	PixelOutputType output;

	float t = -input.nearPoint.y / (input.farPoint.y - input.nearPoint.y);
	float3 pos = input.nearPoint + t * (input.farPoint - input.nearPoint);
	
	float linearDepth = ComputeLinearDepth(pos);
	float fading = saturate(1.0f - gradient * linearDepth);

	output.depth = ComputeDepth(pos);
	output.color = (Grid(pos, 1.0f) + Grid(pos, 0.1f)) * float(t > 0.0f);
	output.color.a *= fading;

	return output;
}
