#inputlayout
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

#include "TerrainObjectCommon.hlsli"

StructuredBuffer<TerrainInstance> TerrainInstances : register(t0);

PixelInputType main(VertexInputType input, uint instanceID : SV_InstanceID)
{
    PixelInputType output;

    float4 worldPosition;
    float3 worldNormal;
    float3 worldTangent;
    
    // CURRENTLY THIS WILL ONLY RENDER TERRAIN OBJECTS!
    if (isInstanced)
    {       
        TerrainInstance inst = TerrainInstances[instanceID];

        float3 localPos = input.position * inst.Scale;
        float3 pCR = inst.PosCR + QuatRotate(inst.RotQuat, localPos);

        float3 rotatedN = QuatRotate(inst.RotQuat, input.normal);
        float3 rotatedT = QuatRotate(inst.RotQuat, input.tangent.xyz);

        // Output
        float4 viewPosition = mul(float4(pCR, 1.0f), viewMatrix);
        output.pixelPosition = mul(viewPosition, projectionMatrix);
        output.viewPosition = viewPosition.xyz;

        worldNormal = rotatedN;
        worldTangent = float4(rotatedT, input.tangent.w);

        float3 viewNormal = normalize(mul(rotatedN, (float3x3) viewMatrix));
        float3 viewTangent = normalize(mul(rotatedT, (float3x3) viewMatrix));
        float3 viewBitangent = cross(viewNormal, viewTangent) * input.tangent.w;

        output.TBN = float3x3(viewTangent, viewBitangent, viewNormal);
        output.viewNormal = viewNormal;
        output.texCoord = input.texCoord;
        output.entityID = -1;

        return output;
    }
    else
    {
        if (noWorldTransform == 1)
        {
            worldPosition = float4(input.position, 1.0f);
            worldPosition = mul(worldPosition, worldTranslationMatrix);
            worldNormal = input.normal;
            worldTangent = input.tangent;
        }
        else
        {
            worldPosition = mul(float4(input.position, 1.0f), worldMatrix);
            worldPosition = mul(worldPosition, worldTranslationMatrix);
            worldNormal = mul(input.normal, (float3x3) worldMatrix);
            worldTangent = mul(input.tangent.xyz, (float3x3) worldMatrix);
        }
    }

    float4 viewPosition = mul(worldPosition, viewMatrix);
    output.pixelPosition = mul(viewPosition, projectionMatrix);
    output.viewPosition = viewPosition.xyz;

    float3 viewNormal = normalize(mul(worldNormal, (float3x3) viewMatrix));
    float3 viewTangent = normalize(mul(worldTangent, (float3x3) viewMatrix));
    
    float3 viewBitangent = cross(viewNormal, viewTangent) * input.tangent.w;
    
    float3x3 TBN = float3x3(viewTangent, viewBitangent, viewNormal);
    
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
    float4 pixelPosition        : SV_POSITION;
    float3 viewPosition         : VIEWPOS;
    float3 viewNormal           : NORMAL;
    float2 texCoord             : TEXCOORD;
    float3x3 TBN                : TBASIS;
    int entityID                : TEXTUREID;
};

struct PixelOutputType
{
    float4 position             : SV_Target0;
    float4 normal               : SV_Target1;
    float4 albedoMetallic       : SV_Target2;
    float4 roughnessAO          : SV_Target3;
    int entityID                : SV_Target4;
    float4 planetMaterialDebug  : SV_Target5;
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

Texture2D AlbedoTexture         : register(t3);
Texture2D NormalTexture         : register(t4);
Texture2D MetalRoughTexture     : register(t5);

SamplerState defaultSampler : register(s0);

struct PBRParameters
{
    float3 Albedo;
    float Metalness;
    float Roughness;
    float AO;
};

float3 LinearToSRGB(float3 x)
{
    float3 lo = x * 12.92;
    float3 hi = 1.055 * pow(abs(x), 1.0 / 2.4) - 0.055;
    return lerp(hi, lo, step(x, 0.0031308));
}

PixelOutputType main(PixelInputType input)
{
    PixelOutputType output;
    PBRParameters params;
    
    // Sample input textures to get shading model params.
    params.Albedo = AlbedoTexToggle > 0 ? AlbedoTexture.Sample(defaultSampler, input.texCoord).rgb : Albedo.rgb;
    params.Metalness = MetalRoughTexToggle > 0 ? MetalRoughTexture.Sample(defaultSampler, input.texCoord).r : Metalness;
    params.Roughness = MetalRoughTexToggle > 0 ? MetalRoughTexture.Sample(defaultSampler, input.texCoord).g : Roughness;
    params.Roughness = max(params.Roughness, 0.05f); // Minimum roughness of 0.05 to keep specular highlight
    
    // TODO MIGHT NEED TO BE FIXED AT A LATER STAGE TO GET CORRECT ALBEDO MAPPING
    if (AlbedoTexToggle > 0)
        params.Albedo = LinearToSRGB(params.Albedo);
          
    // Position
    output.position = float4(input.viewPosition, 1.0f);
	
    // Entity ID
    if (input.entityID > -1)
        output.entityID = input.entityID + 1;
    else
        output.entityID = 0;
    
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

    if (input.entityID > -1)
        output.normal = float4(encodedNormal, 1.0);
    else
        output.normal = float4(encodedNormal, (float)input.entityID);
    
    // Albedo & Metallic
    output.albedoMetallic.rgb = params.Albedo;
    output.albedoMetallic.a = params.Metalness;
    
    // RoughnessAO
    output.roughnessAO = float4(params.Roughness, 0.0f, 0.0f, 1.0f);
    
    output.planetMaterialDebug = float4(0.0f, 0.0f, 0.0f, 1.0f);
    
    return output;
}