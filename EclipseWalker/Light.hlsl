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
    int IsToon;
};

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

// Directional light
float3 ComputeDirectionalLight(Light L, Material M, float3 normal, float3 toEye)
{
    float3 lightVec = -L.Direction;
    float3 lightStrength = float3(0.0f, 0.0f, 0.0f);
    float3 specStrength = float3(0.0f, 0.0f, 0.0f);
    float3 rimColor = float3(0.0f, 0.0f, 0.0f); 

    // Toon-lit characters
    if (M.IsToon > 0)
    {
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
    }
    // Regular lit geometry
    else 
    {
        // Standard diffuse
        float ndotl = max(dot(lightVec, normal), 0.0f);
        lightStrength = L.Strength * ndotl;

        // Standard specular
        float3 r = reflect(-lightVec, normal);
        float specFactor = pow(max(dot(r, toEye), 0.0f), M.Roughness);
        specStrength = L.Strength * specFactor * M.FresnelR0;       
    }

    return (M.DiffuseAlbedo.rgb * lightStrength) + specStrength + rimColor;
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
    lightStrength *= att * att;

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
    lightStrength *= att * att;

    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
    lightStrength *= spotFactor;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}
