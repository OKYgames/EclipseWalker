#define MAX_LIGHTS 16

struct Light
{
    float3 Strength;      // Light intensity
    float FalloffStart;   // Point/Spot attenuation start
    float3 Direction;     // Directional/Spot light direction
    float FalloffEnd;     // Point/Spot attenuation end
    float3 Position;      // Point/Spot position
    float SpotPower;      // Spot exponent
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float Metallic;
    int IsToon;
};

static const float PI = 3.14159265f;

// Fresnel approximation
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));
    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);
    return reflectPercent;
}

// Blinn-Phong lighting
float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    const float m = mat.Roughness * 256.0f;
    float3 halfVec = normalize(toEye + lightVec);

    float roughnessFactor = (m + 8.0f) * pow(max(dot(normal, halfVec), 0.0f), m) / 8.0f;
    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

    float3 specAlbedo = fresnelFactor * roughnessFactor;

    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

float3 FresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0f - f0) * pow(1.0f - saturate(cosTheta), 5.0f);
}

float DistributionGGX(float3 normal, float3 halfVec, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float ndoth = saturate(dot(normal, halfVec));
    float ndoth2 = ndoth * ndoth;
    float denom = (ndoth2 * (alpha2 - 1.0f)) + 1.0f;
    return alpha2 / max(PI * denom * denom, 0.0001f);
}

float GeometrySchlickGGX(float ndotv, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return ndotv / max(ndotv * (1.0f - k) + k, 0.0001f);
}

float GeometrySmith(float3 normal, float3 viewDir, float3 lightVec, float roughness)
{
    float ndotv = saturate(dot(normal, viewDir));
    float ndotl = saturate(dot(normal, lightVec));
    return GeometrySchlickGGX(ndotv, roughness) * GeometrySchlickGGX(ndotl, roughness);
}

float3 CookTorrancePbr(float3 radiance, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    float ndotl = saturate(dot(normal, lightVec));
    float lightMask = step(0.0001f, ndotl);
    ndotl = max(ndotl, 0.0001f);

    float3 viewDir = normalize(toEye);
    float3 halfVec = normalize(viewDir + lightVec);
    float roughness = clamp(mat.Roughness, 0.045f, 1.0f);
    float metallic = saturate(mat.Metallic);

    float3 f0 = mat.FresnelR0;
    float3 F = FresnelSchlick(saturate(dot(halfVec, viewDir)), f0);
    float D = DistributionGGX(normal, halfVec, roughness);
    float G = GeometrySmith(normal, viewDir, lightVec, roughness);

    float ndotv = max(saturate(dot(normal, viewDir)), 0.0001f);
    float3 specular = (D * G * F) / max(4.0f * ndotv * ndotl, 0.0001f);
    specular = min(specular, float3(6.0f, 6.0f, 6.0f));

    float3 kS = F;
    float3 kD = (1.0f - kS) * (1.0f - metallic);
    float3 diffuse = kD * mat.DiffuseAlbedo.rgb;

    return (diffuse + specular) * radiance * ndotl * lightMask;
}

// Directional light
float3 ComputeDirectionalLight(Light L, Material M, float3 normal, float3 toEye)
{
    float3 lightVec = -L.Direction;

    // Toon-lit characters
    if (M.IsToon > 0)
    {
        float3 lightStrength = float3(0.0f, 0.0f, 0.0f);
        float3 specStrength = float3(0.0f, 0.0f, 0.0f);
        float3 rimColor = float3(0.0f, 0.0f, 0.0f);

        // Diffuse in three bands
        float rawNdotL = dot(lightVec, normal);
        float toonDiffuse = 0.2f;

        if (rawNdotL > 0.5f)       toonDiffuse = 1.0f;
        else if (rawNdotL > 0.1f)  toonDiffuse = 0.6f;
        
        lightStrength = L.Strength * toonDiffuse;

        // Hard specular highlight
        float3 r = reflect(-lightVec, normal);
        float specFactor = pow(max(dot(r, toEye), 0.0f), M.Roughness);
        float toonSpec = (specFactor > 0.1f) ? 0.5f : 0.0f;
        
        specStrength = L.Strength * M.FresnelR0 * toonSpec;

        // Rim light
        float rimFactor = 1.0f - max(dot(normal, toEye), 0.0f);
        if (rimFactor > 0.7f)
        {
            rimColor = float3(1.0f, 1.0f, 1.0f) * 0.5f;
        }

        return (M.DiffuseAlbedo.rgb * lightStrength) + specStrength + rimColor;
    }

    return CookTorrancePbr(L.Strength, lightVec, normal, toEye, M);
}

// Point light
float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    if (length(L.Strength) <= 0.0f) return float3(0.0f, 0.0f, 0.0f);

    float3 lightVec = L.Position - pos;
    float d = length(lightVec);

    if (d > L.FalloffEnd) return float3(0.0f, 0.0f, 0.0f);

    d = max(d, 0.01f);

    lightVec /= d;
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    float att = saturate((L.FalloffEnd - d) / (L.FalloffEnd - L.FalloffStart));
    float attenuation = att * att;
    lightStrength *= attenuation;

    if (mat.IsToon <= 0)
    {
        return CookTorrancePbr(L.Strength * attenuation, lightVec, normal, toEye, mat);
    }

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

// Spot light
float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);

    if (d > L.FalloffEnd) return 0.0f;

    lightVec /= d;
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    float att = saturate((L.FalloffEnd - d) / (L.FalloffEnd - L.FalloffStart));
    float attenuation = att * att;
    lightStrength *= attenuation;

    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
    lightStrength *= spotFactor;

    if (mat.IsToon <= 0)
    {
        return CookTorrancePbr(L.Strength * attenuation * spotFactor, lightVec, normal, toEye, mat);
    }

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}
