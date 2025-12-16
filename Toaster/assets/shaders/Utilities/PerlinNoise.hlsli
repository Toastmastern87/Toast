float Fade(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float LerpF(float a, float b, float t)
{
    return a + t * (b - a);
}

float Grad(int hash, float x, float y)
{
    int h = hash & 3;
    float u = (h < 2) ? x : y;
    float v = (h < 2) ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

int PermAt(int i)
{
    int4 v = Perm[i >> 2];
    return v[i & 3];
}

// 2D Perlin in [-1, 1]
float Perlin2D(float2 p)
{
    float x = p.x;
    float y = p.y;

    int xi = (int) floor(x) & 255;
    int yi = (int) floor(y) & 255;

    float xf = x - floor(x);
    float yf = y - floor(y);

    float u = Fade(xf);
    float v = Fade(yf);
    
    int xi1 = (xi + 1) & 255;
    int yi1 = (yi + 1) & 255;

    int aa = PermAt((PermAt(xi) + yi) & 255);
    int ab = PermAt((PermAt(xi) + yi1) & 255);
    int ba = PermAt((PermAt(xi1) + yi) & 255);
    int bb = PermAt((PermAt(xi1) + yi1) & 255);

    float x1 = LerpF(
        Grad(aa, xf, yf),
        Grad(ba, xf - 1, yf),
        u
    );
    float x2 = LerpF(
        Grad(ab, xf, yf - 1),
        Grad(bb, xf - 1, yf - 1),
        u
    );

    return LerpF(x1, x2, v);
}

float FractalPerlin2D(float2 p, int octaves, float baseFreq, float baseAmp)
{
    float result = 0.0f;
    float frequency = baseFreq;
    float amplitude = baseAmp;

    [loop]
    for (int i = 0; i < octaves; i++)
    {
        result += Perlin2D(p * frequency) * amplitude;

        frequency *= 2.0f;
        amplitude *= 0.5f;
    }

    return result;
}