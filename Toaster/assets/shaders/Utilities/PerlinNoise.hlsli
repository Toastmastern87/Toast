float Fade(float t)
{
    return t * t * t * (t * (t * 6 - 15) + 10);
}
float LerpF(float a, float b, float t)
{
    return a + t * (b - a);
}

int PermAt(int permBase, int i)
{
    int4 v = PermTables[permBase + (i >> 2)];
    return v[i & 3];
}

int Hash3(int permBase, int x, int y, int z)
{
    // classic Perlin hashing: perm[x + perm[y + perm[z]]]
    int a = PermAt(permBase, x);
    int b = PermAt(permBase, (a + y) & 255);
    return PermAt(permBase, (b + z) & 255);
}

float Grad3(int hash, float x, float y, float z)
{
    // 12 (or 16) gradient directions, no trig
    int h = hash & 15;
    float u = (h < 8) ? x : y;
    float v = (h < 4) ? y : ((h == 12 || h == 14) ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

// 3D Perlin in ~[-1,1]
float Perlin3D(int permBase, float3 p)
{
    float x = p.x, y = p.y, z = p.z;

    int xi = (int) floor(x) & 255;
    int yi = (int) floor(y) & 255;
    int zi = (int) floor(z) & 255;

    float xf = x - floor(x);
    float yf = y - floor(y);
    float zf = z - floor(z);

    float u = Fade(xf);
    float v = Fade(yf);
    float w = Fade(zf);

    int xi1 = (xi + 1) & 255;
    int yi1 = (yi + 1) & 255;
    int zi1 = (zi + 1) & 255;

    int aaa = Hash3(permBase, xi, yi, zi);
    int aba = Hash3(permBase, xi, yi1, zi);
    int aab = Hash3(permBase, xi, yi, zi1);
    int abb = Hash3(permBase, xi, yi1, zi1);

    int baa = Hash3(permBase, xi1, yi, zi);
    int bba = Hash3(permBase, xi1, yi1, zi);
    int bab = Hash3(permBase, xi1, yi, zi1);
    int bbb = Hash3(permBase, xi1, yi1, zi1);

    float x00 = LerpF(Grad3(aaa, xf, yf, zf), Grad3(baa, xf - 1, yf, zf), u);
    float x10 = LerpF(Grad3(aba, xf, yf - 1, zf), Grad3(bba, xf - 1, yf - 1, zf), u);
    float x01 = LerpF(Grad3(aab, xf, yf, zf - 1), Grad3(bab, xf - 1, yf, zf - 1), u);
    float x11 = LerpF(Grad3(abb, xf, yf - 1, zf - 1), Grad3(bbb, xf - 1, yf - 1, zf - 1), u);

    float y0 = LerpF(x00, x10, v);
    float y1 = LerpF(x01, x11, v);

    return LerpF(y0, y1, w);
}

float FractalPerlin3D(int permBase, float3 p, int octaves, float baseFreq, float baseAmp, float lacunarity, float persistence)
{
    float result = 0.0f;
    float frequency = baseFreq;
    float amplitude = baseAmp;

    [loop]
    for (int i = 0; i < octaves; i++)
    {
        result += Perlin3D(permBase, p * frequency) * amplitude;
        frequency *= lacunarity;
        amplitude *= persistence;
    }
    return result;
}

float RidgedPerlin3D(int permBase, float3 p, int octaves, float baseFreq, float baseAmp, float lacunarity, float persistence,
                     float sharpness)
{
    float result = 0.0f;
    float frequency = baseFreq;
    float amplitude = baseAmp;

    [loop]
    for (int i = 0; i < octaves; i++)
    {
        float n = Perlin3D(permBase, p * frequency);
        n = 1.0f - abs(n);
        n = pow(n, sharpness);
        result += n * amplitude;
        frequency *= lacunarity;
        amplitude *= persistence;
    }
    return result;
}

float TurbulencePerlin3D(int permBase, float3 p, int octaves, float baseFreq, float baseAmp, float lacunarity, float persistence)
{
    float result = 0.0f;
    float frequency = baseFreq;
    float amplitude = baseAmp;

    [loop]
    for (int i = 0; i < octaves; i++)
    {
        result += abs(Perlin3D(permBase, p * frequency)) * amplitude;
        frequency *= lacunarity;
        amplitude *= persistence;
    }
    return result;
}

float Voronoi3D(int permBase, float3 p, int octaves, float baseFreq, float baseAmp, float lacunarity, float persistence)
{
    float result = 0.0f;
    float frequency = baseFreq;
    float amplitude = baseAmp;

    [loop]
    for (int i = 0; i < octaves; i++)
    {
        float3 scaledP = p * frequency;
        float3 cellI = floor(scaledP);
        float3 cellF = scaledP - cellI;

        float F1 = 8.0;
        float F2 = 8.0;

        [unroll]
        for (int dz = -1; dz <= 1; dz++)
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                {
                    float3 neighbor = float3(dx, dy, dz);
                    int hashI = Hash3(permBase,
                (int) (cellI.x + dx) & 255,
                (int) (cellI.y + dy) & 255,
                (int) (cellI.z + dz) & 255);
                    float3 rand = float3(
                frac(hashI * 0.1031),
                frac(hashI * 0.1030),
                frac(hashI * 0.0973)
            );
                    float3 diff = neighbor + rand - cellF;
                    float d = dot(diff, diff);

                    if (d < F1)
                    {
                        F2 = F1;
                        F1 = d;
                    }
                    else if (d < F2)
                    {
                        F2 = d;
                    }
                }

        float edge = sqrt(F2) - sqrt(F1);
        result += edge * amplitude;

        frequency *= lacunarity;
        amplitude *= persistence;
    }
    return result;
}

struct PhacelleSample
{
    float c; // cosine / height component
    float s; // sine / derivative phase component
    float2 sideDir; // derivative direction in input coordinate space
};

// Stable replacement for the sin-based Hash22.
// Takes a 2D grid point (integer coordinates after floor()) and produces
// a deterministic [0, 1) random pair. Numerically stable at any scale.
uint HashU(uint x, uint y)
{
    // Wang hash style mixing
    uint h = x * 0x27d4eb2du;
    h ^= h >> 15;
    h *= 0x85ebca6bu;
    h ^= y;
    h *= 0x27d4eb2du;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

float2 Hash22Stable(float2 p)
{
    // Convert to integer grid coordinates (already floored from caller).
    // We add a large bias so negative inputs don't underflow uint cast.
    int2 ip = int2(floor(p) + 100000.0);
    uint x = (uint) ip.x;
    uint y = (uint) ip.y;
    
    uint h1 = HashU(x, y);
    uint h2 = HashU(x ^ 0x7ed55d16u, y ^ 0xc761c23cu);
    
    // Convert hash to [0, 1) by taking the lower 24 bits / 2^24
    return float2(
        (float) (h1 & 0xFFFFFFu) / 16777216.0f,
        (float) (h2 & 0xFFFFFFu) / 16777216.0f
    );
}

PhacelleSample PhacelleNoise(float2 p, float2 normDir, float cellScale, float offset, float normalization)
{
    PhacelleSample result;

    const float TAU = 6.28318530718;

    // Orthogonal direction to stripe direction.
    // Magnitude controls stripe frequency inside each cell.
    float2 sideDir = normDir.yx * float2(-1.0, 1.0) * cellScale * TAU;
    offset *= TAU;

    float2 pInt = floor(p);
    float2 pFrac = frac(p);

    float2 phaseDir = float2(0.0, 0.0);
    float weightSum = 0.0;

    [loop]
    for (int i = -1; i <= 2; i++)
    {
        [loop]
        for (int j = -1; j <= 2; j++)
        {
            float2 gridOffset = float2(i, j);
            float2 gridPoint = pInt + gridOffset;

            // Your Hash22 returns [0,1], convert to [-0.5,0.5].
            float2 randomOffset = Hash22Stable(gridPoint) - 0.5;

            float2 vectorFromCellPoint = pFrac - gridOffset - randomOffset;

            float sqrDist = dot(vectorFromCellPoint, vectorFromCellPoint);

            float weight = exp(-sqrDist * 2.0);
            weight = max(0.0, weight - 0.01111);

            weightSum += weight;

            float waveInput = dot(vectorFromCellPoint, sideDir) + offset;

            phaseDir += float2(cos(waveInput), sin(waveInput)) * weight;
        }
    }

    float2 interpolated = phaseDir / max(weightSum, 0.0001);

    float magnitude = sqrt(dot(interpolated, interpolated));

    // Partial normalization.
    magnitude = max(1.0 - normalization, magnitude);

    result.c = interpolated.x / magnitude;
    result.s = interpolated.y / magnitude;
    result.sideDir = sideDir;

    return result;
}