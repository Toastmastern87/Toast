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
	int entityID			: TEXTUREID;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.position = mul(float4(input.position.xy, 1.0f, 1.0f), worldMatrix);
	output.position.x = ((output.position.x / width) * 2.0f) - 1.0f;
	output.position.y = ((output.position.y / height) * 2.0f) + 1.0f;
	output.position.z = 1.0f;
	output.position.w = 1.0f;

	output.texCoord = input.texCoord;

	output.entityID = entityID;

	return output;
}

#type pixel
cbuffer UIProp : register(b13)
{
	float4 color;
	float width;
	float height;
	float cornerRadius;
	float type;
};

struct PixelInputType
{
	float4 position			: SV_POSITION;
	float2 texCoord			: TEXCOORD;
	int entityID			: TEXTUREID;
};

Texture2D MDSFAtlas				: register(t6);

SamplerState defaultSampler		: register(s0);

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

float median(float r, float g, float b)
{
	return max(min(r, g), min(max(r, g), b));
}

// for 2D Text rendering only, for 3D another functions needs implementation
float ScreenPxRange()
{
	float pixRange = 2.0f;
	float geoSize = 72.0f;
	return geoSize / 32.0f * pixRange;
}

float4 main(PixelInputType input) : SV_TARGET
{
	// Panels
	if (type == 1.0f)
	{
		float2 dimensions = float2(width, height);

		float2 coords = input.texCoord * dimensions;
		if (ShouldDiscard(coords, dimensions, cornerRadius))
			discard;

		return color;
	}

	// TEXT
	if (type == 2.0f)
	{
		float4 bgColor = float4(color.rgb, 0.0); 
		float4 fgColor = color;

		float3 msd = MDSFAtlas.Sample(defaultSampler, input.texCoord).rgb;
		float sd = median(msd.r, msd.g, msd.b);
		float screenPxDistance = ScreenPxRange() * (sd - 0.5f);
		float opacity = clamp(screenPxDistance + 0.5f, 0.0f, 1.0f);
		float4 finalColor = lerp(bgColor, fgColor, opacity);
		if (opacity == 0.0)
			discard;

		return finalColor;
	}

	return color;
}