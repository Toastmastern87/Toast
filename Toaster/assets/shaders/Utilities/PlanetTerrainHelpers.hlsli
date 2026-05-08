#define MAX_MATERIALS 8

// ================================================================
// Terrain erosion compile-time quality
// ================================================================
// 1 = fast iteration shader
// 0 = full quality shader
#define TERRAIN_EROSION_DEV_MODE 0

#if TERRAIN_EROSION_DEV_MODE

    // Fast compile / fast iteration
    #define TERRAIN_EROSION_OCTAVES 1
    #define TERRAIN_EROSION_USE_9_CELL_BLEND 0
    #define TERRAIN_NORMAL_INCLUDES_EROSION 0

#else

    // Full quality
    #define TERRAIN_EROSION_OCTAVES 4
    #define TERRAIN_EROSION_USE_9_CELL_BLEND 1
    #define TERRAIN_NORMAL_INCLUDES_EROSION 1

#endif

float3 CubeFaceUVToDirRemap(uint face, float2 uv)
{
    CubeSample cs = RemapFaceUV(face, uv);
    return CubeFaceUVToDir(cs.face, cs.uv);
}

float SampleCubeBilinear(float3 dir, uint2 dims, uint mip)
{
    CubeSample cs = DirectionToCube(dir);
    uint face = cs.face;
    float2 uv = cs.uv;

    float2 p = uv * dims - 0.5f;
    float2 fxy = frac(p);
    int2 i0 = int2(floor(p));
    int2 i1 = i0 + 1;

    float2 uv00 = (float2(i0) + 0.5f) / dims;
    float2 uv10 = (float2(i1.x, i0.y) + 0.5f) / dims;
    float2 uv01 = (float2(i0.x, i1.y) + 0.5f) / dims;
    float2 uv11 = (float2(i1) + 0.5f) / dims;

    CubeSample c00 = RemapFaceUV(face, uv00);
    CubeSample c10 = RemapFaceUV(face, uv10);
    CubeSample c01 = RemapFaceUV(face, uv01);
    CubeSample c11 = RemapFaceUV(face, uv11);

    int2 wh = int2(dims);

    int2 ij00 = clamp(int2(c00.uv * wh), int2(0, 0), wh - 1);
    int2 ij10 = clamp(int2(c10.uv * wh), int2(0, 0), wh - 1);
    int2 ij01 = clamp(int2(c01.uv * wh), int2(0, 0), wh - 1);
    int2 ij11 = clamp(int2(c11.uv * wh), int2(0, 0), wh - 1);

    float v00 = HeightCubeArray.Load(int4(ij00, c00.face, mip));
    float v10 = HeightCubeArray.Load(int4(ij10, c10.face, mip));
    float v01 = HeightCubeArray.Load(int4(ij01, c01.face, mip));
    float v11 = HeightCubeArray.Load(int4(ij11, c11.face, mip));

    float vx0 = lerp(v00, v10, fxy.x);
    float vx1 = lerp(v01, v11, fxy.x);
    return lerp(vx0, vx1, fxy.y);
}

// NEW ONES
void BuildSphereTangents(float3 dir, out float3 tanU, out float3 tanV)
{
    float3 up = (abs(dir.y) < 0.999) ? float3(0, 1, 0) : float3(1, 0, 0);
    tanU = normalize(cross(up, dir));
    tanV = normalize(cross(dir, tanU));
}

float SampleHeightMetres(float3 dir)
{
    uint W, H, L;
    HeightCubeArray.GetDimensions(W, H, L);
    return SampleCubeBilinear(normalize(dir), uint2(W, H), 0);
}

float ComputeWallSteepenBoost(float3 dir, float hC, out float outMask)
{
    outMask = 0.0;

    if (WallEnhancementEnabled <= 0.5 || WallStrength <= 0.0001)
        return 0.0;

    float3 tanU, tanV;
    BuildSphereTangents(dir, tanU, tanV);

    float stepMeters = max(WallStepMeters, 1.0);
    float angularStep = stepMeters / max(planetRadius + hC, 1.0);

    float3 dirU0 = normalize(dir + tanU * angularStep);
    float3 dirU1 = normalize(dir - tanU * angularStep);
    float3 dirV0 = normalize(dir + tanV * angularStep);
    float3 dirV1 = normalize(dir - tanV * angularStep);

    float hU0 = SampleHeightMetres(dirU0);
    float hU1 = SampleHeightMetres(dirU1);
    float hV0 = SampleHeightMetres(dirV0);
    float hV1 = SampleHeightMetres(dirV1);

    float dhdu = (hU0 - hU1) / max(2.0 * stepMeters, 1.0);
    float dhdv = (hV0 - hV1) / max(2.0 * stepMeters, 1.0);

    float2 grad = float2(dhdu, dhdv);
    float slopeTan = length(grad);

    float slopeMask = smoothstep(WallSlopeStart, WallSlopeEnd, slopeTan);
    outMask = slopeMask;

    if (slopeMask <= 0.001 || slopeTan <= 0.0001)
        return 0.0;

    float2 gradDir = grad / slopeTan;
    float3 tangentGradDir = normalize(tanU * gradDir.x + tanV * gradDir.y);

    float3 dirHigh = normalize(dir + tangentGradDir * angularStep);
    float3 dirLow = normalize(dir - tangentGradDir * angularStep);

    float hHigh = SampleHeightMetres(dirHigh);
    float hLow = SampleHeightMetres(dirLow);

    float hMin = min(hHigh, hLow);
    float hMax = max(hHigh, hLow);

    float hMid = 0.5 * (hMin + hMax);
    float halfRange = max(0.5 * (hMax - hMin), 1.0);

    // 0..1 position through the local height transition.
    float x = saturate((hC - hMid) / halfRange * 0.5 + 0.5);

    // Smaller interval = harder, more vertical wall.
    float sharpStart = min(WallSharpStart, WallSharpEnd - 0.001);
    float sharpEnd = max(WallSharpEnd, sharpStart + 0.001);

    float xSharp = smoothstep(sharpStart, sharpEnd, x);

    float hSharp = lerp(hMin, hMax, xSharp);

    float boost = (hSharp - hC) * WallStrength * slopeMask;

    return clamp(boost, -WallMaxDelta, WallMaxDelta);
}

float SampleWallHeightOnly(float3 dir)
{
    float h = SampleHeightMetres(dir);

    float wallMask = 0.0;
    float wallBoost = ComputeWallSteepenBoost(dir, h, wallMask);

    return h + wallBoost;
}

float SampleColorAvg(float3 dir)
{
    uint W, H, L;
    AlbedoCubeArray.GetDimensions(W, H, L);

    CubeSample cs = DirectionToCube(normalize(dir));
    int2 ij = clamp(int2(cs.uv * float2(W, H)), int2(0, 0), int2(W - 1, H - 1));
    float4 color = AlbedoCubeArray.Load(int4(ij, cs.face, 0));
    return (color.r + color.g + color.b) / 3.0;
}


float ComputeMaterialWeight(MaterialData mat, float slope, float colorAvg)
{
    // Full score inside range, smooth falloff outside edges
    float slopeScore = 1.0;
    if (slope < mat.SlopeMin)
        slopeScore = saturate(1.0 - (mat.SlopeMin - slope) * mat.BlendSharpness);
    else if (slope > mat.SlopeMax)
        slopeScore = saturate(1.0 - (slope - mat.SlopeMax) * mat.BlendSharpness);

    float colorScore = 1.0;
    if (mat.UseAlbedo > 0.5)
    {
        if (colorAvg < mat.ColorAvgMin)
            colorScore = saturate(1.0 - (mat.ColorAvgMin - colorAvg) * mat.BlendSharpness);
        else if (colorAvg > mat.ColorAvgMax)
            colorScore = saturate(1.0 - (colorAvg - mat.ColorAvgMax) * mat.BlendSharpness);
    }

    return slopeScore * colorScore;
}

void ComputeAllMaterialWeights(float slope, float colorAvg, uint matCount, out float weights[MAX_MATERIALS])
{
    float totalWeight = 0.0;

    for (uint m = 0; m < matCount; m++)
    {
        weights[m] = ComputeMaterialWeight(Materials[m], slope, colorAvg);
        totalWeight += weights[m];
    }

    // Normalize
    float invTotal = (totalWeight > 0.001) ? (1.0 / totalWeight) : 0.0;
    for (uint m2 = 0; m2 < matCount; m2++)
        weights[m2] *= invTotal;
}

void BuildErosionFramePS(float3 dir, out float3 axisA, out float3 axisB)
{
    float3 a = abs(dir);

    if (a.x >= a.y && a.x >= a.z)
    {
        axisA = float3(0, 0, 1);
        axisB = float3(0, 1, 0);
    }
    else if (a.y >= a.x && a.y >= a.z)
    {
        axisA = float3(1, 0, 0);
        axisB = float3(0, 0, 1);
    }
    else
    {
        axisA = float3(1, 0, 0);
        axisB = float3(0, 1, 0);
    }
}

float ComputeErosionFadeTarget(float3 dir, float hC)
{
    float3 tanU, tanV;
    BuildSphereTangents(dir, tanU, tanV);

    float stepMeters = max(ErosionStepMeters, 1.0);
    float angularStep = stepMeters / max(planetRadius + hC, 1.0);

    float3 dirU0 = normalize(dir + tanU * angularStep);
    float3 dirU1 = normalize(dir - tanU * angularStep);
    float3 dirV0 = normalize(dir + tanV * angularStep);
    float3 dirV1 = normalize(dir - tanV * angularStep);

    float hU0 = SampleWallHeightOnly(dirU0);
    float hU1 = SampleWallHeightOnly(dirU1);
    float hV0 = SampleWallHeightOnly(dirV0);
    float hV1 = SampleWallHeightOnly(dirV1);

    float hMin = min(min(hU0, hU1), min(hV0, hV1));
    float hMax = max(max(hU0, hU1), max(hV0, hV1));

    float hMid = 0.5 * (hMin + hMax);
    float hRange = max(0.5 * (hMax - hMin), max(ErosionStrength * 0.6, 1.0));

    return clamp((hC - hMid) / hRange, -1.0, 1.0);
}

struct HeightGrad2D
{
    float h;
    float2 grad;
};

HeightGrad2D ComputeWallHeightGrad2D(float3 dir, out float3 axisA, out float3 axisB)
{
    HeightGrad2D r;

    float hC = SampleWallHeightOnly(dir);

    BuildErosionFramePS(dir, axisA, axisB);

    // Project frame axes onto the local tangent plane.
    float3 tanA = axisA - dir * dot(axisA, dir);
    float3 tanB = axisB - dir * dot(axisB, dir);

    float lenA = length(tanA);
    float lenB = length(tanB);

    if (lenA < 0.001 || lenB < 0.001)
        BuildSphereTangents(dir, tanA, tanB);
    else
    {
        tanA /= lenA;
        tanB /= lenB;
    }

    float stepMeters = max(ErosionStepMeters, 1.0);
    float angularStep = stepMeters / max(planetRadius + hC, 1.0);

    float3 dirA0 = normalize(dir + tanA * angularStep);
    float3 dirA1 = normalize(dir - tanA * angularStep);
    float3 dirB0 = normalize(dir + tanB * angularStep);
    float3 dirB1 = normalize(dir - tanB * angularStep);

    float hA0 = SampleWallHeightOnly(dirA0);
    float hA1 = SampleWallHeightOnly(dirA1);
    float hB0 = SampleWallHeightOnly(dirB0);
    float hB1 = SampleWallHeightOnly(dirB1);

    r.h = hC;

    r.grad = float2((hA0 - hA1) / max(2.0 * stepMeters, 1.0), (hB0 - hB1) / max(2.0 * stepMeters, 1.0));

    return r;
}

float EaseOutSlope(float t)
{
    float v = 1.0 - saturate(t);
    return 1.0 - v * v;
}

float SlopeToErosionMask(float slopeTan)
{
    float t = smoothstep(ErosionSlopeStart, ErosionSlopeFull, slopeTan);
    float fadeOut = 1.0 - smoothstep(ErosionSlopeEnd, ErosionSlopeFadeOut, slopeTan);

    return EaseOutSlope(t) * fadeOut;
}

float2 SafeNormalize2(float2 v)
{
    float len = length(v);
    return (len > 1e-10) ? (v / len) : float2(0.0, 0.0);
}

float PowInv(float t, float power)
{
    return 1.0 - pow(1.0 - saturate(t), power);
}

float SmoothStart(float t, float smoothing)
{
    if (t >= smoothing)
        return t - 0.5 * smoothing;

    return 0.5 * t * t / max(smoothing, 0.0001);
}

float ComputeRuneStyleErosion(float3 dir, float baseHeight, out float outMask, out float outPattern)
{
    outMask = 0.0;
    outPattern = 0.0;
     
    if (ErosionEnabled <= 0.5 || ErosionStrength <= 0.0001)
        return 0.0;
    
    // Distance fade — skip entirely beyond max distance
    float3 worldPos = dir * (planetRadius + baseHeight);
    float distToCam = length(worldPos - camHiPS);
    float distFade = saturate((ErosionMaxDistance - distToCam) / max(ErosionMaxDistance - ErosionFadeStart, 1.0));
    
    if (distFade <= 0.001f)
        return 0.0f;

    // ---------------------------------------------------------------------
    // Build local 2D position and initial wall-enhanced slope.
    // ---------------------------------------------------------------------

    float3 axisA, axisB;
    HeightGrad2D hg = ComputeWallHeightGrad2D(dir, axisA, axisB);

    float3 p3 = dir * (planetRadius + baseHeight);

    float2 p = float2(dot(p3, axisA), dot(p3, axisB));

    float2 grad = hg.grad;

    float slopeLength = max(length(grad), 1e-10);

    // ---------------------------------------------------------------------
    // Rune-style controls.
    // Move these to cbuffer later if you like.
    // ---------------------------------------------------------------------
    float ridgeRounding = 0.05;
    float creaseRounding = 0.00;

    // x/y equivalent from Rune's rounding.z / rounding.w
    float roundingInputScale = 0.05;
    float roundingOctaveMultiplier = ErosionLacunarity;

    // Equivalent to Rune's onset.x and onset.y
    float onsetInitial = 1.25;
    float onsetOctave = 1.25;

    // Use your existing slope mask as an outer terrain mask.
    float terrainSlopeMask = SlopeToErosionMask(slopeLength);
    outMask = terrainSlopeMask;

    if (terrainSlopeMask <= 0.001)
    {
        outPattern = 0.5;
        return 0.0;
    }

    float fadeTarget = ComputeErosionFadeTarget(dir, hg.h);

    float2 gullySlope = lerp(grad, SafeNormalize2(grad) * ErosionAssumedSlope, ErosionAssumedSlopeBlend);

    float totalHeightDelta = 0.0;

    float freq = 1.0 / max(ErosionTilingMeters * ErosionCellScale, 1.0);
    float octaveStrength = ErosionStrength;

    float roundingMult = 1.0;

    // Initial combi mask, Rune-style.
    float roundingForInput = lerp(creaseRounding, ridgeRounding, saturate(fadeTarget + 0.5)) * roundingInputScale;

    float combiMask = EaseOutSlope(SmoothStart(slopeLength * onsetInitial, roundingForInput * onsetInitial));

    combiMask *= terrainSlopeMask;

[loop]
    for (int octave = 0; octave < TERRAIN_EROSION_OCTAVES; octave++)
    {
        float active = step((float) octave + 0.5, (float) ErosionOctaves);

        float2 gullyDir = SafeNormalize2(gullySlope);

        PhacelleSample phacelle = PhacelleNoise(p * freq, gullyDir, ErosionCellScale, 0.25, ErosionNormalization);

        // Since PhacelleNoise receives p * freq, scale derivative back.
        // Negative because slope directions point down, following Rune.
        float2 phacelleDerivDir = phacelle.sideDir * -freq;

        float sloping = abs(phacelle.s);

        // Rune's non-masked gully direction feedback.
        gullySlope += sign(phacelle.s) * phacelleDerivDir * octaveStrength * ErosionGullyWeight * active;

        // Height + slope delta from gullies.
        float3 gullies = float3(phacelle.c, phacelle.s * phacelleDerivDir);

        // Rune-style fade toward ridge/valley target.
        float3 fadedGullies = lerp(float3(fadeTarget, 0.0, 0.0), gullies * ErosionGullyWeight, combiMask);

        totalHeightDelta += fadedGullies.x * octaveStrength * active;

        // Keep slope update too. This is important.
        grad += fadedGullies.yz * octaveStrength * active;

        // Update fade target.
        fadeTarget = fadedGullies.x;

        // Rune-style octave mask.
        float roundingForOctave = lerp(creaseRounding, ridgeRounding, saturate(phacelle.c + 0.5)) * roundingMult;

        float newMask = EaseOutSlope(SmoothStart(sloping * onsetOctave, roundingForOctave * onsetOctave));

        combiMask = PowInv(combiMask, ErosionDetail) * newMask * active;

        octaveStrength *= saturate(ErosionPersistence);
        freq *= max(ErosionLacunarity, 1.001);
        roundingMult *= roundingOctaveMultiplier;
    }

    outPattern = totalHeightDelta / max(ErosionStrength, 1.0);

    return totalHeightDelta * terrainSlopeMask;
}

float SampleTerrainHeight(float3 dir, float3 worldPos, int currentLOD, uint matCount, float slope, float colorAvg, float3 normalPS, out float wallDebug, out float wallMaskDebug, out float erosionMaskDebug, out float erosionPatternDebug, out float erosionDeltaDebug)
{
    // Base heightmap.
    float baseHeight = SampleHeightMetres(dir);

    // Wall correction: broad ramp -> steeper wall.
    float wallMask = 0.0;
    float wallBoost = ComputeWallSteepenBoost(dir, baseHeight, wallMask);
    baseHeight += wallBoost;

    wallDebug = saturate(abs(wallBoost) / max(WallMaxDelta, 1.0));
    wallMaskDebug = wallMask;

    // Slope erosion after wall shaping.
    float erosionMask = 0.0;
    float erosionPattern = 0.0;

    float erosionDelta = ComputeRuneStyleErosion(dir, baseHeight, erosionMask, erosionPattern);

    baseHeight += erosionDelta;

    erosionMaskDebug = erosionMask;
    erosionPatternDebug = saturate(erosionPattern * 0.5 + 0.5);
    erosionDeltaDebug = saturate(abs(erosionDelta) / max(ErosionStrength, 1.0));
    
    // Material-weighted noise displacement.
    float weights[MAX_MATERIALS];
    ComputeAllMaterialWeights(slope, colorAvg, matCount, weights);

    float totalNoise = 0.0;

    for (uint m = 0; m < matCount; m++)
    {
        if (weights[m] < 0.001)
            continue;

        MaterialData mat = Materials[m];

        float matNoise = 0.0;

        for (int n = 0; n < mat.NoiseLayerCount; n++)
        {
            NoiseLayerData layer = NoiseLayers[mat.NoiseLayerStart + n];

            float vertexDist = length(worldPos - camHiPS);
            float activationDist = planetRadius / (float) (1 << layer.LODActivation);
            float fadeFactor = saturate(1.0 - (vertexDist - activationDist * 0.8) / (activationDist * 0.2));

            float3 noisePos = worldPos;

            if (abs(layer.RadialFrequencyScale - 1.0) > 0.001)
            {
                float radialComponent = dot(worldPos, dir);
                noisePos -= dir * radialComponent * (1.0 - layer.RadialFrequencyScale);
            }

            float h = 0.0;

            if (layer.Type == 0)
                h = FractalPerlin3D(layer.PermBase, noisePos, layer.Octaves, layer.Frequency, layer.Amplitude, layer.Lacunarity, layer.Persistence);
            else if (layer.Type == 1)
                h = RidgedPerlin3D(layer.PermBase, noisePos, layer.Octaves, layer.Frequency, layer.Amplitude, layer.Lacunarity, layer.Persistence, layer.RidgeSharpness);
            else if (layer.Type == 2)
                h = TurbulencePerlin3D(layer.PermBase, noisePos, layer.Octaves, layer.Frequency, layer.Amplitude, layer.Lacunarity, layer.Persistence);
            else if (layer.Type == 3)
                h = Voronoi3D(layer.PermBase, noisePos, layer.Octaves, layer.Frequency, layer.Amplitude, layer.Lacunarity, layer.Persistence);

            matNoise += h * layer.BlendWeight * fadeFactor;
        }

        totalNoise += matNoise * weights[m];
    }

    return baseHeight + totalNoise;
}

// ── compute terrain normal via finite differences ────────────────
float3 ComputeBaseNormalPS(float3 dir)
{
    // Tangent frame on the unit sphere
    float3 up = (abs(dir.y) < 0.999) ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 tanU = normalize(cross(up, dir));
    float3 tanV = cross(dir, tanU);

    // Angular step: one texel of the cubemap face
    uint width, height, layers;
    HeightCubeArray.GetDimensions(width, height, layers);
    float angularStep = (2.0 / (float) width) * (3.14159265 * 0.5);

    float3 dirU = normalize(dir + tanU * angularStep);
    float3 dirV = normalize(dir + tanV * angularStep);

    float hC = SampleHeightMetres(dir);
    float hU = SampleHeightMetres(dirU);
    float hV = SampleHeightMetres(dirV);

    // Displaced positions on the surface
    float3 pC = dir * (planetRadius + hC);
    float3 pU = dirU * (planetRadius + hU);
    float3 pV = dirV * (planetRadius + hV);

    float3 N = normalize(cross(pU - pC, pV - pC));

    // Ensure outward-facing
    if (dot(N, dir) < 0.0)
        N = -N;

    return N;
}
