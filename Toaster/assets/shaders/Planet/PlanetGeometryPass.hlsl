#inputlayout        // packed 16-bit grid coordinates: uint16 gx, uint16 gy
vertex

#type vertex
#pragma pack_matrix( row_major )

static const float PI = 3.14159265359f;
static const float INV_TWO_PI = 1.0f / (2.0f * PI);
static const float INV_PI = 1.0f / PI;

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
    float3 PlanetCentreVS;
    float PlanetRadius;

    float3 BasisTanEast;
    float MaxHeight;
    float3 BasisTanNorth;
    float MinHeight;
    float3 BasisRadUp;
    float3 BasisLonEast;
    float3 BasisLonNorth;
    float3 BasisSpinUp;
};

cbuffer PlanetLevel : register(b7) 
{
    int OriginX;
    int OriginY;
    int CellSize;
    int GridSize;
};

struct PixelInputType
{
    float4 pixelPosition        : SV_POSITION;
    float3 viewPosition         : VIEWPOS;
    float3 viewNormal           : NORMAL0;
    float3 planetNormal         : NORMAL1;
    float2 texCoord             : TEXCOORD0;
};

Texture2D HeightMapTexture      : register(t0);

SamplerState HeightMapSampler   : register(s5);

float SampleHeight(float2 uv)     // uv in [0,1]
{
    float h = HeightMapTexture.SampleLevel(HeightMapSampler, uv, 0).r;
    return lerp(MinHeight, MaxHeight, h);
}

float2 SphereUV(float3 nSphere)
{
    float3 v;
    v.x = dot(nSphere, BasisLonEast);
    v.y = dot(nSphere, BasisSpinUp);
    v.z = dot(nSphere, BasisLonNorth);

    float lon = atan2(v.z, v.x); // −π … +π
    float lat = asin(v.y); // −π/2 … +π/2
    return float2(lon * INV_TWO_PI + 0.5, lat * INV_PI + 0.5);
}

PixelInputType main(VertexInputType input)
{
    PixelInputType output;
    
    // unpack & scroll to **planet metres** on the tangent plane
    int2 g = int2(input.grid); // 0 … 256 etc.
    int2 world = int2(OriginX, OriginY) + g; // scrolled grid coords
    float2 off = (float2) world * float(CellSize);
    
    /*--- position in camera-relative space ---------------------------*/
    float3 Pws = PlanetCentreVS + BasisRadUp * PlanetRadius + BasisTanEast * off.x + BasisTanNorth * off.y;

    // push down onto the sphere surface
    float3 normalSphere = normalize(Pws - PlanetCentreVS);
    
    output.planetNormal = float3(dot(normalSphere, BasisLonEast), dot(normalSphere, BasisSpinUp), dot(normalSphere, BasisLonNorth));
    
    float2 uv = SphereUV(normalSphere);
    
    float rawHeightCenter  = SampleHeight(uv);
    
    Pws = PlanetCentreVS + normalSphere * (PlanetRadius + rawHeightCenter);
    
    // Eastern neighbour height
    float2 offE = off + float2(CellSize, 0);
    float3 PE_sph = PlanetCentreVS + BasisRadUp * PlanetRadius + BasisTanEast * offE.x + BasisTanNorth * offE.y;

    float3 nSphereE = normalize(PE_sph - PlanetCentreVS);
    float2 uvE = SphereUV(nSphereE);
    float hE = SampleHeight(uvE);
    float3 P_e_ws = PlanetCentreVS + nSphereE * (PlanetRadius + hE);

    // Northern neighbour height
    float2 offN = off + float2(0, CellSize);
    float3 PN_sph = PlanetCentreVS + BasisRadUp * PlanetRadius + BasisTanEast * offN.x + BasisTanNorth * offN.y;

    float3 nSphereN = normalize(PN_sph - PlanetCentreVS);
    float2 uvN = SphereUV(nSphereN);
    float hN = SampleHeight(uvN);
    float3 P_n_ws = PlanetCentreVS + nSphereN * (PlanetRadius + hN);

    // Geometric normal
    float3 normalGeometric_ws = normalize(cross(P_n_ws - Pws, P_e_ws - Pws));
    
    float4 Pv = mul(float4(Pws, 1), viewMatrix);
    output.pixelPosition = mul(Pv, projectionMatrix);
    output.viewPosition = Pv.xyz;
    output.viewNormal = mul(float4(normalGeometric_ws, 0.0), viewMatrix).xyz;
    return output;
}

#type pixel
#pragma pack_matrix( row_major )

static const float PI = 3.14159265359f;
static const float INV_TWO_PI = 1.0f / (2.0f * PI);
static const float INV_PI = 1.0f / PI;

struct PixelInputType
{
    float4 pixelPosition    : SV_POSITION;
    float3 viewPosition     : VIEWPOS;
    float3 viewNormal       : NORMAL0;
    float3 planetNormal     : NORMAL1;
    float2 texCoord         : TEXCOORD0;
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
    
    params.Albedo = Albedo.rgb; /* later:   if(AlbedoTexToggle) … */
    
    float lon = atan2(input.planetNormal.z, input.planetNormal.x); // −π … +π
    float lat = asin(input.planetNormal.y); // −π/2 … +π/2

    float2 uv = float2(lon * INV_TWO_PI + 0.5, lat * INV_PI + 0.5);
    
    output.albedoMetallic.rgb = float4(params.Albedo, 0.0f);
    output.albedoMetallic.a = 0.0f;

    /*--------------------------------------------------------------*/
    /* 4) roughness + ambient occlusion                             */
    /*--------------------------------------------------------------*/
    output.roughnessAO = float4(Roughness, 0.0f, 0.0, 1.0);

    output.entityID = 0;

    return output;
}