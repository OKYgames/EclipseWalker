#define MAX_LIGHTS 16

struct Light
{
    float3 Strength;      // 빛의 색상 및 세기
    float FalloffStart;   // (Point/Spot) 감쇠 시작 거리
    float3 Direction;     // (Directional/Spot) 빛의 방향
    float FalloffEnd;     // (Point/Spot) 감쇠 끝 거리
    float3 Position;      // (Point/Spot) 광원 위치
    float SpotPower;      // (Spot) 스포트라이트 집광도
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
};

// 프레넬 효과 (빛이 비스듬히 닿을수록 반사율 증가)
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));
    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);
    return reflectPercent;
}

// 블린-퐁 반사 모델
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

// 1. 방향성 조명 (태양) 계산
float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
    float3 lightVec = -L.Direction;
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

// 2. 점 조명 계산
float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 result = 0.0f;

    if (length(L.Strength) <= 0.0f) return result;

    float3 lightVec = L.Position - pos;
    float d = length(lightVec);

    if (d > L.FalloffEnd) return result;

    d = max(d, 0.01f);

    lightVec /= d;
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    float att = saturate((L.FalloffEnd - d) / (L.FalloffEnd - L.FalloffStart));
    lightStrength *= att * att;

    result = BlinnPhong(lightStrength, lightVec, normal, toEye, mat);

    return result;
}

// 3. 스포트 라이트 계산
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