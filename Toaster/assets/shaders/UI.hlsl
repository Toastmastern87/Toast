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
    float4 params           : POSITION3;
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
    float4 params               : POSITION1;
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

    output.params = input.params;
    
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
    float4 params           : POSITION1;
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

float4 Premultiply(float4 col)
{
    return float4(col.rgb * col.a, col.a);
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

float SDSegment(float2 p, float2 a, float2 b)
{
    float2 pa = p - a;
    float2 ba = b - a;
    float h = saturate(dot(pa, ba) / dot(ba, ba));
    return length(pa - ba * h); // distance to segment
}

float SMin(float a, float b, float k)
{
    float h = saturate(0.5f + 0.5f * (b - a) / k);
    return lerp(b, a, h) - k * h * (1.0f - h);
}

float SDElbow(float2 p, float2 a, float2 corner, float2 b, float k)
{
    float d1 = SDSegment(p, a, corner);
    float d2 = SDSegment(p, corner, b);

    return (k > 0.0f) ? SMin(d1, d2, k) : min(d1, d2);
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
        output.color = Premultiply(fill);
    }
	// Text
    else if (input.UIType == 2.0f)
	{
        float3 msd = MDSFAtlas.Sample(defaultSampler, float3(input.texCoord, input.textureIndex)).rgb;
        float sd = median(msd.r, msd.g, msd.b);
        float screenPxDistance = ScreenPxRange(input.texCoord) * (sd - 0.5f);
        float opacity = clamp(screenPxDistance + 0.5f, 0.0f, 1.0f);

        if (opacity == 0.0)
            discard;

        float4 textColor = input.color;
        textColor.a *= opacity;
        output.color = Premultiply(textColor);
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
        output.color = Premultiply(fill);
    }
    // Connectors
    else if (input.UIType > 3.5f && input.UIType < 4.5f)
    {
        float2 A = input.ab.xy;
        float2 B = input.ab.zw;

        float thickness = input.texCoord.x; // px
        float style = input.texCoord.y;
        float cornerK = input.textured;

        // SV_POSITION.xy is in render-target pixels under the ortho projection,
        // which is the same space A and B are given in
        float2 P = input.position.xy;

        float d;
        if (style < 0.5f)
            d = SDSegment(P, A, B);
        else
        {
            float2 corner = (style < 1.5f) ? float2(B.x, B.y) : float2(A.x, A.y);
            d = SDElbow(P, A, corner, B, cornerK);
        }

        // Inflate to the requested thickness. Round caps come free.
        d -= thickness * 0.5f;

        float coverage = SDFCoverage(d);

        if (coverage <= 0.0f)
            discard;

        float outlineWidth = input.params.x;
        float outlineColor = input.params.yzw;
        
        float4 col = input.color;
        
        if (outlineWidth > 0.0f)
        {
            float dOutline = abs(d) - outlineWidth;
            float aa = max(fwidth(dOutline), 1e-5f);
            float outlineMask = 1.0f - smoothstep(-aa, aa, dOutline);

            col.rgb = lerp(col.rgb, outlineColor, outlineMask);
        }
        
        col.a *= coverage;
        output.color = Premultiply(col);
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