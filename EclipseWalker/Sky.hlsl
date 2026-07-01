#include "Light.hlsl"

TextureCube gCubeMap : register(t0);
SamplerState gsamAnisotropic : register(s4);

cbuffer cbPass : register(b1)
{
    float4x4 gView;      
    float4x4 gInvView;   
    float4x4 gProj;      
    float4x4 gInvProj;
    float4x4 gViewProj;  
    float4x4 gInvViewProj;
    float4x4 gShadowTransform;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;
    Light gLights[MAX_LIGHTS];
    float3 gDomainCenter;
    float gDomainRadius;
    int gIsDomainActive;
    float3 gDomainPad;
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 gFogPad;
    float4 gSkyTint;
    float gHeightFogTop;
    float gHeightFogRange;
    float gHeightFogStrength;
    float gHeightFogPad;
    float4 gSkyEclipseDirection;
    float4 gSkyEclipseParams;
};


cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosL = vin.PosL;

    float3 fixedScalePos = vin.PosL * 5000.0f; 
    float4 posW = float4(fixedScalePos, 1.0f);

    vout.PosH = mul(posW, gViewProj);

    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float3 dir = normalize(pin.PosL);
    float4 texColor = gCubeMap.Sample(gsamAnisotropic, pin.PosL) * gSkyTint;

    // 아래쪽 하늘은 심연 안개색으로 강하게 눌러서 허공이 직접 보이지 않게 한다.
    float lowerHemisphere = saturate((-dir.y - 0.01f) / 0.12f);
    float horizonBlend = smoothstep(0.0f, 1.0f, lowerHemisphere);
    float3 abyssFogColor = lerp(gFogColor.rgb, gFogColor.rgb * 0.35f, 0.7f);
    float3 finalColor = lerp(texColor.rgb, abyssFogColor, horizonBlend);

    if (gSkyEclipseDirection.w > 0.5f)
    {
        float progress = saturate(gSkyEclipseParams.x);
        finalColor *= lerp(1.0f, 0.46f, progress * progress);

        float3 eclipseDir = normalize(gSkyEclipseDirection.xyz);
        float facing = dot(dir, eclipseDir);
        if (facing > 0.001f)
        {
            float3 basisUp = abs(eclipseDir.y) > 0.95f ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
            float3 basisRight = normalize(cross(basisUp, eclipseDir));
            float3 basisY = normalize(cross(eclipseDir, basisRight));
            float2 plane = float2(dot(dir, basisRight), dot(dir, basisY)) / facing;

            float halfHeight = max(gSkyEclipseParams.y, 0.001f);
            float aspect = max(gSkyEclipseParams.z, 0.001f);
            float2 local = float2(
                plane.x / (halfHeight * aspect),
                plane.y / halfHeight);

            float r = length(local);
            float sunDisk = 1.0f - smoothstep(0.97f, 1.02f, r);
            float innerHalo = 1.0f - smoothstep(0.72f, 1.25f, r);
            float corona = pow(saturate(1.0f - r / 3.1f), 2.2f);
            float outerCorona = pow(saturate(1.0f - r / 5.6f), 5.0f);

            float3 sunColor =
                float3(1.0f, 0.82f, 0.34f) * (sunDisk * 1.15f + innerHalo * 0.35f) +
                float3(1.0f, 0.48f, 0.12f) * corona * 1.6f +
                float3(0.95f, 0.88f, 0.62f) * outerCorona * 0.8f;
            float sunBlend = saturate(sunDisk + innerHalo * 0.3f + corona * 0.55f + outerCorona * 0.25f);
            finalColor = lerp(finalColor, sunColor, sunBlend);

            float coverT = smoothstep(0.0f, 1.0f, progress);
            float2 moonCenter = float2(lerp(-2.25f, 0.0f, coverT), 0.0f);
            float moonRadius = lerp(0.96f, 1.04f, coverT);
            float moonR = length(local - moonCenter);
            float moonDisk = 1.0f - smoothstep(moonRadius, moonRadius + 0.035f, moonR);
            float eclipseArea = saturate(sunDisk + innerHalo * 0.65f + corona * 0.12f);
            float3 moonColor = lerp(float3(0.035f, 0.018f, 0.012f), float3(0.0f, 0.0f, 0.0f), coverT);
            finalColor = lerp(finalColor, moonColor, moonDisk * eclipseArea);

            float totality = smoothstep(0.86f, 1.0f, progress);
            float rim = smoothstep(0.86f, 1.02f, r) * (1.0f - smoothstep(1.02f, 1.34f, r));
            finalColor += float3(0.95f, 0.90f, 0.72f) * rim * totality * 0.45f;
        }
    }

    return float4(finalColor, 1.0f);
}
