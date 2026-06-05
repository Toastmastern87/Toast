#inputlayout
#type vertex

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
    matrix worldMatrix; // marker's true-world transform (position + orient + scale)
    float clickable;
    int entityID;
    int noWorldTransform;
    int isInstanced;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

VSOut main(uint vertexID : SV_VertexID)
{
    // Two triangles, 6 verts forming a unit quad in local XZ (flat on Y).
    // Model matrix's up-row = surface normal lifts/orients it.
    float2 corners[6] =
    {
        float2(-0.5, -0.5), float2(-0.5, 0.5), float2(0.5, -0.5),
        float2(0.5, -0.5), float2(-0.5, 0.5), float2(0.5, 0.5)
    };
    float2 uvs[6] =
    {
        float2(0, 1), float2(0, 0), float2(1, 1),
        float2(1, 1), float2(0, 0), float2(1, 0)
    };

    float2 c = corners[vertexID];
    float4 localPos = float4(c.x, 0.0, c.y, 1.0);

    VSOut output;
    float4 trueWorld = mul(localPos, worldMatrix); // local -> true world
    float4 cameraRel = mul(trueWorld, worldTranslationMatrix); // true world -> camera-relative
    float4 viewPos = mul(cameraRel, viewMatrix);
    output.position = mul(viewPos, projectionMatrix);
    output.uv = uvs[vertexID];
    return output;
}

#type pixel
cbuffer MarkerCBuffer : register(b8)
{
    float4 MarkerParams; // x = alpha, y = time (seconds), z = rotationSpeed (rad/sec), w = unused
};

Texture2D MarkerTexture : register(t0);
SamplerState DefaultSampler : register(s1);

struct PSIn
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

struct PSOut
{
    float4 hdr : SV_Target0;
    float4 sdr : SV_Target1;
};

PSOut main(PSIn input)
{
    // Rotate UVs around the center by time * speed
    float angle = MarkerParams.y * MarkerParams.z;
    float s = sin(angle);
    float c = cos(angle);

    float2 centered = input.uv - 0.5f;
    float2 rotated = float2(
        centered.x * c - centered.y * s,
        centered.x * s + centered.y * c);
    float2 uv = rotated + 0.5f;

    float4 tex = MarkerTexture.Sample(DefaultSampler, uv);
    float a = tex.a * MarkerParams.x;
    if (a <= 0.001f)
        discard;

    PSOut output;
    output.sdr = float4(tex.rgb, a);
    const float paperWhiteScale = 2000.0f / 200.0f;
    output.hdr = float4(tex.rgb * paperWhiteScale, a);
    return output;
}