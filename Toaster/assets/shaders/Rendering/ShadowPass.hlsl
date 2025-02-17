#inputlayout
vertex
vertex
vertex
vertex
vertex
instance

#type vertex
#pragma pack_matrix( row_major )

cbuffer Model : register(b1)
{
    matrix worldMatrix;
    int entityID;
    int noWorldTransform;
    int isInstanced;
};

cbuffer DirectionalLight : register(b3)
{
    matrix lightViewProj;
    float4 direction;
    float4 radiance;
    float multiplier;
};

struct VertexInputType
{
    float3 position                 : POSITION0;
    float3 normal                   : NORMAL;
    float4 tangent                  : TANGENT;
    float2 texCoord                 : TEXCOORD;
    float3 color                    : COLOR0;
    float3 worldInstancePosition    : POSITION1;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
};

float Random(float3 seed)
{
    // Simple random function based on the dot product
    return frac(sin(dot(seed, float3(12.9898f, 78.233f, 45.164f)) * 43758.5453f));
}

float4x4 CreateRotationMatrix(float3 rotationAngles)
{
    // Rotation matrix around the X axis
    float cosX = cos(rotationAngles.x);
    float sinX = sin(rotationAngles.x);
    float4x4 rotationX = float4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, cosX, -sinX, 0.0f,
        0.0f, sinX, cosX, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

    // Rotation matrix around the Y axis
    float cosY = cos(rotationAngles.y);
    float sinY = sin(rotationAngles.y);
    float4x4 rotationY = float4x4(
        cosY, 0.0f, sinY, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        -sinY, 0.0f, cosY, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

    // Rotation matrix around the Z axis
    float cosZ = cos(rotationAngles.z);
    float sinZ = sin(rotationAngles.z);
    float4x4 rotationZ = float4x4(
        cosZ, -sinZ, 0.0f, 0.0f,
        sinZ, cosZ, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

    // Combine all rotations
    return mul(mul(rotationX, rotationY), rotationZ);
}

PixelInputType main(VertexInputType input)
{
    PixelInputType output;
    float4 worldPosition;
    
    if (isInstanced)
    {
        // Generate pseudo-random scale and rotation
        float seed = Random(input.worldInstancePosition);
        float scale = lerp(1.0f, 5.0f, seed); // Scale between 80% and 120%
    
        // Generate random rotation angles (between 0 and 2 * PI)
        float3 rotationAngles = float3(
        seed * 6.2831853, // Random X rotation
        (seed + 0.33f) * 6.2831853, // Random Y rotation
        (seed + 0.66f) * 6.2831853); // Random Z rotation
        
        // Create rotation matrix
        float4x4 rotationMatrix = CreateRotationMatrix(rotationAngles);
        
        // Apply scale and rotation
        float4 transformedPosition = float4(input.position * scale, 1.0f);
        transformedPosition = mul(transformedPosition, rotationMatrix);
        
        worldPosition = float4(transformedPosition.xyz + input.worldInstancePosition, 1.0f);
    }
    else
    {
        if (noWorldTransform == 1)
        {
            worldPosition = float4(input.position, 1.0f);
        }
        else
        {
            worldPosition = mul(float4(input.position, 1.0f), worldMatrix);
        }
    }

    output.position = mul(worldPosition, lightViewProj);
    return output;
}

#type pixel
struct PixelInputType
{
    float4 position                 : SV_POSITION;
};

float4 main(PixelInputType input) : SV_TARGET
{
    // Depth is automatically written to the depth buffer
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}