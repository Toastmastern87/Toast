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

float FractalPerlin3D(int permBase, float3 p, int octaves, float baseFreq, float baseAmp)
{
    float result = 0.0f;
    float frequency = baseFreq;
    float amplitude = baseAmp;

    [loop]
    for (int i = 0; i < octaves; i++)
    {
        result += Perlin3D(permBase, p * frequency) * amplitude;
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }
    return result;
}