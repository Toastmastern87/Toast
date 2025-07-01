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
    float3 normal               : NORMAL0;
};

struct PlanetPointVS      
{
    float3 posVS; // for SV_POSITION
    float3 nWS; // unit sphere normal in world-space
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

PlanetPointVS CalulatePlanetPosVS(int2 gWorld, float heightScale)
{
    // metres on local tangent plane
    float2 off = (float2)gWorld * (float)CellSize;

    // reference‐sphere point in view space
    float3 pSphereWS = PlanetCentreVS + BasisRadUp * PlanetRadius + BasisTanEast * off.x + BasisTanNorth * off.y;

    // radial direction (compute **once**)
    float3 nWS = normalize(pSphereWS - PlanetCentreVS);

    // height sample and displacement
    float h = SampleHeight(SphereUV(nWS));
    float3 pWS = PlanetCentreVS + nWS * (PlanetRadius + h * heightScale);

    PlanetPointVS p;
    p.posVS = mul(float4(pWS, 1.0f), viewMatrix).xyz;
    p.nWS = nWS;
    return p;
}

// Call this *instead* of face‐averaging
float3 AnalyticalNormal(float2 uv)
{
    uint texWidth, texHeight;
    HeightMapTexture.GetDimensions(texWidth, texHeight);
    
    float u = 1.0f / texWidth;
    float v = 1.0f / texHeight;
    
    // 1) base + neighbor heights
    float h0 = SampleHeight(uv);
    float hU = SampleHeight(uv + float2(u, 0));
    float hV = SampleHeight(uv + float2(0, v));

    // 2) parameterize sphere direction from uv
    float lon = (uv.x - 0.5) * 2 * PI;
    float lat = (uv.y - 0.5) * PI;
    float3 S = float3(cos(lat) * sin(lon), sin(lat), cos(lat) * cos(lon));

    // 3) partials ∂S/∂lon, ∂S/∂lat
    float3 dSdlon = float3(cos(lat) * cos(lon), 0, -cos(lat) * sin(lon));
    float3 dSdlat = float3(-sin(lat) * sin(lon), cos(lat), -sin(lat) * cos(lon));

    // 4) chain‐rule for P(u,v)=(R+h)·S
    float dlon = u * 2 * PI;
    float dlat = v * PI;
    float dhdlon = (hU - h0) / dlon;
    float dhdlat = (hV - h0) / dlat;

    float R = PlanetRadius;
    float3 Pu = (R + h0) * dSdlon + dhdlon * S;
    float3 Pv = (R + h0) * dSdlat + dhdlat * S;

    // 5) exact normal
    return normalize(cross(Pv, Pu));
}

PixelInputType main(VertexInputType input)
{
    PixelInputType output;
    
    // unpack & scroll to **planet metres** on the tangent plane
    int2 gWorld = int2(OriginX, OriginY) + int2(input.grid);
    
    PlanetPointVS C = CalulatePlanetPosVS(gWorld, 1.0f);
    
    float2 uv = SphereUV(C.nWS);
   
    float3 smoothNOS = AnalyticalNormal(uv);
    float3 smoothNVS = normalize(mul(smoothNOS,(float3x3)viewMatrix));
    
    output.pixelPosition = mul(float4(C.posVS, 1.0f), projectionMatrix);
    output.viewPosition = C.posVS;
    output.normal = smoothNVS;
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
    float3 normal           : NORMAL0;
};

struct PixelOutputType
{
    float4 position         : SV_Target0;
    float4 normal           : SV_Target1;
    float4 albedoMetallic   : SV_Target2;
    float4 roughnessAO      : SV_Target3;
    int entityID            : SV_Target4;
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

struct PBRParameters
{
    float3 Albedo;
    float Metalness;
    float Roughness;
    float AO;
};

Texture2D HeightMapTexture : register(t0);

SamplerState HeightMapSampler : register(s5);

PixelOutputType main(PixelInputType input)
{
    PixelOutputType output;
    PBRParameters params;
    
    /*--------------------------------------------------------------*/
    /* 1) position + normal                                         */
    /*--------------------------------------------------------------*/
    
    output.position = float4(input.viewPosition, 1.0);
    output.normal = float4(input.normal * 0.5f + 0.5f, -1);

    /*--------------------------------------------------------------*/
    /* 2) albedo + metallic                                         */
    /*--------------------------------------------------------------*/
    
    params.Albedo = Albedo.rgb; /* later:   if(AlbedoTexToggle) … */
    
    output.albedoMetallic.rgb = params.Albedo;
    output.albedoMetallic.a = 0.0f;

    /*--------------------------------------------------------------*/
    /* 3) roughness + ambient occlusion                             */
    /*--------------------------------------------------------------*/
    output.roughnessAO = float4(Roughness, 0.0f, 0.0, 1.0);

    output.entityID = 0;

    return output;
}