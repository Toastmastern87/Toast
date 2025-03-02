#inputlayout
vertex
vertex
vertex
vertex
vertex
instance

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
    float clickable;
    int entityID;
    int noWorldTransform;
    int isInstanced;
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
    float4 pixelPosition    : SV_POSITION;
    float3 viewPosition     : VIEWPOS;
    float3 viewNormal       : NORMAL;
    float2 texCoord         : TEXCOORD;
    float3x3 TBN            : TBASIS;
    int entityID            : TEXTUREID;
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
    float3 worldNormal;
    float4 worldTangent;
    
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
        
        float3 worldNormal = mul(input.normal, (float3x3) rotationMatrix);
        float3 worldTangent = mul(input.tangent.xyz, (float3x3) rotationMatrix);
    }
    else
    {
        if (noWorldTransform == 1)
        {
            worldPosition = float4(input.position, 1.0f);
            worldNormal = input.normal;
            worldTangent = input.tangent;

        }
        else
        {
            worldPosition = mul(float4(input.position, 1.0f), worldMatrix);
            worldNormal = mul(input.normal, (float3x3) worldMatrix);
            worldTangent = mul(input.tangent, worldMatrix);
        }
    }

    float4 viewPosition = mul(worldPosition, viewMatrix);
    output.pixelPosition = mul(viewPosition, projectionMatrix);
    output.viewPosition = viewPosition.xyz;

    float3 viewNormal = normalize(mul(worldNormal, (float3x3) viewMatrix));
    float4 viewTangent = normalize(mul(worldTangent, viewMatrix));
    
    float3 viewBitangent = cross(viewNormal, viewTangent.xyz) * viewTangent.w;
    
    float3x3 TBN = float3x3(viewTangent.xyz, viewBitangent, viewNormal);
    
    output.TBN = TBN;
    output.viewNormal = viewNormal;

    output.texCoord = input.texCoord;

    if (clickable > 0)
        output.entityID = entityID;
    else
        output.entityID = -1;

    return output;
}

#type pixel
#pragma pack_matrix( row_major )

struct PixelInputType
{
    float4 pixelPosition    : SV_POSITION;
    float3 viewPosition     : VIEWPOS;
    float3 viewNormal       : NORMAL;
    float2 texCoord         : TEXCOORD;
    float3x3 TBN            : TBASIS;
    int entityID            : TEXTUREID;
};

struct PixelOutputType
{
    float4 position         : SV_Target0;
    float4 normal           : SV_Target1;
    float4 albedoMetallic   : SV_Target2;
    float4 roughnessAO      : SV_Target3;
    int entityID            : SV_Target4;
};

cbuffer Material : register(b2)
{
    float4 Albedo;
    float Emission;
    float Metalness;
    float Roughness;
    int AlbedoTexToggle;
    int NormalTexToggle;
    int MetalRoughTexToggle;
};

Texture2D AlbedoTexture : register(t3);
Texture2D NormalTexture : register(t4);
Texture2D MetalRoughTexture : register(t5);

SamplerState defaultSampler : register(s0);

SamplerState defaultSampler2
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
    AddressW = Wrap;
    MipLODBias = 0.0f;
    MaxAnisotropy = 1;
    ComparisonFunc = NEVER;
    BorderColor = float4(0, 0, 0, 0);
    MinLOD = 0.0f;
    MaxLOD = FLT_MAX;
};

struct PBRParameters
{
    float3 Albedo;
    float Metalness;
    float Roughness;
    float AO;
};

PixelOutputType main(PixelInputType input)
{
    PixelOutputType output;
    PBRParameters params;
	
    // Sample input textures to get shading model params.
    params.Albedo = AlbedoTexToggle > 0 ? AlbedoTexture.Sample(defaultSampler, input.texCoord).rgb : Albedo.rgb;
    params.Metalness = MetalRoughTexToggle > 0 ? MetalRoughTexture.Sample(defaultSampler, input.texCoord).b : Metalness;
    params.Roughness = MetalRoughTexToggle > 0 ? MetalRoughTexture.Sample(defaultSampler, input.texCoord).r : Roughness;
    params.Roughness = max(params.Roughness, 0.05f); // Minimum roughness of 0.05 to keep specular highlight
    
    // Position
    output.position = float4(input.viewPosition, 1.0f);
	
    // Entity ID
    if (input.entityID > -1)
        output.entityID = input.entityID + 1;
    
    // Handle Normal Mapping
    float3 N;
    
    if (NormalTexToggle > 0)
    {
        // Sample the normal map
        float3 sampledNormal = NormalTexture.Sample(defaultSampler, input.texCoord).rgb;
        
        // Decode the normal from [0,1] to [-1,1]
        sampledNormal = sampledNormal * 2.0f - 1.0f;
        sampledNormal = normalize(sampledNormal);
        
        // Transform the sampled normal to view space
        N = normalize(mul(sampledNormal, input.TBN));
    }
    else
    {
        // Use the default view normal
        N = normalize(input.viewNormal);
    }
    
    // Encode Normal  
    float3 encodedNormal = N * 0.5 + 0.5;

    output.normal = float4(encodedNormal, 0.0);
    
    // Albedo & Metallic
    output.albedoMetallic.rgb = params.Albedo;
    output.albedoMetallic.a = params.Metalness;
    
    // RoughnessAO
    output.roughnessAO = float4(params.Roughness, 0.0f, 0.0f, 1.0f);
    
    return output;
}