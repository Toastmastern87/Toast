#inputlayout
vertex
vertex

#type vertex
#pragma pack_matrix( row_major )

cbuffer Camera : register(b0)
{
    matrix worldMovementMatrix;
	matrix viewMatrix;
	matrix projectionMatrix;
	matrix inverseViewMatrix;
	matrix inverseProjectionMatrix;
	float4 cameraPosition;
	float far;
	float near;
    float viewportWidth;
    float viewportHeight;
};

cbuffer Model : register(b1)
{
    matrix worldMatrix;
    int entityID;
    int instanced;
};

struct VertexInputType
{
	float4 position			: POSITION0;
	float4 size				: POSITION1;
    float4 color			: COLOR;
    float3 texCoord         : POSITION2;
    uint entityID           : TEXTUREID0;
    uint textureIndex       : TEXTUREID1;
};

struct PixelInputType
{
    float4 position             : SV_POSITION;
    float4 color                : COLOR;
    float2 size                 : POSITION;
    float2 texCoord             : TEXCOORD0;
    float cornerRadius          : PSIZE0;
    float textured              : PSIZE1;
    int entityID                : TEXTUREID0;
    int UIType                  : TEXTUREID1;
    uint textureIndex           : TEXTUREID2;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.position = float4(input.position.xyz, 1.0f);
	output.position = mul(output.position, projectionMatrix);
	//output.position.w = 1.0f;

	output.texCoord = input.texCoord.xy;
	
    output.color = input.color;

	output.entityID = input.entityID;
	
    output.size = input.size.xy;
	
    output.UIType = (int) input.texCoord.z;
    output.cornerRadius = input.size.z;
    
    output.textured = input.position.w;
    
    output.textureIndex = input.textureIndex;

	return output;
}

#type pixel
struct PixelInputType
{
    float4 position         : SV_POSITION;
    float4 color            : COLOR;
    float2 size             : POSITION;
    float2 texCoord         : TEXCOORD0;
    float cornerRadius      : PSIZE0;
    float textured          : PSIZE1;
    int entityID            : TEXTUREID0;
    int UIType              : TEXTUREID1;
    uint textureIndex       : TEXTUREID2;
};

struct PixelOutputType
{
    float4 color		    : SV_Target0;
    int entityID		    : SV_Target1;
};

Texture2D MDSFAtlas				: register(t6);
Texture2DArray UITextures       : register(t8);

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

// Function to check distance from a point to a corner center
float CheckCornerDistance(float2 p, float2 center, float radius)
{
    return length(p - center) > radius;
}

PixelOutputType main(PixelInputType input) : SV_TARGET
{
    PixelOutputType output;
	
	// Panels
	if (input.UIType == 1.0f)
	{
        float4 textureColor;
        
		float2 coords = input.texCoord * input.size;
        if (ShouldDiscard(coords, input.size, input.cornerRadius))
			discard;

        if (input.textured >= 0.5f)
        {
            float2 activeUV;
            activeUV.x = input.texCoord.x * (input.size.x / 1000.0f);
            activeUV.y = (1.0f - (input.size.y / 1000.0f)) + input.texCoord.y * (input.size.y / 1000.0f);
            textureColor = UITextures.Sample(defaultSampler, float3(activeUV, input.textureIndex));
            output.color = textureColor;
        }
        else
            output.color = input.color;
    }
	// Text
    else if (input.UIType == 2.0f)
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

        output.color = finalColor;
    }
    // Buttons
    else if (input.UIType == 3.0f)
    {
        float4 textureColor;  

        if (input.textured >= 0.5f)
        {
            float2 activeUV;
            activeUV.x = input.texCoord.x * (input.size.x / 1000.0f);
            activeUV.y = (1.0f - (input.size.y / 1000.0f)) + input.texCoord.y * (input.size.y / 1000.0f);
            textureColor = UITextures.Sample(defaultSampler, float3(activeUV, input.textureIndex));
            output.color = textureColor;
        }
        else
            output.color = input.color;
    }
	else
        output.color = input.color;
	
    if (input.entityID > 0)
        output.entityID = input.entityID + 1;
    else
        output.entityID = input.entityID;
    
    return output;
}