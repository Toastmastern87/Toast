#inputlayout
vertex

#type vertex
#pragma pack_matrix( row_major )

cbuffer Camera : register(b0)
{
	matrix viewMatrix;
	matrix projectionMatrix;
	matrix inverseViewMatrix;
	matrix inverseProjectionMatrix;
	float4 cameraPosition;
};

struct VertexInputType
{
	uint vertexID : SV_VertexID;
};

struct PixelInputType
{
	float4 position			: SV_POSITION;
	float3 nearPoint		: NORMAL0;
	float3 farPoint			: NORMAL1;
	float3 cameraPosition	: POSITION0;
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

	output.cameraPosition = cameraPosition.xyz;
	output.nearPoint = UnprojectPoint(p.x, p.y, 0.0f, inverseViewMatrix, inverseProjectionMatrix); // unprojecting on the near plane
	output.farPoint = UnprojectPoint(p.x, p.y, 1.0f, inverseViewMatrix, inverseProjectionMatrix); // unprojecting on the far plane
	output.position = float4(p, 1.0f); // using directly the clipped coordinates

	return output;
}

#type pixel
#pragma pack_matrix( row_major )

cbuffer Grid			: register(b10)
{
	matrix viewMatrix;
	matrix projectionMatrix;
	float far;
	float near;
};

struct PixelInputType
{
	float4 position			: SV_POSITION;
	float3 nearPoint		: NORMAL0;
	float3 farPoint			: NORMAL1;
	float3 cameraPosition	: POSITION0;
};

struct PixelOutputType
{
	float4 color		: SV_Target;
	float depth			: SV_Depth;
};

float ComputeLinearDepth(float3 pos)
{
	float4 clip = mul(float4(pos, 1.0f), viewMatrix);
	float depth = clip.z;
	return (depth - near) / (far - near);
}

float ComputeDepth(float3 pos) {
	float4 clipSpacePos = mul(mul(float4(pos, 1.0f), viewMatrix), projectionMatrix);

	return (clipSpacePos.z / clipSpacePos.w);
}

float4 Grid(float3 R, float scale)
{
	float2 coord = R.xz * scale; // use the scale variable to set the distance between the lines
	float2 derivative = fwidth(coord);
	float2 grid = abs(frac(coord - 0.5f) - 0.5f) / derivative;
	float li = min(grid.x, grid.y);
	float minimumz = min(derivative.y, 1.0f);
	float minimumx = min(derivative.x, 1.0f);
	float4 color = float4(0.4f, 0.4f, 0.4f, 1.0f - min(li, 1.0f));
	// z axis
	if (R.x > (-(1.0f/scale) * minimumx) && R.x < ((1.0f / scale) * minimumx))
		color = float4(0.0f, 0.0f, 1.0f, 1.0f - min(li, 1.0f));
	// x axis
	if (R.z > (-(1.0f / scale) * minimumz) && R.z < ((1.0f / scale) * minimumz))
		color = float4(1.0f, 0.0f, 0.0f, 1.0f - min(li, 1.0f));

	return color;
}

PixelOutputType main(PixelInputType input)
{
	PixelOutputType output;

	float t = -input.nearPoint.y / (input.farPoint.y - input.nearPoint.y);
	float3 pos = input.nearPoint + t * (input.farPoint - input.nearPoint);

	float distanceToOrigo = sqrt(input.cameraPosition.x * input.cameraPosition.x + input.cameraPosition.y * input.cameraPosition.y + input.cameraPosition.z * input.cameraPosition.z);

	float gradient = 7500.0f / distanceToOrigo;
	float scale = 10.0f / distanceToOrigo;

	float linearDepth = ComputeLinearDepth(pos);
	float fading = saturate(1.0f - gradient * linearDepth);

	output.depth = ComputeDepth(pos);
	output.color = (Grid(pos, scale) + Grid(pos, scale / 10.0f)) * float(t > 0.0f);
	output.color.a *= fading;

	return output;
}