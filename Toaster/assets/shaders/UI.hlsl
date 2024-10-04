#inputlayout
vertex
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
	float far;
	float near;
};

cbuffer Model : register(b1)
{
	matrix worldMatrix;
	int entityID;
};

struct VertexInputType
{
	float3 position			: POSITION0;
	float3 size				: POSITION1;
    float4 color			: COLOR;
	float2 texCoord			: TEXCOORD;
    uint EntityID			: TEXTUREID;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 size : POSITION;
    float2 texCoord : TEXCOORD;
    float cornerRadius : PSIZE;
    int entityID : TEXTUREID0;
    int UIType : TEXTUREID1;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.position = float4(input.position.xy, 1.0f, 1.0f);
	output.position = mul(output.position, projectionMatrix);
	output.position.z = 1.0f;
	output.position.w = 1.0f;

	output.texCoord = input.texCoord;
	
    output.color = input.color;

	output.entityID = entityID;
	
    output.size = input.size.xy;
	
    output.UIType = (int) input.position.z;
    output.cornerRadius = input.size.z;

	return output;
}

#type pixel
struct PixelInputType
{
    float4 position		: SV_POSITION;
    float4 color		: COLOR;
    float2 size			: POSITION;
    float2 texCoord		: TEXCOORD;
    float cornerRadius	: PSIZE;
    int entityID		: TEXTUREID0;
    int UIType			: TEXTUREID1;
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
	if (input.UIType == 1.0f)
	{
		float2 coords = input.texCoord * input.size;
        if (ShouldDiscard(coords, input.size, input.cornerRadius))
			discard;

		return input.color;
	}

	// TEXT
    if (input.UIType == 2.0f)
	{
		float4 bgColor = float4(input.color.rgb, 0.0); 
		float4 fgColor = input.color;

		float3 msd = MDSFAtlas.Sample(defaultSampler, input.texCoord).rgb;
		float sd = median(msd.r, msd.g, msd.b);
		float screenPxDistance = ScreenPxRange() * (sd - 0.5f);
		float opacity = clamp(screenPxDistance + 0.5f, 0.0f, 1.0f);
		float4 finalColor = lerp(bgColor, fgColor, opacity);
		if (opacity == 0.0)
			discard;

		return finalColor;
	}

    return input.color;
}