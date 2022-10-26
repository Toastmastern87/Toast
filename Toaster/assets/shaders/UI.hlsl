#inputlayout
vertex
vertex

#type vertex
#pragma pack_matrix( row_major )

cbuffer UI : register(b12)
{
	float width;
	float height;
};

cbuffer Model : register(b1)
{
	matrix worldMatrix;
	int entityID;
};

struct VertexInputType
{
	float3 position			: POSITION;
	float2 texCoord			: TEXCOORD;
};

struct PixelInputType
{
	float4 position			: SV_POSITION;
	float2 texCoord			: TEXCOORD;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.position.x = ((input.position.x / width) * 2.0f) - 1.0f;
	output.position.y = ((input.position.y / height) * 2.0f) + 1.0f;
	output.position.z = 1.0f;
	output.position.w = 1.0f;

	output.texCoord = input.texCoord;

	return output;
}

#type pixel
cbuffer UIProp : register(b13)
{
	float4 color;
	float width;
	float height;
	float cornerRadius;
};

struct PixelInputType
{
	float4 position			: SV_POSITION;
	float2 texCoord			: TEXCOORD;
};

bool ShouldDiscard(float2 coords, float2 dimensions, float radius)
{
    float2 circle_center = float2(radius, radius);

    if (length(coords - circle_center) > radius
        && coords.x < circle_center.x && coords.y < circle_center.y) return true; //first circle

    circle_center.x += dimensions.x - 2 * radius;

    if (length(coords - circle_center) > radius
        && coords.x > circle_center.x && coords.y < circle_center.y) return true; //second circle

    circle_center.y += dimensions.y - 2 * radius;

    if (length(coords - circle_center) > radius
        && coords.x > circle_center.x && coords.y > circle_center.y) return true; //third circle

    circle_center.x -= dimensions.x - 2 * radius;

    if (length(coords - circle_center) > radius
        && coords.x < circle_center.x && coords.y > circle_center.y) return true; //fourth circle

    return false;
}

float4 main(PixelInputType input) : SV_TARGET
{
	float2 dimensions = float2(width, height);

	float2 coords = input.texCoord * dimensions;
	if (ShouldDiscard(coords, dimensions, cornerRadius))
		discard;

	return color;
}