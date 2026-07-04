float EwLuminance(float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 EwExtractBloom(float3 color)
{
    float brightness = EwLuminance(color);
    float bloomMask = smoothstep(0.72f, 1.05f, brightness);
    return color * bloomMask;
}

float3 EwApplyFilmGrade(float3 color, float2 uv, float strength)
{
    float3 originalColor = color;
    color = max(color, 0.0f);

    float luma = EwLuminance(color);
    color = lerp(luma.xxx, color, 1.12f);
    color = (color - 0.5f) * 1.08f + 0.5f;
    color = pow(saturate(color), float3(0.94f, 0.94f, 0.94f));

    float2 centeredUv = uv * 2.0f - 1.0f;
    float vignette = 1.0f - smoothstep(0.55f, 1.42f, dot(centeredUv, centeredUv)) * 0.22f;
    color *= vignette;

    return lerp(originalColor, max(color, 0.0f), saturate(strength));
}
