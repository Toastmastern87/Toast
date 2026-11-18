#ifndef LOCAL_EXPOSURE_HLSLI
#define LOCAL_EXPOSURE_HLSLI

static const float LE_LOG_MIN = -12.0f;
static const float LE_LOG_MAX = 12.0f;
static const uint LE_BINS = 32;

// Position along the brightness axis, 0..1 (the grid's z in uvw).
float LocalExposureBin01(float logLum)
{
    return saturate((logLum - LE_LOG_MIN) / (LE_LOG_MAX - LE_LOG_MIN));
}

#endif