#inputlayout        // packed 16-bit grid coordinates: uint16 gx, uint16 gy
vertex

#type vertex
#pragma pack_matrix( row_major )

struct VertexInputType
{
    uint2 grid : POSITION0; // gx,gy 0 … N-1
};

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

cbuffer PlanetFrame : register(b4)
{
    float3 PlanetCentreWS;
    float PlanetRadius;
    float3 BasisEast;
    float3 BasisNorth;
    float3 BasisUp;
};

cbuffer PlanetLevel : register(b7) 
{
    uint OriginX;
    uint OriginY;
    uint CellSize;
    uint GridSize;
};

struct PixelInputType
{
    float4 pixelPosition : SV_POSITION;
    float3 viewPosition : VIEWPOS;
    float3 viewNormal : NORMAL;
};

PixelInputType main(VertexInputType input)
{
    PixelInputType output;
    
    // unpack & scroll to **planet metres** on the tangent plane
    int2 g = int2(input.grid); // 0 … 256 etc.
    int2 world = int2(OriginX, OriginY) + g; // scrolled grid coords
    float2 off = (float2) world * float(CellSize);

    // build the 3-D position in world space (tangent space -> world space)
    float3 Pws = PlanetCentreWS + BasisUp * PlanetRadius + BasisEast * off.x + BasisNorth * off.y;

    // push down onto the sphere surface
    float3 nrm = normalize(Pws - PlanetCentreWS);
    Pws = PlanetCentreWS + nrm * PlanetRadius;
    
    float4 Pwst = mul(float4(Pws, 1.0f), worldTranslationMatrix);

    float4 Pv = mul(Pwst, viewMatrix);
    output.pixelPosition = mul(Pv, projectionMatrix);
    output.viewPosition = Pv.xyz;
    output.viewNormal = mul(float4(nrm, 1.0f), viewMatrix);
    return output;
}

#type pixel
#pragma pack_matrix( row_major )

struct PixelInputType
{
    float4 pixelPosition    : SV_POSITION;
    float3 viewPosition     : VIEWPOS;
    float3 viewNormal       : NORMAL;
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

    /*--------------------------------------------------------------*/
    /* 1) position – just forward the view-space position           */
    /*--------------------------------------------------------------*/
    output.position = float4(input.viewPosition, 1.0f);

    /*--------------------------------------------------------------*/
    /* 2) normal – encode from -1..1 to 0..1 so it fits RGBA8       */
    /*--------------------------------------------------------------*/
    float3 N = normalize(input.viewNormal);
    float3 encN = N * 0.5 + 0.5; // map to [0,1]

    // like your mesh shader: pack encoded normal; spare channel carries “no-ID flag”
    output.normal = float4(encN, -1);

    /*--------------------------------------------------------------*/
    /* 3) albedo + metallic                                         */
    /*--------------------------------------------------------------*/
    float3 albedo = Albedo.rgb; /* later:   if(AlbedoTexToggle) … */

    output.albedoMetallic.rgb = float3(0.0f, 0.0f, 0.0f);
    output.albedoMetallic.a = 0.0f;

    /*--------------------------------------------------------------*/
    /* 4) roughness + ambient occlusion                             */
    /*--------------------------------------------------------------*/
    output.roughnessAO = float4(Roughness, 0.0f, 0.0, 1.0);

    /*--------------------------------------------------------------*/
    /* 5) entity / picking ID                                       */
    /*--------------------------------------------------------------*/
    output.entityID = -1; // 0 == “no hit”

    return output;
}