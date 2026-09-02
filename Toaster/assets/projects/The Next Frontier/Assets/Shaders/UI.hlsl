#inputlayout
vertex
vertex
vertex
vertex
vertex
vertex

#type vertex
#pragma pack_matrix( row_major )

cbuffer Camera : register(b0)
{
    matrix worldTranslationMatrix;
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
    float4 ab                   : POSITION;
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
	
    output.ab = input.size;
	
    output.UIType = (int) input.texCoord.z;
    output.cornerRadius = input.size.z;
    
    output.textured = input.position.w;
    
    output.textureIndex = input.textureIndex;

	return output;
}

#type pixel
#define MSDF_PX_RANGE 2.0f

struct PixelInputType
{
    float4 position         : SV_POSITION;
    float4 color            : COLOR;
    float4 ab               : POSITION;
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
    float4 colorHDRRT       : SV_Target1;
    int entityID		    : SV_Target2;
};

Texture2DArray MDSFAtlas        : register(t6);
Texture2DArray UITextures       : register(t8);

SamplerState defaultSampler		: register(s0);

float RoundedBoxSDF(float2 p, float2 halfSize, float radius)
{
    radius = min(radius, min(halfSize.x, halfSize.y));

    float2 q = abs(p) - halfSize + radius;
    
    return min(max(q.x, q.y), 0.0f) + length(max(q, 0.0f)) - radius;

}

float SDFCoverage(float d)
{
    float aa = max(fwidth(d), 1e-5f);

    return 1.0f - smoothstep(-aa, aa, d);
}

float2 ElementLocalPos(float2 texCoord, float2 size)
{
    return texCoord * size - size * 0.5f;
}

float median(float r, float g, float b)
{
	return max(min(r, g), min(max(r, g), b));
}

// for 2D Text rendering only, for 3D another functions needs implementation
float ScreenPxRange(float2 uv)
{
    float atlasWidth, atlasHeight, atlasElements;
    MDSFAtlas.GetDimensions(atlasWidth, atlasHeight, atlasElements);
    
    float2 unitRange = float2(MSDF_PX_RANGE, MSDF_PX_RANGE) / float2(atlasWidth, atlasHeight);
    float2 screenTexSize = 1.0f / fwidth(uv);
    
    return max(0.5f * dot(unitRange, screenTexSize), 1.0f);
}

float sdSegment(float2 p, float2 a, float2 b)
{
    float2 pa = p - a;
    float2 ba = b - a;
    float h = saturate(dot(pa, ba) / dot(ba, ba));
    return length(pa - ba * h); // distance to segment
}

PixelOutputType main(PixelInputType input) : SV_TARGET
{
    PixelOutputType output;
	
	// Panels
	if (input.UIType == 1.0f)
	{
        float2 size = abs(input.ab.xy);
        float2 halfSize = size * 0.5f;
        
        // Where this pixel sits inside the element, relative to its centre.
        float2 p = ElementLocalPos(input.texCoord, size);
        
        // How far this pixel is from the button's rounded outline.
        float d = RoundedBoxSDF(p, halfSize, input.cornerRadius);
        
        // Smooth 0..1 coverage instead of the panel branch's hard discard.
        float coverage = SDFCoverage(d);
        
        if (coverage <= 0.0f)
            discard;

        float4 fill;
        if (input.textured >= 0.5f)
        {
            float2 activeUV = input.texCoord * (size / 1000.0f);
            fill = UITextures.Sample(defaultSampler, float3(activeUV, input.textureIndex));
        }
        else
            fill = input.color;
        
        fill.a *= coverage;
        output.color = fill;
    }
	// Text
    else if (input.UIType == 2.0f)
	{
		float4 bgColor = float4(input.color.rgb, 0.0); 
		float4 fgColor = input.color;

        float3 msd = MDSFAtlas.Sample(defaultSampler, float3(input.texCoord, input.textureIndex)).rgb;
		float sd = median(msd.r, msd.g, msd.b);
        float screenPxDistance = ScreenPxRange(input.texCoord) * (sd - 0.5f);
		float opacity = clamp(screenPxDistance + 0.5f, 0.0f, 1.0f);
		float4 finalColor = lerp(bgColor, fgColor, opacity);
        if (opacity == 0.0)
            discard;

        output.color = finalColor;
    }   
    // Buttons
    else if (input.UIType > 2.5f && input.UIType < 3.5f)
    {
        float2 size = abs(input.ab.xy);
        float2 halfSize = size * 0.5f;
        
        // Where this pixel sits inside the element, relative to its centre.
        float2 p = ElementLocalPos(input.texCoord, size);
        
        // How far this pixel is from the button's rounded outline.
        float d = RoundedBoxSDF(p, halfSize, input.cornerRadius);
        
        // Smooth 0..1 coverage instead of the panel branch's hard discard.
        float coverage = SDFCoverage(d);
        
        if (coverage <= 0.0f)
            discard;
        
        float4 fill;
        if (input.textured >= 0.5f)
        {
            float2 activeUV = input.texCoord * (size / 1000.0f);
            fill = UITextures.Sample(defaultSampler, float3(activeUV, input.textureIndex));
        }
        else
            fill = input.color;
        
        fill.a *= coverage;
        output.color = fill;
    }
    // Connectors
    else if (input.UIType > 3.5f && input.UIType < 4.5f)
    {
        float2 A = input.ab.xy;
        float2 B = input.ab.zw;

        float thickness = input.texCoord.x; // px
        float aa = input.texCoord.y; // px

        // Current pixel position in UI space:
        // input.position is SV_POSITION in clip space after ortho; in D3D it is in pixels for rasterized screen-space.
        // With your off-center ortho, SV_POSITION.xy should match pixel coords in the render target.
        float2 P = input.position.xy;

        float d = sdSegment(P, A, B);

        float r = thickness * 0.5f;
        // alpha = 1 inside the line, fades out over 'aa' pixels
        float alpha = saturate((r + aa - d) / aa);

        if (alpha <= 0.0f)
            discard;

        output.color = float4(input.color.rgb, input.color.a * alpha);
    }  
	else
        output.color = input.color;
	
    output.colorHDRRT = output.color;
    
    // Connector should write to the picking render target
    if (input.UIType > 3.5f && input.UIType < 4.5f)
        output.entityID = 0;
    else if (input.entityID > -1)
        output.entityID = input.entityID + 1;
    else
        output.entityID = input.entityID;
    
    return output;
}