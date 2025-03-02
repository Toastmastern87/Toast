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
	float2 texCoord			: TEXCOORD;
    uint entityID			: TEXTUREID;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 size : POSITION;
    float2 texCoord : TEXCOORD;
    float cornerRadius : PSIZE0;
    float textured : PSIZE1;
    float borderSize : PSIZE2;
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

	output.entityID = input.entityID;
	
    output.size = input.size.xy;
	
    output.UIType = (int) input.position.z;
    output.cornerRadius = input.size.z;
    output.borderSize = input.size.w;
    
    output.textured = input.position.w;

	return output;
}

#type pixel
struct PixelInputType
{
    float4 position		: SV_POSITION0;
    float4 color		: COLOR;
    float2 size			: POSITION;
    float2 texCoord		: TEXCOORD;
    float cornerRadius	: PSIZE0;
    float textured      : PSIZE1;
    float borderSize    : PSIZE2;
    int entityID		: TEXTUREID0;
    int UIType			: TEXTUREID1;
};

struct PixelOutputType
{
    float4 color		: SV_Target0;
    int entityID		: SV_Target1;
};

Texture2D MDSFAtlas				: register(t6);
Texture2D PanelTexture          : register(t8);

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
		float2 coords = input.texCoord * input.size;
        if (ShouldDiscard(coords, input.size, input.cornerRadius))
			discard;

        if (input.textured >= 0.5f)
        {                        
            // Texture dimensions in texture space
            float w;
            float h;
            PanelTexture.GetDimensions(w, h);
            float2 textureSize = float2(w, h);
            
            float texBorderSizeX = 10.0f; // Width of the corner slice in the texture
            float texBorderSizeY = 10.0f; // Height of the corner slice in the texture
            
            float borderSizeX = input.borderSize;
            float borderSizeY = input.borderSize;
            
            // Compute the size of the middle slice in the texture
            float texMiddleSizeX = textureSize.x - 2.0f * texBorderSizeX;
            float texMiddleSizeY = textureSize.y - 2.0f * texBorderSizeY;
            
            // Compute the size of the middle region in screen space
            float middleSizeX = max(input.size.x - 2.0f * borderSizeX, 0.0f);
            float middleSizeY = max(input.size.y - 2.0f * borderSizeY, 0.0f);

            bool inLeft = coords.x <= borderSizeX;
            bool inRight = coords.x >= (input.size.x - borderSizeX);
            bool inTop = coords.y >= (input.size.y - borderSizeY);
            bool inBottom = coords.y <= borderSizeY;

            bool isCorner = (inLeft && inTop) || (inRight && inTop) || (inLeft && inBottom) || (inRight && inBottom);
            bool isEdgeHorizontal = (coords.x > borderSizeX && coords.x < (input.size.x - borderSizeX)) && (inTop || inBottom);
            bool isEdgeVertical = (coords.y > borderSizeY && coords.y < (input.size.y - borderSizeY)) && (inLeft || inRight);
            bool isMiddle = !isCorner && !isEdgeHorizontal && !isEdgeVertical;
            
            float2 uv;
            float2 cornerPosition;
            float2 cornerUVStart;
            
            float4 textureColor;
            if (isCorner)
            {
                // Determine corner positions and UV starts
                if (inLeft && inTop)
                {
                    // Top-left corner
                    cornerPosition = float2(0.0f, input.size.y - borderSizeY);
                    cornerUVStart = float2(0.0f, 0.0f);
                }
                else if (inRight && inTop)
                {
                    // Top-right corner
                    cornerPosition = float2(input.size.x - borderSizeX, input.size.y - borderSizeY);
                    cornerUVStart = float2(textureSize.x - texBorderSizeX, 0.0f);
                }
                else if (inLeft && inBottom)
                {
                    // Bottom-left corner
                    cornerPosition = float2(0.0f, 0.0f);
                    cornerUVStart = float2(0.0f, textureSize.y - texBorderSizeY);
                }
                else // inRight && inBottom
                {
                    // Bottom-right corner
                    cornerPosition = float2(input.size.x - borderSizeX, 0.0f);
                    cornerUVStart = float2(textureSize.x - texBorderSizeX, textureSize.y - texBorderSizeY);
                }

                // Calculate local coordinates within the corner (screen space)
                float2 localCoords = coords - cornerPosition;

                   // Scale localCoords to match the size of the texture slices
                float2 scaledCoords = localCoords * float2(texBorderSizeX / borderSizeX, texBorderSizeY / borderSizeY);

                // Compute UVs
                uv = (cornerUVStart + scaledCoords) / textureSize;

                textureColor = PanelTexture.Sample(defaultSampler, uv);
            }
            else if (isEdgeHorizontal)
            {
                // Determine if top or bottom edge
                bool isTopEdge = inTop;

                // Screen space positions and sizes
                float2 edgePosition = float2(borderSizeX, isTopEdge ? input.size.y - borderSizeY : 0.0f);
                float2 edgeSize = float2(middleSizeX, borderSizeY);

                // Texture space UV starts
                float2 edgeUVStart = float2(texBorderSizeX, isTopEdge ? 0.0f : textureSize.y - texBorderSizeY);

                // Local coordinates within the edge
                float2 localCoords = coords - edgePosition;

                // Scale localCoords to match texture slice size
                float2 scaledCoords = localCoords * float2(texMiddleSizeX / edgeSize.x, texBorderSizeY / edgeSize.y);

                // Compute UVs
                uv = (edgeUVStart + scaledCoords) / textureSize;

                // Sample the texture
                textureColor = PanelTexture.Sample(defaultSampler, uv);
            }
            else if (isEdgeVertical)
            {
                // Determine if left or right edge
                bool isLeftEdge = inLeft;

                // Screen space positions and sizes
                float2 edgePosition = float2(isLeftEdge ? 0.0f : input.size.x - borderSizeX, borderSizeY);
                float2 edgeSize = float2(borderSizeX, middleSizeY);

                // Texture space UV starts
                float2 edgeUVStart = float2(isLeftEdge ? 0.0f : textureSize.x - texBorderSizeX, texBorderSizeY);

                // Local coordinates within the edge
                float2 localCoords = coords - edgePosition;

                // Scale localCoords to match texture slice size
                float2 scaledCoords = localCoords * float2(texBorderSizeX / edgeSize.x, texMiddleSizeY / edgeSize.y);

                // Compute UVs
                uv = (edgeUVStart + scaledCoords) / textureSize;

                // Sample the texture
                textureColor = PanelTexture.Sample(defaultSampler, uv);
            }
            else if (isMiddle)
            {
                // Define inner dimensions
                float innerBorderSizeX = borderSizeX;
                float innerBorderSizeY = borderSizeY;
                float2 innerSize = input.size - 2.0f * float2(innerBorderSizeX, innerBorderSizeY);

                // Ensure the inner corner radius is valid
                float innerCornerRadius = max(input.cornerRadius - borderSizeX, 0.0f);

                // Determine if the current pixel is near an inner corner
                bool inInnerTopLeft = (coords.x < innerBorderSizeX + innerCornerRadius) && (coords.y > input.size.y - innerBorderSizeY - innerCornerRadius);
                bool inInnerTopRight = (coords.x > input.size.x - innerBorderSizeX - innerCornerRadius) && (coords.y > input.size.y - innerBorderSizeY - innerCornerRadius);
                bool inInnerBottomLeft = (coords.x < innerBorderSizeX + innerCornerRadius) && (coords.y < innerBorderSizeY + innerCornerRadius);
                bool inInnerBottomRight = (coords.x > input.size.x - innerBorderSizeX - innerCornerRadius) && (coords.y < innerBorderSizeY + innerCornerRadius);

                bool shouldRenderInnerBorder = false;
                float2 innerCornerPosition;
                float2 innerCornerUVStart;

                if (inInnerTopLeft)
                {
                    float2 center = float2(innerBorderSizeX + innerCornerRadius, input.size.y - innerBorderSizeY - innerCornerRadius);
                    shouldRenderInnerBorder = CheckCornerDistance(coords, center, innerCornerRadius) > 0.0f;

                    if (shouldRenderInnerBorder)
                    {
                    // Compute UV for inner top-left corner
                        innerCornerPosition = float2(innerBorderSizeX, input.size.y - innerBorderSizeY);
                        innerCornerUVStart = float2(0.0f, 0.0f);

                        float2 localCoords = coords - innerCornerPosition;

                        float2 scaledCoords = localCoords * float2(texBorderSizeX / borderSizeX, texBorderSizeY / borderSizeY);

                        uv = (innerCornerUVStart + scaledCoords) / textureSize;

                        textureColor = PanelTexture.Sample(defaultSampler, uv);
                    }
                }
                else if (inInnerTopRight)
                {
                    float2 center = float2(input.size.x - innerBorderSizeX - innerCornerRadius, input.size.y - innerBorderSizeY - innerCornerRadius);
                    shouldRenderInnerBorder = CheckCornerDistance(coords, center, innerCornerRadius) > 0.0f;

                    if (shouldRenderInnerBorder)
                    {
                    // Compute UV for inner top-right corner
                        innerCornerPosition = float2(input.size.x - innerBorderSizeX, input.size.y - innerBorderSizeY);
                        innerCornerUVStart = float2(textureSize.x - texBorderSizeX, 0.0f);

                        float2 localCoords = coords - innerCornerPosition;

                        float2 scaledCoords = localCoords * float2(texBorderSizeX / borderSizeX, texBorderSizeY / borderSizeY);

                        uv = (innerCornerUVStart + scaledCoords) / textureSize;

                        textureColor = PanelTexture.Sample(defaultSampler, uv);
                    }
                }
                else if (inInnerBottomLeft)
                {
                    float2 center = float2(innerBorderSizeX + innerCornerRadius, innerBorderSizeY + innerCornerRadius);
                    shouldRenderInnerBorder = CheckCornerDistance(coords, center, innerCornerRadius) > 0.0f;

                    if (shouldRenderInnerBorder)
                    {
                    // Compute UV for inner bottom-left corner
                        innerCornerPosition = float2(innerBorderSizeX, innerBorderSizeY);
                        innerCornerUVStart = float2(0.0f, textureSize.y - texBorderSizeY);

                        float2 localCoords = coords - innerCornerPosition;

                        float2 scaledCoords = localCoords * float2(texBorderSizeX / borderSizeX, texBorderSizeY / borderSizeY);

                        uv = (innerCornerUVStart + scaledCoords) / textureSize;

                        textureColor = PanelTexture.Sample(defaultSampler, uv);
                    }
                }
                else if (inInnerBottomRight)
                {
                    float2 center = float2(input.size.x - innerBorderSizeX - innerCornerRadius, innerBorderSizeY + innerCornerRadius);
                    shouldRenderInnerBorder = CheckCornerDistance(coords, center, innerCornerRadius) > 0.0f;

                    if (shouldRenderInnerBorder)
                    {
                    // Compute UV for inner bottom-right corner
                        innerCornerPosition = float2(input.size.x - innerBorderSizeX, innerBorderSizeY);
                        innerCornerUVStart = float2(textureSize.x - texBorderSizeX, textureSize.y - texBorderSizeY);

                        float2 localCoords = coords - innerCornerPosition;

                        float2 scaledCoords = localCoords * float2(texBorderSizeX / borderSizeX, texBorderSizeY / borderSizeY);

                        uv = (innerCornerUVStart + scaledCoords) / textureSize;

                        textureColor = PanelTexture.Sample(defaultSampler, uv);
                    }
                }

                if (!shouldRenderInnerBorder)
                {
                    // Local coordinates within the middle region
                    float2 localCoords = coords - float2(borderSizeX, borderSizeY);

                    // Ensure middleSizeX and middleSizeY are not zero
                    float safeMiddleSizeX = max(middleSizeX, 1.0f);
                    float safeMiddleSizeY = max(middleSizeY, 1.0f);

                    // Compute tileUV in [0,1] range
                    float2 tileUV = frac(localCoords / float2(safeMiddleSizeX, safeMiddleSizeY));

                    // Define UV coordinates for the middle slice
                    float2 uvMin = float2(texBorderSizeX, texBorderSizeY) / textureSize;
                    float2 uvMax = (float2(texBorderSizeX, texBorderSizeY) + float2(texMiddleSizeX, texMiddleSizeY)) / textureSize;

                    // Calculate texel size
                    float2 texelSize = 1.0f / textureSize;

                    // Inset uvMin and uvMax to avoid sampling at the edges
                    uvMin += texelSize * 0.5f; // Adjust multiplier as needed
                    uvMax -= texelSize * 0.5f;

                    // Map tileUV from [0,1] to [uvMin, uvMax]
                    float2 uvMiddle = uvMin + tileUV * (uvMax - uvMin);

                    // Sample the texture for the inner area
                    textureColor = PanelTexture.Sample(defaultSampler, uvMiddle);
                }  
            }
            
            output.color = textureColor;
        }
        else
            output.color = input.color;
    }
	// TEXT
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
	else
        output.color = input.color;
	
    if (input.entityID > 0)
        output.entityID = input.entityID + 1;
    else
        output.entityID = input.entityID;

    return output;
}