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
    int planet;
};

struct VertexInputType
{
    float3 position         : POSITION;
    float3 normal           : NORMAL;
    float4 tangent          : TANGENT;
    float2 texCoord         : TEXCOORD;
    float3 color            : COLOR0;
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

PixelInputType main(VertexInputType input)
{
    PixelInputType output;

    float4 worldPosition;
    if (planet != 1)
    {
        worldPosition = mul(float4(input.position, 1.0f), worldMatrix);
    }
    else
    {
        worldPosition = float4(input.position, 1.0f);
    }
 
    float4 viewPosition = mul(worldPosition, viewMatrix);
    output.pixelPosition = mul(viewPosition, projectionMatrix);
    output.viewPosition = viewPosition.xyz;
    
    float3 worldNormal = mul(input.normal, (float3x3) worldMatrix);
    float3 viewNormal = mul(worldNormal, (float3x3) viewMatrix);
    
    float3 worldTangent = mul(input.tangent.xyz, (float3x3) worldMatrix);
    float3 viewTangent = mul(worldTangent, (float3x3) viewMatrix);
    viewTangent = normalize(viewTangent);
    
    float3 viewBitangent = cross(viewNormal, viewTangent) * input.tangent.w;
    
    float3x3 TBN = float3x3(viewTangent, viewBitangent, viewNormal);
    
    output.TBN = TBN;
    output.viewNormal = viewNormal;

    output.texCoord = input.texCoord;

    output.entityID = entityID;

    return output;
}

#type pixel
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
    params.Albedo = AlbedoTexToggle == 1 ? AlbedoTexture.Sample(defaultSampler, input.texCoord).rgb : Albedo.rgb;
    params.Metalness = MetalRoughTexToggle == 1 ? MetalRoughTexture.Sample(defaultSampler, input.texCoord).b : Metalness;
    params.Roughness = MetalRoughTexToggle == 1 ? MetalRoughTexture.Sample(defaultSampler, input.texCoord).r : Roughness;
    params.Roughness = max(params.Roughness, 0.05f); // Minimum roughness of 0.05 to keep specular highlight
    
    // Position
    output.position = float4(input.viewPosition, 1.0f);
	
    // Entity ID
    output.entityID = input.entityID + 1;
    
    // Normal
    float3 N = normalize(input.viewNormal);
    if (NormalTexToggle == 1)
    {
        float3 normalMap = 2.0f * NormalTexture.Sample(defaultSampler, input.texCoord).rgb - 1.0f;
        N = normalize(mul(normalMap, input.TBN));
    }
    
    float3 encodedNormal = N * 0.5 + 0.5;

    output.normal = float4(encodedNormal, 0.0);
    
    // Albedo & Metallic
    output.albedoMetallic.rgb = params.Albedo;
    output.albedoMetallic.a = params.Metalness;
    
    // RoughnessAO
    output.roughnessAO = float4(params.Roughness, 0.0f, 0.0f, 1.0f);
    
    return output;
}